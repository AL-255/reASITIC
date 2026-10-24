"""Measure original separate-reference variability without changing its inputs."""
import argparse,hashlib,json,os,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_three_port import native_script,tech_text,record

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');p.add_argument('--runs',type=int,default=8);args=p.parse_args();binary=args.reference/'run/asitic.linux.2.2'
 if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable differs')
 case=json.loads((ROOT/'tests/fixtures/three-port-cases.json').read_text())['cases'][0];technology=(ROOT/'web/technology/CMOS.tek').read_text();base=ROOT/'build/original-three-port-variability';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base));env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
 def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 script,suffix,f,_=native_script(case,'NAME=G:LEN=100:WID=40:METAL=M2:XORG=100:YORG=100');script+='3PORT '+' '.join(case['names'])+f' {f} {f} .1 Z RECT z.out'+suffix+'\n';results=[]
 for dump in [False,True]:
  for repeat in range(args.runs):
   directory=work/str(dump)/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech_text(case,technology));text=script.replace('SAVEMAT TRUE','SAVEMAT '+('TRUE' if dump else 'FALSE'));(directory/'input.asit').write_text(text)
   process=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,start_new_session=True,preexec_fn=limits,timeout=90);(directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
   if process.returncode:raise RuntimeError('Original audit process failed')
   value=record(directory/'z.out');results.append({'savemat':dump,'repeat':repeat,'script_sha256':hashlib.sha256(text.encode()).hexdigest(),'record_sha256':hashlib.sha256(value.encode()).hexdigest(),'record':value})
 counts={str(dump):len({r['record_sha256'] for r in results if r['savemat']==dump}) for dump in [False,True]};report={'reference':'Unmodified 3PORT with a separate ground; identical script and fresh process/cache per repetition','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.encode()).hexdigest(),'unique_outputs_by_savemat':counts,'runs':results};(ROOT/'tests/fixtures/original-three-port-variability.json').write_text(json.dumps(report,indent=2)+'\n');print('Original output variants per identical SAVEMAT script:',counts);print('Raw proof:',work)
if __name__=='__main__':main()
