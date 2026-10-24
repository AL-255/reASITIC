"""Capture unmodified three-port sweeps, including saved native frequencies."""
import argparse,concurrent.futures,hashlib,json,os,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_three_port import native_script,tech_text
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/three-port-sweep-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=[c for c in json.loads(source.read_text())['cases'] if c['reference']=='original']
    base=ROOT/'build/original-three-port-sweep';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(item):
        case=item['case'];script,suffix,_,_=native_script(case);first,last,step=item['range_ghz'];modes=[]
        for polar in [False,True]:
            for parameter in ['S','Y','Z']:
                key=parameter.lower()+('-polar' if polar else '');modes.append((key,parameter,polar));script+='3PORT '+' '.join(case['names'])+f' {first} {last} {step} {parameter} '+('POLAR' if polar else 'RECT')+' '+key+'.out'+suffix+'\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/item['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech_text(case,text));(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json
rows=[]
class Writer(gdb.Breakpoint):
 def stop(self):
  memory=gdb.selected_inferior().read_memory;esp=int(gdb.parse_and_eval('$esp'))
  rows.append(bytes(memory(esp+8,8)).hex());return False
Writer('*0x80890bc')
end
run -g -t CMOS.tek -x input.asit < exit.txt
python
open('frequencies.json','w').write(json.dumps(rows))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb);p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=240,start_new_session=True,preexec_fn=limits);(directory/'gdb.log').write_text(p.stdout+'\n'+p.stderr)
            if p.returncode:raise RuntimeError(item['name']+': native capture failed '+p.stdout+p.stderr)
            rows=json.loads((directory/'frequencies.json').read_text());assert len(rows)>len(modes) and len(rows)%len(modes)==0
            count=len(rows)//len(modes);frequencies=rows[:count];assert all(rows[i*count:(i+1)*count]==frequencies for i in range(len(modes)))
            records={}
            for key,parameter,_ in modes:
                content=(directory/(key+'.out')).read_text();start=content.find('\n!\t'+parameter+'11');assert start>=0
                records[key]=content[start:]+'\n'
            snapshots.append({'frequency_hex':frequencies,'records':records})
        assert snapshots[0]==snapshots[1],item['name']+' differs between fresh processes'
        print(item['name']+': six native S/Y/Z sweep modes and saved frequencies repeat exactly',flush=True);return {'name':item['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-three-port-sweep.json';output.write_text(json.dumps({'reference':'Unmodified original 3PORT sweeps in two fresh processes; read-only GDB writer-frequency observations; complete numerical file sections','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'cases':captured},indent=2)+'\n');print(output);print('Raw proof:',work)
if __name__=='__main__':main()
