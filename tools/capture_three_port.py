"""Capture unmodified original three-port records and explicit native backing states.

GDB only reads memory at the post-allocation node boundary and model return.
It does not write input bytes, registers or numerical results.
"""
import argparse,concurrent.futures,hashlib,json,math,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS
from capture_network import matrix

def native_script(case,ground=None):
    l,w,t=case['cells'];cl,cw=case['contact_cells'];script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY '+('TRUE' if case['eddy_mode'] else 'FALSE')+'\nSAVEMAT TRUE\n'
    for creator in case['creators']:script+=COMMANDS[creator['id']]+' '+creator['keywords']+'\n'
    ground=case.get('ground','') if ground is None else ground;ground_name=''
    if ground.startswith('@'):ground_name=case['names'][int(ground[1])-1]
    elif ground:
        script+='WIRE '+ground+'\n';name=re.search(r'(?i)(?:^|:)NAME=([^:]+)',ground);ground_name=name[1] if name else 'G'
    suffix=' '+ground_name if ground_name else '';f=case['frequency_ghz']
    return script,suffix,f,ground_name

def tech_text(case,text):
    tech=re.sub(r'(?mi)^\s*eddy\s*=.*$',lambda _:'\n'.join('eddy = '+str(i) for i in case['layers']),text)
    for key,value in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
    return tech

