"""Read original SELFRES analysis requests, Y matrices and native returns."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/self-resonance-cases.json';technology=ROOT/'web/technology/CMOS.tek';cases=json.loads(source.read_text())['cases']
    base=ROOT/'build/original-self-resonance';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        tech=technology.read_text()
        for key,value in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        l,w,t=case['cells'];cl,cw=case['contact_cells'];c=case['creator'];script=f'EDDY FALSE\nCELL {l} {w} {t}\nCCELL {cl} {cw}\n'+('CAPACITOR' if c['id']==4 else COMMANDS[c['id']])+' '+c['keywords']+'\n'
        if case.get('automatic_cells'):a=case['automatic_cells'];script+=f"AUTOCELL {a['alpha']} {a['beta']}\n"
        first,second=case['bounds_ghz'];script+=f"SELFRES A {first} {second} {case['step_ghz']} "+('D' if case['mode']=='differential' else 'S')+(' FAST ' if case['fast'] else ' SLOW ')+str(case['native_iterations'])+(' '+case['ground'] if case['ground'] else '')+'\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json,struct
samples=[];returns=[];requests=[]
def read(address,n):return bytes(gdb.selected_inferior().read_memory(address,n))
class ModelReturn(gdb.FinishBreakpoint):
 def __init__(self,frequency):super().__init__(gdb.newest_frame(),internal=True);self.frequency=frequency
 def stop(self):
  samples.append(dict(frequency_hex=self.frequency,admittance_hex=(read(0x80d8da8,32)+read(0x80d8dd8,32)).hex()));return False
class Model(gdb.Breakpoint):
 def stop(self):
  esp=int(gdb.parse_and_eval('$esp'));frequency=read(esp+8,8).hex();requests.append(frequency);ModelReturn(frequency);return False
class SearchReturn(gdb.FinishBreakpoint):
 def stop(self):returns.append(struct.pack('<d',float(gdb.parse_and_eval('$st0'))).hex());return False
class Search(gdb.Breakpoint):
 def stop(self):SearchReturn(gdb.newest_frame(),internal=True);return False
Model('*0x8052b60',internal=True);Search('*0x804e590',internal=True)
end
run -g -t CMOS.tek -x input.asit < exit.txt
python
open('search.json','w').write(json.dumps(dict(samples=samples,requests=requests,return_frequency_hex=returns)))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb)
            p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=240,start_new_session=True,preexec_fn=limits);(directory/'gdb.log').write_text(p.stdout+p.stderr)
            if p.returncode:raise RuntimeError(case['name']+': '+p.stdout+p.stderr)
            result=json.loads((directory/'search.json').read_text())
            if case.get('expect_native_fault'):
                assert 'SIGSEGV' in p.stdout and not result['return_frequency_hex'];result['native_fault']='SIGSEGV'
            else:assert result['samples'] and len(result['return_frequency_hex'])==1,(case['name'],p.stdout+p.stderr)
            result['printed_result']=re.findall(r'Self Resonant Frequency Occurs at [^\n]+|Could not find self resonant frequency in given range\.',p.stdout)
            snapshots.append(result)
        assert snapshots[0]==snapshots[1],case['name']+' original search changed between fresh runs'
        print(case['name']+': '+str(len(snapshots[0]['samples']))+' native analyses and return repeat exactly',flush=True);return dict(name=case['name'],**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-self-resonance.json';output.write_text(json.dumps(dict(reference='Unmodified SELFRES twice per case; read-only GDB analysis entry/return and search return observations',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),cases=captured),indent=2)+'\n');print(output);print('Raw proof:',work)

if __name__=='__main__':main()
