"""Capture native two-port records and read-only writer inputs in fresh processes."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text()
    sources={name:ROOT/f'tests/fixtures/{name}-cases.json' for name in ['network','extended','pad','transformer']}
    selected=[]
    for family,path in sources.items():
        cases=json.loads(path.read_text())['cases']
        # A full node solve for each model, plus nonsymmetric transformer ports,
        # eddy integration, and a changed frequency/mesh.
        chosen=[next(c for c in cases if c['id'] in [0,1,16,17,18])] if family=='extended' else [cases[0]]
        if family=='network':chosen+=[next(c for c in cases if c['eddy_mode']),cases[-1]]
        if family=='transformer':chosen+=[next(c for c in cases if 'reflection' in c['name'])]
        selected.extend((family,c) for c in chosen)
    base=ROOT/'build/original-two-port';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None)
    env['LD_LIBRARY_PATH']='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(item):
        family,case=item;name=family+'-'+case['name'];tech=re.sub(r'(?mi)^\s*eddy\s*=.*$',lambda _:'\n'.join('eddy = '+str(i) for i in case['layers']),text)
        for key,v in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(v),tech)
        l,w,t=case['cells'];cl,cw=case['contact_cells'];script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY '+('TRUE' if case['eddy_mode'] else 'FALSE')+'\n'
        creators=[case['primary'],case['secondary']] if family=='transformer' else [case['device'],*case['pads']] if family=='pad' else [{'id':case['id'],'keywords':case['keywords']}]
        for c in creators:
            if c:script+=COMMANDS[c['id']]+' '+c['keywords']+'\n'
        g=case.get('ground','');g='@self' if case.get('self_ground') else g
        ground={'@self':'A','@1':'A' if family=='transformer' else 'P1','@2':'B' if family=='transformer' else 'P2'}.get(g,'G' if g else 'A')
        if g and not g.startswith('@'):script+='WIRE '+g+'\n'
        f=case['frequency_ghz'];model='TRANSPI' if family=='transformer' else 'PI';modes=[('s','S',False),('y','Y',False),('model',model,False),('s-polar','S',True),('y-polar','Y',True),('model-polar',model,True)]
        for key,parameter,polar in modes:
            form='POLAR' if polar else 'RECT'
            if family=='transformer':command=f'2PORTTRANS A B {f} {f} .1 {parameter} {form} {key}.out'+(' '+ground+' '+str(case['termination']) if g else '')
            elif family=='pad':command=f'2PORTPAD A P1 P2 {f} {f} .1 {parameter} {form} {key}.out '+ground
            elif family=='extended':command=f'2PORTX A {f} {f} .1 {parameter} {form} SLOW {key}.out'+(' '+ground if g else '')
            else:command=f'2PORTGND A {ground} {f} {f} .1 {parameter} {form} {key}.out'
            script+=command+'\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/name/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n')
            gdb='''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json,struct
rows=[]
class Writer(gdb.Breakpoint):
 def stop(self):
  m=gdb.selected_inferior().read_memory
  esp=int(gdb.parse_and_eval('$esp'))
  rows.append(dict(frequency_hz=struct.unpack('<d',m(esp+8,8))[0],parameter=struct.unpack('<i',m(esp+16,4))[0],polar=bytes(m(esp+20,1))[0],y_hex=(bytes(m(0x80d8da8,32))+bytes(m(0x80d8dd8,32))).hex(),model_hex=bytes(m(0x80d8828,72)).hex(),prior_s_hex=b''.join(bytes(m(a,16)) for a in [0x80d8d58,0x80d8d28,0x80d8d48,0x80d8d18]).hex()))
  return False
Writer('*0x8088c9c')
end
run -g -t CMOS.tek -x input.asit < exit.txt
python
open('writer.json','w').write(json.dumps(rows))
end
quit
'''
            (directory/'capture.gdb').write_text(gdb)
            p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=180,start_new_session=True,preexec_fn=limits);(directory/'gdb.log').write_text(p.stdout+'\n'+p.stderr)
            if p.returncode:raise RuntimeError(name+' GDB failed: '+p.stdout+p.stderr)
            inputs=json.loads((directory/'writer.json').read_text());assert len(inputs)==len(modes),(name,inputs,p.stdout)
            records={};headers={}
            for key,_,_ in modes:
                content=(directory/(key+'.out')).read_text();lines=content.splitlines(keepends=True)
                # The final line is the numerical row. Keep all spacing and the
                # preceding column header verbatim, excluding dated comments.
                record='\n'+lines[-1].removesuffix('\n');records[key]=record
                start=content.rfind('\n# GHZ');assert start>=0
                headers[key]=content[start:content.rfind(record)]
            # Native S struct order is s11,s12,s21,s22; the second entry occurs
            # after the first S writer and observes its computed global state.
            snapshots.append({'inputs':inputs,'scattering_hex':inputs[1]['prior_s_hex'],'records':records,'headers':headers})
        assert snapshots[0]==snapshots[1],name+' differs between fresh processes'
        print(name+': six native record modes and writer bytes repeat exactly',flush=True)
        return {'name':name,'family':family,'case':case,**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:cases=list(pool.map(capture,selected))
    document={'reference':'Unmodified original port commands, two fresh processes; GDB only observes writer input and previously computed S globals','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':{n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in sources.items()},'cases':cases}
    output=ROOT/'tests/fixtures/original-two-port.json';output.write_text(json.dumps(document,indent=2)+'\n');print(output);print('Raw proof:',work)
if __name__=='__main__':main()
