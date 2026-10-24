"""Read-only original INDUCTANCE, RESIS, RESISHF, Q and METALAREA reports.

Reload actual native BSAVE files in fresh processes so the reference creator's
known heap overrun cannot affect later analysis allocations. Never edit saved
records, process memory, instructions or imported functions.
"""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_geometry_edits import physical_layout

FREQUENCIES=['0.001','1','2.25','100']

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re')
    args=parser.parse_args();binary=args.reference/'run/asitic.linux.2.2'
    assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/creator-coverage-cases.json';cases=json.loads(source.read_text())['cases']
    extras=ROOT/'tests/fixtures/conductor-metric-extra-cases.json';cases+=json.loads(extras.read_text())['cases']
    technologies={name:(ROOT/'web/technology'/(name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
    base=ROOT/'build/original-conductor-metrics';base.mkdir(parents=True,exist_ok=True)
    work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu')
    env.pop('LD_PRELOAD',None)
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        snapshots=[];tech=technologies[case['technology']];metals=len(re.findall(r'(?im)^\s*<metal>',tech))
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True)
            (directory/'process.tek').write_text(tech)
            (directory/'create.asit').write_text(case['command']+'\n'+case.get('edits','')+'\nBSAVE raw\n')
            p=subprocess.run([str(binary),'-g','-t','process.tek','-x','create.asit'],cwd=directory,env=env,input=case['answers']+'EXIT\n',capture_output=True,text=True,timeout=30,start_new_session=True,preexec_fn=limits)
            (directory/'creator.log').write_text(p.stdout+p.stderr)
            assert p.returncode in (0,-11),(case['name'],p.returncode)
            creator_log=p.stdout+((directory/'asitic.LOG').read_text() if (directory/'asitic.LOG').exists() else '')
            assert 'Error:' not in creator_log,case['name']+' original rejected the requested geometry'
            saved=list(directory.glob('raw.*'));assert len(saved)==1,case['name']+' missing original BSAVE'
            shapes=physical_layout(saved[0].read_bytes(),metals)
            assert sorted(s['name'] for s in shapes)==sorted(case['names'])
            commands=['BLOAD raw','BSAVE before']
            for name in case['names']:
                commands += ['INDUCTANCE '+name,'RESIS '+name,'METALAREA '+name]
                for frequency in FREQUENCIES:commands += [f'RESISHF {name} {frequency}',f'Q {name} {frequency}']
            commands+=['BSAVE after']
            (directory/'metrics.asit').write_text('\n'.join(commands)+'\n');(directory/'stdin.txt').write_text('EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,struct,json,math
observations=[]
def read(address,n):return bytes(gdb.selected_inferior().read_memory(address,n))
def integer(address):return struct.unpack('<I',read(address,4))[0]
def string(address):return read(address,80).split(b'\\0')[0].decode('ascii')
last_frequency=None
class Frequency(gdb.Breakpoint):
 def stop(self):
  global last_frequency
  last_frequency=struct.unpack('<d',read(int(gdb.parse_and_eval('$esp'))+8,8))[0];return False
class Report(gdb.Breakpoint):
 def stop(self):
  stack=int(gdb.parse_and_eval('$esp'));format=string(integer(stack+8));key=None;offset=16
  if format.startswith('Inductance of '):key='inductance_nh'
  elif format.startswith('Resistance of '):key='resistance_dc'
  elif format.startswith('Total Metal Area of '):key='metal_area_um2'
  elif format.startswith('LF Extrapolated Q of '):key='quality_low_frequency'
  elif format.startswith('%s: at %lf GHz R = '):key='resistance_hf';offset=24
  if key:
   raw=read(stack+offset,8);value=struct.unpack('<d',raw)[0]
   observations.append(dict(name=string(integer(stack+12)),key=key,value=value if math.isfinite(value) else None,hex=raw.hex(),frequency_hz=last_frequency if key in ('resistance_hf','quality_low_frequency') else None))
  return False
Frequency('*0x804d70c',internal=True)
Report('*0x804ae04',internal=True)
end
run -g -t process.tek -x metrics.asit < stdin.txt
python
open('observations.json','w').write(json.dumps(observations,allow_nan=False))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb)
            q=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,capture_output=True,text=True,timeout=60,start_new_session=True,preexec_fn=limits)
            (directory/'gdb.log').write_text(q.stdout+q.stderr)
            if q.returncode or 'SIGSEGV' in q.stdout:raise RuntimeError(case['name']+': '+q.stdout+q.stderr)
            observations=json.loads((directory/'observations.json').read_text())
            assert len(observations)==len(case['names'])*(3+2*len(FREQUENCIES)),(case['name'],observations)
            before=physical_layout((directory/'before.SPI').read_bytes(),metals);after=physical_layout((directory/'after.SPI').read_bytes(),metals)
            assert before==after,case['name']+' analysis mutated geometry'
            assert sorted(before,key=lambda s:s['name'])==sorted(shapes,key=lambda s:s['name'])
            log=(directory/'asitic.LOG').read_text()
            printed=[line.strip() for line in log.splitlines() if re.match(r'(Inductance of |Resistance of |Total Metal Area of |LF Extrapolated Q of |\S+: at \S+ GHz R = )',line)]
            assert len(printed)==len(observations),(case['name'],printed)
            snapshots.append(dict(shapes=before,observations=observations,printed=printed,creator_exit_status=p.returncode))
        assert snapshots[0]==snapshots[1],case['name']+' original metrics vary'
        print(case['name']+': repeated original metric words, reports and unchanged physical geometry captured',flush=True)
        return dict(name=case['name'],technology=case['technology'],metal_count=metals,**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-conductor-metrics.json'
    output.write_text(json.dumps(dict(reference=__doc__,executable_sha256=ORIGINAL_SHA256,case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),extra_case_definition_sha256=hashlib.sha256(extras.read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},frequencies_ghz=FREQUENCIES,cases=captured),indent=2)+'\n')
    print(output);print('Raw proof:',work)

if __name__=='__main__':main()