def record(path):
    text=path.read_text();match=re.search(r'\n[-+]?\d\.\d{15}e[-+]\d+\s*\n',text)
    if not match:raise RuntimeError('Missing native three-port record: '+str(path))
    return text[match.start():]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');parser.add_argument('--backing-samples',type=int,default=4);parser.add_argument('--backing-ground',default='NAME=G:LEN=100:WID=40:METAL=M2:XORG=100:YORG=100');parser.add_argument('--cases',type=Path,default=ROOT/'tests/fixtures/three-port-cases.json');parser.add_argument('--technology',type=Path,default=ROOT/'web/technology/CMOS.tek');parser.add_argument('--output-dir',type=Path,default=ROOT/'tests/fixtures');args=parser.parse_args();args.output_dir.mkdir(parents=True,exist_ok=True)
    reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable differs')
    source=args.cases;technology=args.technology;text=technology.read_text();metal_count=len(re.findall(r'(?mi)^\s*<metal>',text));substrate_count=len(re.findall(r'(?mi)^\s*<layer>',text));cases=json.loads(source.read_text())['cases'];base=ROOT/'build/original-three-port';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        script,suffix,f,_=native_script(case)
        for polar in [False,True]:
            for parameter in ['S','Y','Z']:script+='3PORT '+' '.join(case['names'])+f' {f} {f} .1 {parameter} '+('POLAR' if polar else 'RECT')+' '+parameter.lower()+('-polar' if polar else '')+'.out'+suffix+'\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech_text(case,text));(directory/'input.asit').write_text(script)
            p=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(p.stdout);(directory/'stderr.log').write_text(p.stderr)
            if p.returncode:raise RuntimeError(case['name']+': native status '+str(p.returncode))
            cache=next(directory.glob('*.dat')).read_bytes();payload=cache[16+substrate_count*40+metal_count*236:];nx,ny=case['grid']
            if len(payload)!=(metal_count*(metal_count+1)//2)*((nx+1)*(ny+1)+2*nx+2*ny)*16:raise RuntimeError('Unexpected Green payload size')
            checksum=14695981039346656037
            for byte in payload:checksum=((checksum^byte)*1099511628211)&((1<<64)-1)
            model={}
            for i in range(1,4):
                m=re.search(rf'L{i}\s*=\s*(\S+) nH\s+R{i}\s*=\s*(\S+)',p.stdout);model[f'L{i}']=m[1];model[f'R{i}']=m[2]
            for pair in ['12','13','23']:
                m=re.search(rf'M{pair}\s*=\s*(\S+) nH\s+k{pair}\s*=\s*(\S+)\s+Re\(Z{pair}\) =\s*(\S+)',p.stdout);model['M'+pair]=m[1];model['k'+pair]=m[2];model['R'+pair]=m[3]
            data={'records':{key:record(directory/(key+'.out')) for key in ['s','y','z']},'polar_records':{key:record(directory/(key+'-polar.out')) for key in ['s','y','z']},'system_before':matrix(directory/'Abigp.out'),'system_matrix':matrix(directory/'Abig.out'),'model_printed':model,'green_table_hash':f'{checksum:016x}','raw_cache_bytes':len(payload),'raw_cache_sha256':hashlib.sha256(payload).hexdigest(),'quadpack_nonconvergence_messages':p.stdout.count('function did not converge.')//6}
            snapshots.append(data)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(case['name']+': original defined matrices/records vary between fresh processes')
        print(case['name']+': repeated native S/Y/Z, polar, MNA and model outputs identical',flush=True);return {'name':case['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    common={'executable_sha256':ORIGINAL_SHA256,'libm_sha256':hashlib.sha256(Path('/lib/i386-linux-gnu/libm.so.6').read_bytes()).hexdigest(),'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
    document={'reference':'Unmodified original 3PORT, six modes twice in fresh processes per case',**common,'cases':captured};(args.output_dir/'original-three-port.json').write_text(json.dumps(document,indent=2,allow_nan=False)+'\n')
    histories=[];case=cases[0];ground=args.backing_ground
    for sample in range(args.backing_samples):
        directory=work/'backing'/str(sample);directory.mkdir(parents=True);script,suffix,f,ground_name=native_script(case,ground);script+='3PORT '+' '.join(case['names'])+f' {f} {f} .1 Z RECT z.out'+suffix+'\n';(directory/'input.asit').write_text(script);(directory/'CMOS.tek').write_text(tech_text(case,text));(directory/'exit.txt').write_text('EXIT\n')
        gdb='''set pagination off
set confirm off
set disable-randomization off
break *0x8054d70
commands
silent
set $cap=*(unsigned int*)($ebp+8)
set $ind=*(unsigned int*)($ebp+12)
set $n=*(unsigned int*)($ebp+24)
printf "NODE_BLOCKS %u %u %u %u %u %u\\n",$cap,$ind,*(unsigned int*)($ebp-64),*(unsigned int*)($ebp-68),*(unsigned int*)($ebp-60),$n
dump binary memory cap.bin $cap ($cap+$n*$n*16)
dump binary memory ind.bin $ind ($ind+$n*$n*16)
disable 1
continue
end
break *0x8089d58
commands
silent
finish
end
run -g -t CMOS.tek -x input.asit < exit.txt
dump binary memory z.bin 0x80d8c48 0x80d8cd8
dump binary memory model.bin 0x80d8848 0x80d88e8
quit
'''
        (directory/'capture.gdb').write_text(gdb);p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=90,start_new_session=True,preexec_fn=limits);(directory/'gdb.log').write_text(p.stdout+'\n'+p.stderr)
        metadata=re.search(r'NODE_BLOCKS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)',p.stdout)
        if p.returncode or not metadata:raise RuntimeError('Read-only native backing capture failed: '+p.stdout+'\n'+p.stderr)
        numbers=list(map(int,metadata.groups()));z=(directory/'z.bin').read_bytes();model=(directory/'model.bin').read_bytes();canonical=b''.join(z[i*16:i*16+16] for i in reversed(range(9)));addresses=[0x80d8880,0x80d8888,0x80d8890,0x80d8848,0x80d8850,0x80d8898,0x80d88a0,0x80d88a8,0x80d88b0,0x80d88d0,0x80d88d8,0x80d88e0,0x80d88b8,0x80d88c0,0x80d88c8];model_raw=b''.join(model[address-0x80d8848:address-0x80d8848+8] for address in addresses)
        history={'format':'reasitic-three-port-backing-v1','names':case['names'],'ground':ground_name,'frequency_hz':struct.unpack_from('<d',next(directory.glob('*.dat')).read_bytes(),8)[0],'node_count':numbers[5],'addresses':numbers[:5],'capacitance_hex':(directory/'cap.bin').read_bytes().hex(),'inductance_hex':(directory/'ind.bin').read_bytes().hex(),'case':{**case,'ground':ground},'expected_impedance_hex':canonical.hex(),'expected_model_hex':model_raw.hex(),'executable_sha256':ORIGINAL_SHA256,'technology_sha256':common['technology_sha256']}
        (directory/'backing.json').write_text(json.dumps(history,indent=2)+'\n');histories.append(history);print('backing '+str(sample)+': native read-only bytes, alias layout and output globals captured',flush=True)
    (args.output_dir/'original-three-port-backing.json').write_text(json.dumps({'reference':'Read-only GDB capture at native node post-allocation boundary and model return; no numerical writes',**common,'histories':histories},indent=2)+'\n');print('Raw proof:',work)
if __name__=='__main__':main()
