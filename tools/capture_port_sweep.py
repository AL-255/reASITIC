"""Capture repeated unmodified port sweeps, with read-only writer frequencies."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/port-sweep-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases']
    base=ROOT/'build/original-port-sweep';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(item):
        case=item['case'];family=item['family'];tech=re.sub(r'(?mi)^\s*eddy\s*=.*$',lambda _:'\n'.join('eddy = '+str(i) for i in case['layers']),text)
        for key,v in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(v),tech)
        l,w,t=case['cells'];cl,cw=case['contact_cells'];script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY '+('TRUE' if case['eddy_mode'] else 'FALSE')+'\n'
        creators=[case['primary'],case['secondary']] if family=='transformer' else [case['device'],*case['pads']] if family=='pad' else [{'id':case['id'],'keywords':case['keywords']}]
        for c in creators:
            if c:script+=COMMANDS[c['id']]+' '+c['keywords']+'\n'
        g=case.get('ground','');ground={'@self':'A','@1':'A' if family=='transformer' else 'P1','@2':'B' if family=='transformer' else 'P2'}.get(g,'G' if g else 'A')
        if g and not g.startswith('@'):script+='WIRE '+g+'\n'
        first,last,step=item['range_ghz'];model='TRANSPI' if family=='transformer' else 'PI'
        modes=[('s','S','RECT'),('s-polar','S','POLAR'),('y','Y','RECT'),('model',model,'RECT')]
        for key,parameter,form in modes:
            values=f'{first} {last} {step} {parameter} {form}'
            if family=='transformer':command=f'2PORTTRANS A B {values} {key}.out'+(' '+ground+' '+str(case['termination']) if g else '')
            elif family=='pad':command=f'2PORTPAD A P1 P2 {values} {key}.out '+ground
            elif family=='extended':command=f'2PORTX A {values} SLOW {key}.out'+(' '+ground if g else '')
            else:command=f'2PORTGND A {ground} {values} {key}.out'
            script+=command+'\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/item['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json,struct
rows=[]
class Writer(gdb.Breakpoint):
 def stop(self):
  m=gdb.selected_inferior().read_memory;esp=int(gdb.parse_and_eval('$esp'))
  rows.append(dict(frequency_hex=bytes(m(esp+8,8)).hex(),parameter=struct.unpack('<i',m(esp+16,4))[0],polar=bytes(m(esp+20,1))[0]))
  return False
Writer('*0x8088c9c')
end
run -g -t CMOS.tek -x input.asit < exit.txt
python
open('writer.json','w').write(json.dumps(rows))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb);p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=180,start_new_session=True,preexec_fn=limits);(directory/'gdb.log').write_text(p.stdout+'\n'+p.stderr)
            if p.returncode:raise RuntimeError(item['name']+': original failed '+p.stdout+p.stderr)
            inputs=json.loads((directory/'writer.json').read_text());assert len(inputs)>len(modes)
            records={}
            for key,_,_ in modes:
                content=(directory/(key+'.out')).read_text();start=content.rfind('\n# GHZ');assert start>=0
                records[key]=content[start:]+'\n'
            frequencies=[r['frequency_hex'] for r in inputs if r['parameter']==1 and r['polar']==0]
            assert len(inputs)==len(frequencies)*len(modes)
            snapshots.append({'frequency_hex':frequencies,'records':records})
        assert snapshots[0]==snapshots[1],item['name']+' differs between fresh processes'
        print(item['name']+': native frequencies and '+str(len(modes))+' complete numerical sweep files repeat exactly',flush=True)
        return {'name':item['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-port-sweep.json';output.write_text(json.dumps({'reference':'Unmodified original port commands in two fresh processes; read-only GDB writer frequencies; numerical file sections retain original headers, spacing and order','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'cases':captured},indent=2)+'\n');print(output);print('Raw proof:',work)
if __name__=='__main__':main()
