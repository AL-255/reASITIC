"""Repeat original PI3 and PI2 controls on both process stacks."""
import argparse,concurrent.futures,hashlib,json,math,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS
from capture_network import matrix

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=p.parse_args();binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
 source=ROOT/'tests/fixtures/pi3-cases.json';cases=json.loads(source.read_text())['cases'];base=ROOT/'build/original-pi3';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base));technologies={name:(ROOT/('web/technology/'+name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
 env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
 def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 def capture(case):
  tech=technologies[case['technology']]
  for key,value in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
  l,w,t=case['cells'];cl,cw=case['contact_cells'];creator=case['creator'];script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY FALSE\nSAVEMAT TRUE\n'+COMMANDS[creator['id']]+' '+creator['keywords']+'\n'
  ground='G' if case.get('ground') else 'A'
  if case.get('ground'):script+='WIRE '+case['ground']+'\n'
  outputs={}
  for command in ['PI3','PI2']:
   snapshots=[];solve=(f'PI3 A {ground} {case["frequency_ghz"]}' if command=='PI3' else f'PI2 A {case["frequency_ghz"]} {ground}')+'\nZIN S\nZIN X\nZIN D\nSHUNTR S\nSHUNTR D\n'
   for repeat in range(2):
    directory=work/case['name']/command/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script+solve)
    result=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits);(directory/'stdout.log').write_text(result.stdout);(directory/'stderr.log').write_text(result.stderr)
    if result.returncode:raise RuntimeError(case['name']+'/'+command+': status '+str(result.returncode)+' '+result.stderr)
    text=result.stdout;quality=re.search(r'Pi Model at f=(\S+) GHz:  Q = ([^\n]+)',text);assert quality,case['name']+'/'+command+': '+text
    numeric=lambda value:re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?|[-+]?nan|[-+]?inf',value,re.I)
    cache=next(directory.glob('*.dat')).read_bytes();layers=tech.lower().count('<layer>');metals=tech.lower().count('<metal>');payload=cache[16+layers*40+metals*236:];nx,ny=case['grid'];assert len(payload)==metals*(metals+1)//2*((nx+1)*(ny+1)+2*nx+2*ny)*16
    checksum=14695981039346656037
    for byte in payload:checksum=((checksum^byte)*1099511628211)&((1<<64)-1)
    snapshots.append({'green_table_hash':f'{checksum:016x}','stage_matrices':{name:matrix(directory/name) for name in ['ZC.out','YM.out','Ycap.out']},'quality_printed':numeric(quality[2]),'pi_printed':[re.search(pattern,text)[1].strip() for pattern in [r'L  =\s*(\S+) nH',r'R  =([^\n]+)',r'Cs1=\s*(\S+) fF',r'Rs1=([^\n]+)',r'Cs2=\s*(\S+) fF',r'Rs2=([^\t]+)',r'f_res = ([^G]+)GHz']],'input_impedance_printed':[numeric(line) for line in re.findall(r'Zin = ([^\n]+)',text)],'shunt_printed':re.findall(r'The shunt resistance is ([^\n]+)',text)})
   assert snapshots[0]==snapshots[1],case['name']+'/'+command+' differs between fresh runs';outputs[command]=snapshots[0]
  print(case['name']+': repeated original PI3 and explicit-ground PI2 '+('differ' if outputs['PI3']!=outputs['PI2'] else 'agree'),flush=True);return {'name':case['name'],**outputs}
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
 output=ROOT/'tests/fixtures/original-pi3.json';output.write_text(json.dumps({'reference':'Unmodified PI3 and explicit-ground PI2 in two fresh processes each, stage matrices and printed reports','executable_sha256':ORIGINAL_SHA256,'technology_sha256':{name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'cases':captured},indent=2)+'\n');print(output);print('Raw proof:',work)
if __name__=='__main__':main()
