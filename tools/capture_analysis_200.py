"""Freeze full-precision results from the unmodified original ASITIC executable.

GDB only observes the original reporting arguments; no numerical state, return
value, register, or input record is replaced. Each scenario repeats in a fresh
process. CI consumes the committed gzip blob and never invokes this recorder.
"""
import argparse,concurrent.futures,gzip,hashlib,json,math,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256,matrix

GDB=r'''set pagination off
set confirm off
set disable-randomization off
python
import gdb,struct,json,math
values={};words={}
def read(address,n):return bytes(gdb.selected_inferior().read_memory(address,n))
def u32(address):return struct.unpack('<I',read(address,4))[0]
def string(address):return read(address,256).split(b'\0')[0].decode('ascii','replace')
def save(key,address):
 raw=read(address,8);v=struct.unpack('<d',raw)[0];values[key]=v if math.isfinite(v) else None;words[key]=raw.hex()
class Report(gdb.Breakpoint):
 def stop(self):
  stack=int(gdb.parse_and_eval('$esp'));fmt=string(u32(stack+8))
  if fmt.startswith('Pi Model at f='):
   for i,key in enumerate(['pi.q1','pi.q2','pi.qd']):save(key,stack+20+8*i)
  elif fmt.startswith('L  ='):
   for i,key in enumerate(['pi.inductance_nh','pi.resistance_ohm','pi.c2_ff','pi.r2_ohm','pi.c1_ff','pi.r1_ohm','pi.resonance_ghz']):save(key,stack+12+8*i)
  elif fmt.startswith('Inductance of '):save('conductor.inductance_nh',stack+16)
  elif fmt.startswith('Resistance of '):save('conductor.resistance_dc',stack+16)
  elif fmt.startswith('Total Metal Area of '):save('conductor.metal_area_um2',stack+16)
  elif fmt.startswith('LF Extrapolated Q of '):save('conductor.quality_low_frequency',stack+16)
  elif fmt.startswith('%s: at %lf GHz R = '):save('conductor.resistance_hf',stack+24)
  return False
Report('*0x804ae04',internal=True)
end
run -g -t process.tek -x input.asit < stdin.txt
python
open('observations.json','w').write(json.dumps(dict(values=values,words=words),allow_nan=False))
end
quit
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path',help='Original executable legacy runtime DSO search path');parser.add_argument('--only',help='Capture one case into a scratch output for recorder validation');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-analysis-200.json.gz');parser.add_argument('--jobs',type=int,default=4);args=parser.parse_args()
    binary=args.reference.resolve()/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/analysis-200-cases.json';definition=json.loads(source.read_text());cases=definition['cases'];assert len(cases)==200
    if args.only:cases=[c for c in cases if c['id']==args.only];assert len(cases)==1
    technologies={name:(ROOT/'web/technology'/(name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
    base=ROOT/'build/original-analysis-200';base.mkdir(exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(binary.parent/'libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(c):
        tech=technologies[c['technology']]
        for key,value in zip(['fftx','ffty'],c['fft']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        name=c['winding'];f=c['frequency_ghz'];script='CELL '+' '.join(map(str,c['mesh']))+'\nCCELL '+' '.join(map(str,c['contact_mesh']))+f'\nMAXNW {c["maxnw"]}\nEDDY FALSE\nTEKIO TRUE\nSAVEMAT TRUE\n'+c['command']+f'\nINDUCTANCE {name}\nRESIS {name}\nRESISHF {name} {f}\nQ {name} {f}\nMETALAREA {name}\nLMAT {name} {f} lmat.out\nPI2 {name} {f}\n'
        repeats=[];report_lines=[]
        for repeat in range(2):
            directory=work/c['id']/str(repeat);directory.mkdir(parents=True);(directory/'process.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'stdin.txt').write_text('EXIT\n');(directory/'capture.gdb').write_text(GDB)
            p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=180,start_new_session=True,preexec_fn=limits);(directory/'gdb.log').write_text(p.stdout+p.stderr)
            # Native EXIT raises SIGQUIT after reporting; other signals are
            # failures. Required observations and complete files are checked.
            assert p.returncode==0 and not re.search(r'Program received signal (?!SIGQUIT)[A-Z]+',p.stdout),(c['id'],p.stdout[-3000:],p.stderr)
            observed=json.loads((directory/'observations.json').read_text());assert len(observed['values'])==15,(c['id'],observed)
            assert all(observed['values'][key] is not None for key in ['pi.q1','pi.q2','pi.qd','pi.inductance_nh','pi.resistance_ohm']),c['id']+' has undefined primary native results'
            lmat=matrix(directory/'lmat.out');z=[sum(v[k] for row in lmat for v in row) for k in range(2)];assert all(map(math.isfinite,z)) and z[0]!=0
            observed['values'].update({'lmat.resistance_ohm':z[0],'lmat.reactance_ohm':z[1],'lmat.inductance_nh':z[1]/(2*math.pi*struct.unpack_from('<d',next(directory.glob('*.dat')).read_bytes(),8)[0])*1e9,'lmat.q':z[1]/z[0]})
            observed['frequency_hz']=struct.unpack_from('<d',next(directory.glob('*.dat')).read_bytes(),8)[0]
            observed['stage_sha256']={n:hashlib.sha256((directory/n).read_bytes()).hexdigest() for n in ['lmat.out','ZC.out','YM.out','Ycap.out']}
            repeats.append(observed)
            log=(directory/'asitic.LOG').read_text();report_lines.append([line.strip() for line in log.splitlines() if re.match(r'(Inductance of |Resistance of |Total Metal Area of |LF Extrapolated Q of |Pi Model at |L  =|Cs1=|Cs2=|\S+: at \S+ GHz R = )',line)])
        assert repeats[0]==repeats[1],c['id']+' original numeric results vary between fresh processes'
        print(c['id']+': repeated original full-precision results captured',flush=True)
        return dict(id=c['id'],**repeats[0],native_reports=report_lines[0],input_sha256=hashlib.sha256(script.encode()).hexdigest())
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:captured=list(pool.map(capture,cases))
    document=dict(schema=1,case_count=len(captured),reference='Unmodified original ASITIC; read-only report-argument observations and original LMAT files; two fresh processes per case',executable_sha256=ORIGINAL_SHA256,definitions_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),capture_script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},repetitions=2,values_units='Explicit per-metric keys; full binary64 report arguments before decimal formatting; LMAT sums from original 20-decimal files',cases=captured)
    blob=gzip.compress(json.dumps(document,sort_keys=True,separators=(',',':'),allow_nan=False).encode(),mtime=0);args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(blob)
    manifest=dict(schema=1,case_count=len(captured),blob=args.output.name,blob_sha256=hashlib.sha256(blob).hexdigest(),uncompressed_sha256=hashlib.sha256(gzip.decompress(blob)).hexdigest(),definitions_sha256=document['definitions_sha256'],relative_error_limit=.001,comparison='Every finite nonzero metric: abs(actual-reference)/abs(reference) < 0.001; reference zero: exact numeric zero; undefined reference: matching null; no case exclusions')
    args.output.with_suffix('').with_suffix('.manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
