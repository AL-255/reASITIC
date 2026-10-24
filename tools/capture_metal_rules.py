"""Observe original SMIN/WMIN normalization and interactive transition geometry."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_geometry_edits import physical_layout


def technology_with_rules(text,case):
    starts=list(re.finditer(r'(?im)^\s*<(?:metal|via)>\s*(\d+)',text))
    for i,match in enumerate(starts):
        if '<metal>' in match[0].lower() and int(match[1])==case['metal']:
            end=starts[i+1].start() if i+1<len(starts) else len(text)
            return text[:end]+f"\nSMIN = {case['smin']}\nWMIN = {case['wmin']}\n"+text[end:]
    raise ValueError('Missing metal in fixture technology')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/metal-rules-cases.json';technology=ROOT/'web/technology/CMOS.tek';cases=json.loads(source.read_text())['cases']
    base=ROOT/'build/original-metal-rules';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        tech=technology_with_rules(technology.read_text(),case);c=case['creator'];script='SYMSQ'+(' '+c['keywords'] if c['keywords'] else '')+'\nBSAVE raw\n';snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'stdin.txt').write_text(c['answers']+'EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,struct,json
rules=[]
def read(address,n):return bytes(gdb.selected_inferior().read_memory(address,n))
class Validated(gdb.FinishBreakpoint):
 def stop(self):
  head=struct.unpack('<I',read(0x80ce99c,4))[0];result={}
  while head:
   index=struct.unpack('<i',read(head+0xa4,4))[0];result[str(index)]=read(head+0xc0,16).hex();head=struct.unpack('<I',read(head+0xd0,4))[0]
  rules.append(result);return False
class Validate(gdb.Breakpoint):
 def stop(self):Validated(gdb.newest_frame(),internal=True);return False
Validate('*0x807c804',internal=True)
end
run -g -t CMOS.tek -x input.asit < stdin.txt
python
open('rules.json','w').write(json.dumps(rules))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb);p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=60,start_new_session=True,preexec_fn=limits)
            (directory/'gdb.log').write_text(p.stdout+p.stderr)
            if p.returncode or 'SIGSEGV' in p.stdout:raise RuntimeError(case['name']+': '+p.stdout+p.stderr)
            rules=json.loads((directory/'rules.json').read_text());assert len(rules)==1 and len(rules[0])==5,(case['name'],rules)
            files=list(directory.glob('raw.*'));assert len(files)==1,(case['name'],p.stdout)
            log=p.stdout+((directory/'asitic.LOG').read_text() if (directory/'asitic.LOG').exists() else '')
            # GDB stdout and the log may both contain console messages; select
            # the original log when it is the program's output destination.
            if (directory/'asitic.LOG').exists():log=(directory/'asitic.LOG').read_text()
            diagnostics={key:log.count(message) for key,message in [('clearance_retries','Need more space for transitions.'),('width_warnings','Transition metal width less than WMIN specified in technology file'),('turn_retries','Cannot have more than')]}
            snapshots.append(dict(metal_rule_hex=rules[0],shapes=physical_layout(files[0].read_bytes()),diagnostics=diagnostics))
        assert json.dumps(snapshots[0],sort_keys=True,allow_nan=False)==json.dumps(snapshots[1],sort_keys=True,allow_nan=False),case['name']+' original observations vary'
        print(case['name']+': raw normalized metal rules, transition geometry and diagnostics repeat exactly',flush=True)
        return dict(name=case['name'],**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-metal-rules.json';output.write_text(json.dumps(dict(reference='Two fresh original processes per case; read-only technology-validation return observation, defined BSAVE physical fields and console diagnostics',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),cases=captured),indent=2)+'\n');print(output);print('Raw proof:',work)


if __name__=='__main__':main()
