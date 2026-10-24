"""Capture complete native port files with nondefault MAXNW settings."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    technology=ROOT/'web/technology/CMOS.tek';definitions=[]
    for family,maximum in [('network',3),('extended',4),('pad',5),('transformer',8)]:
        item=next(c for c in json.loads((ROOT/'tests/fixtures/port-sweep-cases.json').read_text())['cases'] if c['family']==family)
        definitions.append(dict(family=family,max_width_cells=maximum,case=item['case']))
    definitions.append(dict(family='three-port',max_width_cells=10,case=json.loads((ROOT/'tests/fixtures/three-port-cases.json').read_text())['cases'][0]))
    base=ROOT/'build/original-maxnw-ports';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(item):
        family=item['family'];case=item['case'];maximum=item['max_width_cells'];tech=technology.read_text()
        for key,value in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        # A 1um transverse cell activates the native edge grading; all other
        # acquisition choices come from already captured original port cases.
        cells=[1000,1,1000];contacts=case['contact_cells'];script=f'EDDY FALSE\nCELL {cells[0]} {cells[1]} {cells[2]}\nCCELL {contacts[0]} {contacts[1]}\nMAXNW {maximum}\n'
        creators=case['creators'] if family=='three-port' else [case['primary'],case['secondary']] if family=='transformer' else [case['device'],*case['pads']] if family=='pad' else [{'id':case['id'],'keywords':case['keywords']}]
        for c in creators:
            if c:script+=COMMANDS[c['id']]+' '+c['keywords']+'\n'
        g=case.get('ground','');ground={'@self':'A','@1':'A' if family=='transformer' else 'P1','@2':'B' if family=='transformer' else 'P2'}.get(g,'G' if g else 'A')
        if g and not g.startswith('@'):script+='WIRE '+g+'\n'
        keys=['s','y']+(['z'] if family=='three-port' else [])
        for key in keys:
            values=f'1 1 .5 {key.upper()} RECT {key}.out'
            if family=='three-port':command='3PORT '+' '.join(case['names'])+' '+values
            elif family=='transformer':command=f'2PORTTRANS A B {values}'+(' '+ground+' '+str(case['termination']) if g else '')
            elif family=='pad':command=f'2PORTPAD A P1 P2 {values} '+ground
            elif family=='extended':command=f'2PORTX A 1 1 .5 {key.upper()} RECT SLOW {key}.out'+(' '+ground if g else '')
            else:command=f'2PORTGND A {ground} {values}'
            script+=command+'\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/family/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script)
            p=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=180,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(p.stdout);(directory/'stderr.log').write_text(p.stderr)
            if p.returncode:raise RuntimeError(family+': '+p.stdout+p.stderr)
            records={}
            for key in keys:
                text=(directory/(key+'.out')).read_text();start=text.find('\n!\t'+key.upper()+'11') if family=='three-port' else text.rfind('\n# GHZ');assert start>=0;records[key]=text[start:]+'\n'
            snapshot=dict(records=records)
            if family=='extended':
                gdb="""set pagination off
set confirm off
set disable-randomization off
break *0x805556c
commands
silent
set $cap=*(unsigned int*)($esp+4)
set $ind=*(unsigned int*)($esp+8)
set $n=*(unsigned int*)($cap+12)
set $c=*(unsigned int*)$cap
set $z=*(unsigned int*)$ind
dump binary memory cap.bin $c ($c+$n*$n*16)
dump binary memory ind.bin $z ($z+$n*$n*16)
continue
end
run -g -t CMOS.tek -x input.asit < exit.txt
quit
"""
                (directory/'capture.gdb').write_text(gdb);(directory/'exit.txt').write_text('EXIT\n')
                g=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=180,start_new_session=True,preexec_fn=limits)
                (directory/'gdb.log').write_text(g.stdout+g.stderr)
                if g.returncode:raise RuntimeError(g.stdout+g.stderr)
                snapshot['raw_stages']={key:(directory/file).read_bytes().hex() for key,file in [('capacitance_matrix','cap.bin'),('inductance_impedance_matrix','ind.bin')]}
            snapshots.append(snapshot)
        assert snapshots[0]==snapshots[1],family+' native port records changed between fresh runs'
        print(family+': complete '+('/'.join(keys)).upper()+' files repeat exactly',flush=True)
        return dict(**item,cells=cells,contact_cells=contacts,**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:cases=list(pool.map(capture,definitions))
    output=ROOT/'tests/fixtures/original-maxnw-ports.json';output.write_text(json.dumps(dict(reference='Unmodified original port commands with MAXNW; full numerical files from two fresh processes per model',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),cases=cases),indent=2)+'\n');print(output);print('Raw proof:',work)

if __name__=='__main__':main()
