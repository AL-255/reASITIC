"""Observe original 2PORTX/2PORTGND model selection, Y and S/Y output files."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_geometry_edits import physical_layout

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/port-dispatch-cases.json';cases=json.loads(source.read_text())['cases']
    technologies={name:(ROOT/'web/technology'/(name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
    base=ROOT/'build/original-port-dispatch';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        text=technologies[case['technology']];metals=len(re.findall(r'(?im)^\s*<metal>',text));snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True)
            for key,value in zip(['fftx','ffty'],case['grid']):text=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),text)
            (directory/'process.tek').write_text(text)
            (directory/'create.asit').write_text(case['command']+'\n'+case['edits']+'\nBSAVE raw\n')
            p=subprocess.run([str(binary),'-g','-t','process.tek','-x','create.asit'],cwd=directory,env=env,input=case['answers']+'EXIT\n',capture_output=True,text=True,timeout=30,start_new_session=True,preexec_fn=limits)
            (directory/'creator.log').write_text(p.stdout+p.stderr)
            log=p.stdout+((directory/'asitic.LOG').read_text() if (directory/'asitic.LOG').exists() else '')
            assert p.returncode in (0,-11) and 'Error:' not in log,(case['name'],p.returncode,log)
            saved=list(directory.glob('raw.*'));assert len(saved)==1,case['name']+' original geometry missing'
            shapes=physical_layout(saved[0].read_bytes(),metals)
            script='EDDY FALSE\nSAVEMAT TRUE\nCELL '+' '.join(map(str,case['cells']))+'\nCCELL '+' '.join(map(str,case['contact_cells']))+'\nBLOAD raw\nBSAVE before\n'
            first,last,step=case['range_ghz'];modes=[('s','S','RECT'),('y','Y','RECT'),('s-polar','S','POLAR'),('y-polar','Y','POLAR')]
            for key,parameter,form in modes:
                command=f"2PORTX {case['winding']} {first} {last} {step} {parameter} {form} SLOW {key}.out" if case['model']=='PIX' else f"2PORTGND {case['winding']} {case['winding']} {first} {last} {step} {parameter} {form} {key}.out"
                if case.get('explicit_ground') and case['model']=='PIX':command+=' '+case['winding']
                script+=command+'\n'
            script+='BSAVE after\n';(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json,struct
events=[];rows=[]
def read(address,n):return bytes(gdb.selected_inferior().read_memory(address,n))
class Returned(gdb.FinishBreakpoint):
 def __init__(self,frame,event):super().__init__(frame,internal=True);self.event=event
 def stop(self):self.event['pi_after']=read(0x80d8820,96).hex();return False
class Driver(gdb.Breakpoint):
 def __init__(self,address,model):super().__init__(address,internal=True);self.model=model
 def stop(self):
  stack=int(gdb.parse_and_eval('$esp'));shape=struct.unpack('<I',read(stack+4,4))[0]
  event=dict(model=self.model,shape_kind=struct.unpack('<i',read(shape+0x50,4))[0],pi_before=read(0x80d8820,96).hex());events.append(event)
  if self.model=='CAP':Returned(gdb.newest_frame(),event)
  return False
class Writer(gdb.Breakpoint):
 def stop(self):
  stack=int(gdb.parse_and_eval('$esp'))
  rows.append(dict(frequency_hex=read(stack+8,8).hex(),parameter=struct.unpack('<i',read(stack+16,4))[0],polar=read(stack+20,1)[0],admittance_hex=(read(0x80d8da8,32)+read(0x80d8dd8,32)).hex(),model=events[-1]['model'],shape_kind=events[-1]['shape_kind']));return False
Driver('*0x8050b2c','PI2');Driver('*0x80527a4','PIX');Driver('*0x80515e4','CAP');Writer('*0x8088c9c',internal=True)
end
run -g -t process.tek -x input.asit < exit.txt
python
open('observations.json','w').write(json.dumps(dict(events=events,rows=rows)))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb)
            q=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,capture_output=True,text=True,timeout=180,start_new_session=True,preexec_fn=limits)
            (directory/'gdb.log').write_text(q.stdout+q.stderr)
            if q.returncode or 'SIGSEGV' in q.stdout:raise RuntimeError(case['name']+': '+q.stdout+q.stderr)
            observation=json.loads((directory/'observations.json').read_text());rows=observation['rows'];events=observation['events']
            assert len(rows)==12 and len(events)==12,(case['name'],observation)
            assert len({r['model'] for r in rows})==len({r['shape_kind'] for r in rows})==1
            if rows[0]['model']=='CAP':assert all(e['pi_before']==e['pi_after'] for e in events),case['name']+' original capacitor changes Pi history'
            records={}
            for key,_,_ in modes:
                content=(directory/(key+'.out')).read_text();start=content.rfind('\n# GHZ');assert start>=0
                records[key]=content[start:]+'\n'
            before=physical_layout((directory/'before.SPI').read_bytes(),metals);after=physical_layout((directory/'after.SPI').read_bytes(),metals)
            assert before==after,case['name']+' original sweep changed physical geometry'
            ordered=lambda values:sorted(values,key=lambda s:s['name']);assert ordered(before)==ordered(shapes)
            log=(directory/'asitic.LOG').read_text();warning='Warning:  User defined structure type may not be compatible with 2port command.'
            snapshots.append(dict(shapes=before,rows=rows,records=records,warning_count=log.count(warning),capacitor_preserves_pi=all(e['pi_before']==e['pi_after'] for e in events) if rows[0]['model']=='CAP' else None))
        assert snapshots[0]==snapshots[1],case['name']+' original results vary'
        print(case['name']+': repeated original dispatch, port words and four complete S/Y files captured',flush=True)
        return dict(name=case['name'],technology=case['technology'],metal_count=metals,**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-port-dispatch.json';output.write_text(json.dumps(dict(reference='Two fresh unmodified native geometry saves and two separate original port-sweep processes per case. Read-only driver/writer breakpoints; no register, numerical memory, binary or saved-record edits.',executable_sha256=ORIGINAL_SHA256,case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},cases=captured),indent=2)+'\n')
    print(output);print('Raw proof:',work)

if __name__=='__main__':main()
