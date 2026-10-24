"""Read automatic mesh limits from the unmodified original executable."""
import argparse,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re')
    args=parser.parse_args();binary=args.reference/'run/asitic.linux.2.2'
    assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    technology=ROOT/'web/technology/CMOS.tek'
    definitions=[dict(name='default',alpha=.03,beta=.6,frequencies_ghz=[.125,.5,1,3.25,10]),dict(name='alternate-top-metal',alpha=.04,beta=.7,thickness=.7,sheet_milliohms=47,frequencies_ghz=[.25,.75,2])]
    base=ROOT/'build/original-automatic-cells';base.mkdir(parents=True,exist_ok=True)
    work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None)
    env['LD_LIBRARY_PATH']='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    cases=[]
    for definition in definitions:
        tech=technology.read_text()
        for key,value in [('chipx',32),('chipy',32),('fftx',64),('ffty',64)]:tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        if 'thickness' in definition:
            start=tech.index('<metal> 4');end=tech.find('\n<',start+1);end=len(tech) if end<0 else end
            metal=tech[start:end]
            for key,value in [('t',definition['thickness']),('rsh',definition['sheet_milliohms'])]:
                metal,n=re.subn(r'(?mi)^(\s*'+key+r'\s*=\s*)[^\n]+',lambda m:m[1]+str(value),metal);assert n==1
            tech=tech[:start]+metal+tech[end:]
        script='EDDY FALSE\nWIRE NAME=A:LEN=20:WID=2:METAL=M5:XORG=4:YORG=8\n'
        script+=f"AUTOCELL {definition['alpha']} {definition['beta']}\n"
        # Start with an untouched winding for the complete port sweep.
        script+='2PORTGND A A .5 1.5 .5 S RECT s.out\n'
        for i,f in enumerate(definition['frequencies_ghz']):script+=f'LMAT A {f} lmat-{i}.out\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/definition['name']/str(repeat);directory.mkdir(parents=True)
            (directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json,struct
rows=[]
def read(address,n):return bytes(gdb.selected_inferior().read_memory(address,n))
class Returned(gdb.FinishBreakpoint):
 def __init__(self,row):
  super().__init__(gdb.newest_frame(),internal=True);self.row=row
 def stop(self):
  self.row['limits_hex']=read(0x80ce580,40).hex();rows.append(self.row);return False
class Updater(gdb.Breakpoint):
 def stop(self):
  esp=int(gdb.parse_and_eval('$esp'));count=struct.unpack('<i',read(0x80d8904,4))[0];metals=struct.unpack('<I',read(0x80d8910,4))[0]
  row={key:read(address,8).hex() for key,address in [('frequency_hex',esp+4),('alpha_hex',0x80ce5b0),('beta_hex',0x80ce5b8),('thickness_hex',metals+(count-1)*236+0xb0),('resistance_hex',metals+(count-1)*236+0xb8)]}
  Returned(row);return False
Updater('*0x80605ec',internal=True)
end
run -g -t CMOS.tek -x input.asit < exit.txt
python
open('limits.json','w').write(json.dumps(rows))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb)
            p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=180,start_new_session=True,preexec_fn=limits)
            (directory/'gdb.log').write_text(p.stdout+'\n'+p.stderr)
            if p.returncode:raise RuntimeError(p.stdout+p.stderr)
            rows=json.loads((directory/'limits.json').read_text());assert len(rows)==len(definition['frequencies_ghz'])+3,(definition,rows)
            record=(directory/'s.out').read_text();record=record[record.rfind('\n# GHZ'):]+'\n'
            snapshots.append(dict(updates=rows,scattering_record=record))
        assert snapshots[0]==snapshots[1]
        cases.append(dict(**definition,**snapshots[0]));print(definition['name']+': raw limits and port output repeat exactly',flush=True)
    output=ROOT/'tests/fixtures/original-automatic-cells.json'
    output.write_text(json.dumps(dict(reference='Unmodified native AUTOCELL, LMAT and 2PORTGND in two fresh processes; read-only breakpoints at updater entry/return',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),domain_um=32,fft_cells=64,cases=cases),indent=2)+'\n')
    print(output);print('Raw proof:',work)

if __name__=='__main__':main()
