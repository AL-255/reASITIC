"""Repeat unmodified 2PZIN commands; observe inputs/results without writes."""
import argparse,hashlib,json,os,re,resource,subprocess,tempfile,concurrent.futures
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS
GDB='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json,struct
rows=[]
def read(a,n):return bytes(gdb.selected_inferior().read_memory(a,n))
def admittance():return (read(0x80d8da8,32)+read(0x80d8dd8,32)).hex()
class Return(gdb.FinishBreakpoint):
 def __init__(self,row,p):super().__init__(gdb.newest_frame(),internal=True);self.row=row;self.p=p
 def stop(self):self.row['impedance_hex']=read(self.p,16).hex();return False
class Entry(gdb.Breakpoint):
 def stop(self):
  sp=int(gdb.parse_and_eval('$esp'));p=struct.unpack('<I',read(sp+4,4))[0]
  row=dict(admittance_hex=admittance(),load_hex=read(sp+8,16).hex(),port=struct.unpack('<i',read(sp+24,4))[0],frequency_hex=read(0x80ce578,8).hex());rows.append(row);Return(row,p);return False
Entry('*0x804e9b0')
end
run -g -t CMOS.tek -x input.asit < exit.txt
python
open('observations.json','w').write(json.dumps(rows,indent=2))
end
quit
'''
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=parser.parse_args()
 binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
 source=ROOT/'tests/fixtures/loaded-port-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases'];base=ROOT/'build/original-loaded-port';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
 env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
 def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 def capture(item):
  case=next(c for c in json.loads((ROOT/('tests/fixtures/'+item['family']+'-cases.json')).read_text())['cases'] if c['name']==item['reference_case']);family=item['family'];tech=text
  for key,v in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(v),tech)
  tech=re.sub(r'(?mi)^\s*eddy\s*=.*$',lambda _:'\n'.join('eddy = '+str(i) for i in case['layers']),tech)
  l,w,t=case['cells'];cl,cw=case['contact_cells'];script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY '+('TRUE' if case['eddy_mode'] else 'FALSE')+'\n'
  creators=[case['primary'],case['secondary']] if family=='transformer' else [case['device'],*case['pads']] if family=='pad' else [{'id':case['id'],'keywords':case['keywords']}]
  for creator in creators:script+=COMMANDS[creator['id']]+' '+creator['keywords']+'\n'
  ground=case.get('ground');ground_name='A' if case.get('self_ground') else 'G' if ground else ''
  if ground:script+='WIRE '+ground+'\n'
  f=case['frequency_ghz']
  if family=='transformer':script+=f'CALCTRANS A B {f}'+(' '+ground_name+' '+str(case['termination']) if ground_name else '')+'\n'
  elif family=='pad':script+=f'PI4 A {f} P1 P2'+(' '+ground_name if ground_name else '')+'\n'
  else:script+=item['model']+f' A {f}'+(' '+ground_name if ground_name else '')+'\n'
  script+='2PZIN\n2PZIN '+' '.join(map(str,[*item['load'],item['port'],*item['caps']]))+'\n2PZIN\n';snapshots=[]
  for repeat in range(2):
   directory=work/item['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n');(directory/'capture.gdb').write_text(GDB)
   p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=180,start_new_session=True,preexec_fn=limits);(directory/'gdb.log').write_text(p.stdout+'\n'+p.stderr)
   if p.returncode:raise RuntimeError(item['name']+': original failed '+p.stdout+p.stderr)
   rows=json.loads((directory/'observations.json').read_text());assert len(rows)==3,item['name'];reports=re.findall(r'Zin = ([^\n]+)',p.stdout);assert len(reports)==3
   snapshots.append(dict(base=rows[0],loaded=rows[1],after=rows[2],printed=reports))
  assert snapshots[0]==snapshots[1],item['name']+' differs between fresh processes'
  print(item['name']+': 2PZIN inputs, output bits and reports repeat in two fresh processes',flush=True)
  return dict(name=item['name'],case=case,**snapshots[0])
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
 output=ROOT/'tests/fixtures/original-loaded-port.json';output.write_text(json.dumps(dict(reference='Unmodified 2PZIN after original PI2, PIX, PI4 and CALCTRANS; two fresh processes; read-only entry/return observations',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),cases=captured),indent=2)+'\n');print(output);print('Raw proof:',work)
if __name__=='__main__':main()
