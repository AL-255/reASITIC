"""Benchmark the identical 20-point task on the ORIGINAL binary FIRST, then WASM.

Both runs compute every point without Green-table reuse, at 256x256 with the
same CMOS coil, 50/1.5/.5 filament mesh, 50/20 contacts and eddy currents off.
The speed gate requires the production-worker median to match or beat the
original median on this machine. It never changes the numerical accuracy gate.
"""
import argparse
import hashlib
import json
import os
import re
import resource
import statistics
import subprocess
import tempfile
import time
from pathlib import Path
from capture_lmat import ROOT, ORIGINAL_SHA256


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re')
    parser.add_argument('--library-path')
    parser.add_argument('--repetitions',type=int,default=3)
    parser.add_argument('--profile-original',action='store_true',help='Read-only coarse GDB timings of original Green generation and FFT, before WASM runs')
    args=parser.parse_args()
    assert 1<=args.repetitions<=10
    binary=args.reference.resolve()/'run/asitic.linux.2.2'
    assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    base=ROOT/'build/perf';base.mkdir(parents=True,exist_ok=True)
    work=Path(tempfile.mkdtemp(prefix='paired-pi2-',dir=base))
    source=(ROOT/'web/technology/CMOS.tek').read_text()
    tech=source
    for key in ['fftx','ffty']:tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = 256',tech)
    script='CELL 50 1.5 0.5\nCCELL 50 20\nMAXNW 500\nEDDY FALSE\nSAVEMAT FALSE\nTEKIO FALSE\nSQUARE NAME=P:LEN=100:WID=90:W=3:S=2:N=2:METAL=M4:EXIT=M3:XORG=0:YORG=0:ORIENT=0:PHASE=1\n'+''.join(f'PI2 P {f}\n' for f in range(1,40,2))
    (work/'process.tek').write_text(tech);(work/'input.asit').write_text(script)
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(binary.parent/'libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu')
    env.pop('LD_PRELOAD',None)
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    originals=[]
    for i in range(args.repetitions):
        folder=work/str(i);folder.mkdir();before=resource.getrusage(resource.RUSAGE_CHILDREN);start=time.perf_counter()
        p=subprocess.run([str(binary),'-g','-t',str(work/'process.tek'),'-x',str(work/'input.asit')],cwd=folder,env=env,input='EXIT\n',text=True,capture_output=True,start_new_session=True,preexec_fn=limits,timeout=120)
        elapsed=time.perf_counter()-start;after=resource.getrusage(resource.RUSAGE_CHILDREN)
        (folder/'stdout.log').write_text(p.stdout);(folder/'stderr.log').write_text(p.stderr)
        values=[[float(x) for x in row] for row in re.findall(r'Pi Model at f=.*?Q =\s*([^,]+),([^,]+),([^\s]+)',p.stdout)]
        assert p.returncode==0 and len(values)==20,(p.returncode,len(values),p.stderr)
        assert set(re.findall(r'Generating capacitance matrix \((\d+)x',p.stdout))=={'17'}
        assert set(re.findall(r'Generating inductance matrix \((\d+)x',p.stdout))=={'34'}
        originals.append(dict(wallSeconds=elapsed,userSeconds=after.ru_utime-before.ru_utime,systemSeconds=after.ru_stime-before.ru_stime,qPrinted=values))
        print(f'Original {i+1}: {elapsed:.6f} seconds for all 20 points',flush=True)
    profile=None
    if args.profile_original:
        folder=work/'profile';folder.mkdir();(folder/'stdin.txt').write_text('EXIT\n')
        commands=r'''set pagination off
set confirm off
set disable-randomization off
set print thread-events off
python
import gdb,time,json,struct
stats={};ends={}
class End(gdb.Breakpoint):
 def __init__(self,ret,label):
  super().__init__('*'+hex(ret),internal=True);self.label=label;self.starts=[]
 def stop(self):
  if self.starts:
   start=self.starts.pop();s=stats.setdefault(self.label,dict(calls=0,seconds=0.));s['calls']+=1;s['seconds']+=time.perf_counter()-start
  return False
class Begin(gdb.Breakpoint):
 def __init__(self,address,label):super().__init__('*'+hex(address),internal=True);self.label=label
 def stop(self):
  sp=int(gdb.parse_and_eval('$esp'));ret=struct.unpack('<I',bytes(gdb.selected_inferior().read_memory(sp,4)))[0];key=(ret,self.label)
  if key not in ends:ends[key]=End(ret,self.label)
  ends[key].starts.append(time.perf_counter());return False
Begin(0x808c350,'green_tables_including_fft')
Begin(0x8091548,'two_dimensional_fft')
started=time.perf_counter()
end
run -g -t ../process.tek -x ../input.asit < stdin.txt
python
open('profile.json','w').write(json.dumps(dict(instrumentedWallSeconds=time.perf_counter()-started,functions=stats),indent=2))
end
quit
'''
        (folder/'profile.gdb').write_text(commands)
        p=subprocess.run(['gdb','-q','-batch','-x','profile.gdb','--args',str(binary)],cwd=folder,env=env,capture_output=True,text=True,timeout=120,start_new_session=True,preexec_fn=limits)
        (folder/'gdb.log').write_text(p.stdout+p.stderr);assert p.returncode==0
        profile=json.loads((folder/'profile.json').read_text());assert all(v['calls']==300 for v in profile['functions'].values())
        print('Original coarse profile: '+json.dumps(profile),flush=True)
    wasm=[]
    for i in range(args.repetitions):
        r=json.loads(subprocess.check_output(['node','tools/benchmark_pi2.mjs'],cwd=ROOT,text=True,timeout=120))
        assert len(r['samples'])==20
        for actual,expected in zip(r['samples'],originals[i]['qPrinted']):
            assert all(abs(a-b)<=.000500001 for a,b in zip(actual['q'],expected)),(actual,expected)
        wasm.append(r);print(f'Production WASM {i+1}: {r["wallSeconds"]:.6f} seconds for all 20 points',flush=True)
    original=statistics.median(r['wallSeconds'] for r in originals);actual=statistics.median(r['wallSeconds'] for r in wasm)
    report=dict(schema=1,originalExecutableSHA256=ORIGINAL_SHA256,wasmSHA256=hashlib.sha256((ROOT/'dist/asitic.wasm').read_bytes()).hexdigest(),technologySHA256=hashlib.sha256(source.encode()).hexdigest(),inputScript=script,task='Identical default coil; direct PI2 at 1,3,...,39 GHz; 256x256 FFT; no cache reuse; 17 contacts and 34 filaments',measurement='Fresh original processes first; fresh production workers second; elapsed includes startup and geometry; timing medians exclude debugger/profiler runs',original=originals,originalProfile=profile,wasm=wasm,originalMedianSeconds=original,wasmMedianSeconds=actual,wasmToOriginalRatio=actual/original,matchedOriginalSpeed=actual<=original,rawProof=str(work))
    encoded=json.dumps(report,indent=2)+'\n'
    (work/'report.json').write_text(encoded)
    out=ROOT/'build/evidence/pi2-speed.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(encoded)
    print(f'Original median {original:.6f}s; WASM median {actual:.6f}s; ratio {actual/original:.4f}')
    assert actual<=original,'Production must match the original simulator speed on the same task'


if __name__=='__main__':main()
