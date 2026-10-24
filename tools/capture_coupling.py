"""Read-only entry/return observations of the original COUPLING command."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_geometry_edits import physical_layout

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/coupling-cases.json';cases=json.loads(source.read_text())['cases'];technologies={name:(ROOT/'web/technology'/(name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
    base=ROOT/'build/original-coupling';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        snapshots=[];tech=technologies[case['technology']];metals=len(re.findall(r'(?im)^\s*<metal>',tech))
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'process.tek').write_text(tech)
            script='\n'.join(c['command'] for c in case['creators'])+'\n'+case.get('edits','')+f"\nBSAVE before\nCOUPLING {case['first']} {case['second']}\nBSAVE after\n";(directory/'input.asit').write_text(script)
            (directory/'stdin.txt').write_text(''.join(c['answers'] for c in case['creators'])+'EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,struct,json
observations=[]
def read(address,n):return bytes(gdb.selected_inferior().read_memory(address,n))
class Returned(gdb.FinishBreakpoint):
 def __init__(self,frame,pointer):super().__init__(frame,internal=True);self.pointer=pointer
 def stop(self):
  coefficient=float(gdb.parse_and_eval('$st0'));mutual=struct.unpack('<d',read(self.pointer,8))[0]
  observations.append(dict(coefficient=coefficient,mutual_nh=mutual,coefficient_hex=struct.pack('<d',coefficient).hex(),mutual_hex=read(self.pointer,8).hex()));return False
class Enter(gdb.Breakpoint):
 def stop(self):
  pointer=struct.unpack('<I',read(int(gdb.parse_and_eval('$esp'))+12,4))[0];Returned(gdb.newest_frame(),pointer);return False
Enter('*0x804d03c',internal=True)
end
run -g -t process.tek -x input.asit < stdin.txt
python
open('observations.json','w').write(json.dumps(observations,allow_nan=False))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb);p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=60,start_new_session=True,preexec_fn=limits)
            (directory/'gdb.log').write_text(p.stdout+p.stderr)
            if p.returncode or 'SIGSEGV' in p.stdout:raise RuntimeError(case['name']+': '+p.stdout+p.stderr)
            observations=json.loads((directory/'observations.json').read_text());assert len(observations)==1,(case['name'],observations)
            before=physical_layout((directory/'before.SPI').read_bytes(),metals);after=physical_layout((directory/'after.SPI').read_bytes(),metals)
            log=(directory/'asitic.LOG').read_text();printed=re.findall(r'k =\s*([-+\d.eE]+) and M =\s*([-+\d.eE]+) \(nH\)',log);assert len(printed)==1,(case['name'],log)
            snapshots.append(dict(shapes=before,after_shapes=after,original_mutated=before!=after,printed=dict(coefficient=printed[0][0],mutual_nh=printed[0][1]),**observations[0]))
        assert json.dumps(snapshots[0],sort_keys=True,allow_nan=False)==json.dumps(snapshots[1],sort_keys=True,allow_nan=False),case['name']+' original output varies'
        print(case['name']+': repeated original coefficient, mutual and geometry captured',flush=True);return dict(name=case['name'],technology=case['technology'],metal_count=metals,**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-coupling.json';output.write_text(json.dumps(dict(reference='Two fresh unmodified original processes per case; read-only COUPLING entry/return observations, original printed report, and physical BSAVE before/after',executable_sha256=ORIGINAL_SHA256,case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},cases=captured),indent=2)+'\n');print(output);print('Raw proof:',work)

if __name__=='__main__':main()
