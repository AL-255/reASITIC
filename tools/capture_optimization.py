"""Observe original SWEEP candidate descriptors, physical geometry and models."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--cases',type=Path,default=ROOT/'tests/fixtures/optimization-cases.json');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-optimization.json');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=args.cases;cases=json.loads(source.read_text())['cases'];technologies={name:(ROOT/'web/technology'/(name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
    base=ROOT/'build'/('original-'+source.stem);base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        snapshots=[];tech=technologies[case['technology']];metals=len(re.findall(r'(?im)^\s*<metal>',tech));p=case['parameters'];search=case.get('search','SWEEP')
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'process.tek').write_text(tech)
            # Stop the original immediately after keyword normalization. Its
            # subsequent batch-name loop reads unwritten text and may hang.
            (directory/'normalize.asit').write_text(case.get('prelude','')+'\n'+search+' '+case['keywords']+'\n')
            normalize='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,struct,json
class Parsed(gdb.FinishBreakpoint):
 def stop(self):
  b=bytes(gdb.selected_inferior().read_memory(0x80d86e0,304));d={}
  for key,offset in [('SMIN',160),('SINC',168),('SMAX',176),('WMIN',184),('WINC',192),('WMAX',200),('AMIN',208),('AINC',216),('AMAX',224),('MINQ',232),('LMIN',248),('LMAX',256),('FREQUENCY_HZ',292)]:d[key]=struct.unpack_from('<d',b,offset)[0]
  for key,offset in [('METAL',272),('EXIT',276),('SIDES',280),('SIMTYPE',284)]:d[key]=struct.unpack_from('<i',b,offset)[0]
  open('normalized.json','w').write(json.dumps(d,allow_nan=False));return True
class Parse(gdb.Breakpoint):
 def stop(self):Parsed(gdb.newest_frame(),internal=True);return False
Parse('*0x80847ec',internal=True)
end
run -g -t process.tek -x normalize.asit < /dev/null
quit
'''
            (directory/'normalize.gdb').write_text(normalize)
            observed=subprocess.run(['gdb','-q','-batch','-x','normalize.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=30,start_new_session=True,preexec_fn=limits)
            (directory/'normalize.log').write_text(observed.stdout+observed.stderr);assert observed.returncode==0
            normalized=json.loads((directory/'normalized.json').read_text());q={**p,**normalized,'FREQ':normalized['FREQUENCY_HZ']/1e9}
            # Feed those exact normalized doubles to the original prompted grid.
            # No original registers, memory, executable or saved data are edited.
            command=search
            (directory/'input.asit').write_text(case.get('prelude','')+'\nEDDY FALSE\nCELL 1000 1000 1000\nCCELL 1000 1000\n'+command+'\n')
            answers=[p['FNAME'],p['NAME'],q['FREQ'],f"{q['LMIN']} {q['LMAX']}",q['MINQ'],f"{q['AMIN']} {q['AINC']} {q['AMAX']}",f"{q['SMIN']} {q['SINC']} {q['SMAX']}",f"{q['WMIN']} {q['WINC']} {q['WMAX']}",q['METAL'],q['EXIT'],q['SIDES']]+([q['SIMTYPE']] if search=='SWEEP' else [])
            (directory/'stdin.txt').write_text('\n'.join(map(str,answers))+('\n' if answers else '')+'EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,struct,json,math
records=[]
def read(a,n):return bytes(gdb.selected_inferior().read_memory(a,n))
def u32(a):return struct.unpack('<I',read(a,4))[0]
def f64(a):return struct.unpack('<d',read(a,8))[0]
def finite(v):return v if math.isfinite(v) else str(v)
class Constructed(gdb.FinishBreakpoint):
 def stop(self):
  address=u32(0x80ce558);origin=[f64(address+o) for o in [0x54,0x5c]];segments=[];segment=u32(address+0xa8)
  while segment:
   metal=struct.unpack('<i',read(segment+0xdc,4))[0];entry=dict(metal=metal,width=f64(segment+0xcc),points=[list(struct.unpack('<3d',read(segment+o,24))) for o in [0,24,0x44,0x5c,0x88,0xa0]])
   if metal<METALS:entry['thickness']=f64(segment+0xd4)
   else:entry.update(nx=u32(segment+0xe0),ny=u32(segment+0xe4))
   segments.append(entry);segment=u32(segment+0xec)
  records[-1]['geometry']=dict(origin=origin,segments=segments);return False
class Construct(gdb.Breakpoint):
 def stop(self):
  a=u32(int(gdb.parse_and_eval('$esp'))+4);words=[struct.unpack('<i',read(a+o,4))[0] for o in [0x50,0x64,0x68,0x6c]];values=[f64(a+o) for o in [0x70,0x88,0x90,0x98,0x54,0x5c]]
  records.append(dict(descriptor=words+values));Constructed(gdb.newest_frame(),internal=True);return False
class Solved(gdb.FinishBreakpoint):
 def stop(self):
  records[-1]['pi']=[finite(v) for v in struct.unpack('<12d',read(0x80d8820,96))];return False
class Solve(gdb.Breakpoint):
 def stop(self):Solved(gdb.newest_frame(),internal=True);return False
Construct('*CONSTRUCTOR',internal=True);Construct('*0x8057248',internal=True);Solve('*0x8050b2c',internal=True)
end
run -g -t process.tek -x input.asit < stdin.txt
python
open('observations.json','w').write(json.dumps(records,allow_nan=False))
end
quit
'''.replace('METALS',str(metals)).replace('CONSTRUCTOR','0x805af5c' if search=='SWEEPMM' else '0x8056670')
            (directory/'capture.gdb').write_text(gdb);process=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits)
            (directory/'gdb.log').write_text(process.stdout+process.stderr)
            if process.returncode or 'SIGSEGV' in process.stdout:raise RuntimeError(case['name']+': '+process.stdout+process.stderr)
            records=json.loads((directory/'observations.json').read_text());assert records and all('geometry' in r and 'pi' in r for r in records),(case['name'],records)
            output=(directory/p['FNAME']).read_text();rows=[re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?|[-+]?nan|[-+]?inf',line,re.I) for line in output.splitlines() if line.strip() and re.match(r'[-+.0-9]',line.strip())]
            assert all(len(row)==13 for row in rows),(case['name'],rows)
            snapshots.append(dict(normalized=normalized,candidates=records,accepted_printed=rows))
        # Keyword SWEEP's constructor name comes from unwritten stack text;
        # compare the defined descriptor and physical fields independently.
        assert json.dumps(snapshots[0],sort_keys=True,allow_nan=False)==json.dumps(snapshots[1],sort_keys=True,allow_nan=False),case['name']+' original outputs vary'
        print(case['name']+': '+str(len(records))+' original candidates and '+str(len(rows))+' accepted reports repeat',flush=True);return dict(name=case['name'],**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:captured=list(pool.map(capture,cases))
    output=args.output;output.write_text(json.dumps(dict(reference='Two original keyword-normalization return observations and two fresh original prompted SWEEP runs per case using those normalized values; read-only constructor/solver observations and native output rows; undefined name/mesh padding excluded',executable_sha256=ORIGINAL_SHA256,case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},cases=captured),indent=2)+'\n');print(output);print('Raw proof:',work)

if __name__=='__main__':main()
