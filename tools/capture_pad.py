"""Capture PI4 from the unchanged executable, including read-only raw binary64 observations."""
import argparse, concurrent.futures, hashlib, json, os, re, resource, subprocess, tempfile
from pathlib import Path
from capture_lmat import ROOT, ORIGINAL_SHA256
from capture_eddy import COMMANDS
from capture_network import matrix

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re')
    parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-pad.json')
    args=parser.parse_args();reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable differs')
    source=ROOT/'tests/fixtures/pad-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases']
    base=ROOT/'build/original-pad';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None)
    env['LD_LIBRARY_PATH']='/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        tech=re.sub(r'(?mi)^\s*eddy\s*=.*$',lambda _: '\n'.join('eddy = '+str(i) for i in case['layers']),text)
        for key,value in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        l,w,t=case['cells'];cl,cw=case['contact_cells'];g=case.get('ground','');ground=' '+({'@self':'A','@1':'P1','@2':'P2'}.get(g,'G')) if g else ''
        script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY '+('TRUE' if case['eddy_mode'] else 'FALSE')+'\nSAVEMAT TRUE\n'
        for creator in [case['device'],*case['pads']]:script+=COMMANDS[creator['id']]+' '+creator['keywords']+'\n'
        if g and not g.startswith('@'):script+='WIRE '+g+'\n'
        script+=f'PI4 A {case["frequency_ghz"]} P1 P2'+ground+'\nZIN S\nZIN X\nZIN D\nSHUNTR S\nSHUNTR D\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True)
            (directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'exit.txt').write_text('EXIT\n')
            process=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
            if process.returncode:raise RuntimeError(case['name']+': original status '+str(process.returncode))
            cache=next(directory.glob('*.dat')).read_bytes();payload=cache[16+2*40+5*236:];nx,ny=case['grid']
            if len(payload)!=15*((nx+1)*(ny+1)+2*nx+2*ny)*16:raise RuntimeError('Unexpected Green payload size')
            checksum=14695981039346656037
            for byte in payload:checksum=((checksum^byte)*1099511628211)&((1<<64)-1)
            numeric=lambda value:re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?|[-+]?nan|[-+]?inf',value,re.I)
            quality=re.findall(r'Pi Model at f=(\S+) GHz:  Q = ([^\n]+)',process.stdout)[-1]
            data={'green_table_hash':f'{checksum:016x}','raw_cache_bytes':len(payload),'raw_cache_sha256':hashlib.sha256(payload).hexdigest(),
                  'capacitance_matrix':matrix(directory/'ZC.out'),'inductance_admittance_matrix':matrix(directory/'YM.out'),'node_capacitance_matrix':matrix(directory/'Ycap.out'),'pad_capacitance_matrix':matrix(directory/'YC.out'),
                  'quality_printed':numeric(quality[1]),'pi_printed':[re.findall(pattern,process.stdout)[-1].strip() for pattern in [r'L  =\s*(\S+) nH',r'R  =([^\n]+)',r'Cs1=\s*(\S+) fF',r'Rs1=([^\n]+)',r'Cs2=\s*(\S+) fF',r'Rs2=([^\t]+)',r'f_res = ([^G]+)GHz']],
                  'input_impedance_printed':[numeric(line) for line in re.findall(r'Zin = ([^\n]+)',process.stdout)],'shunt_printed':re.findall(r'The shunt resistance is ([^\n]+)',process.stdout),
                  'quadpack_nonconvergence_messages':process.stdout.count('function did not converge.')}
            # Observe native matrices before in-place stamping/subtraction and
            # globals at final return. No numerical memory/register writes.
            gdb='''set pagination off
set confirm off
set disable-randomization off
break *0x8053e00
commands
silent
set $c=*(unsigned int*)($esp+4)
set $z=*(unsigned int*)($esp+8)
set $n=*(unsigned int*)($esp+12)
printf "PI4_GROUPS %u\\n",$n
dump binary memory cap.bin $c ($c+$n*$n*16)
dump binary memory ind.bin $z ($z+$n*$n*16)
continue
end
break *0x8050f9c
commands
silent
dump binary memory inner1.bin 0x80d8da8 0x80d8dc8
dump binary memory inner2.bin 0x80d8dd8 0x80d8df8
continue
end
break *0x8051048
commands
silent
set $v=$ebp-236
set $d=*(unsigned int*)$v
set $n=*(unsigned int*)($v+12)
printf "PAD_GROUPS %u\\n",$n
dump binary memory pad.bin $d ($d+$n*$n*16)
continue
end
break *0x8051275
run -g -t CMOS.tek -x input.asit < exit.txt
dump binary memory y1.bin 0x80d8da8 0x80d8dc8
dump binary memory y2.bin 0x80d8dd8 0x80d8df8
dump binary memory pi.bin 0x80d8820 0x80d8880
quit
'''
            (directory/'capture.gdb').write_text(gdb)
            p=subprocess.run(['gdb','-q','-batch','-x','capture.gdb','--args',str(binary)],cwd=directory,env=env,text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits)
            (directory/'gdb.log').write_text(p.stdout+'\n'+p.stderr)
            if p.returncode or 'PI4_GROUPS' not in p.stdout:raise RuntimeError(case['name']+': GDB capture failed '+p.stdout+p.stderr)
            data['raw_hex']={key:(directory/file).read_bytes().hex() for key,file in [('capacitance_matrix','cap.bin'),('inductance_admittance_matrix','ind.bin'),('pad_capacitance_matrix','pad.bin'),('pi','pi.bin')]}
            data['raw_hex']['matrix']=((directory/'y1.bin').read_bytes()+(directory/'y2.bin').read_bytes()).hex()
            data['raw_hex']['with_pad_admittance_matrix']=((directory/'inner1.bin').read_bytes()+(directory/'inner2.bin').read_bytes()).hex()
            data['groups']=int(re.search(r'PI4_GROUPS (\d+)',p.stdout)[1]);data['pad_groups']=int(re.search(r'PAD_GROUPS (\d+)',p.stdout)[1])
            snapshots.append(data)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(case['name']+': original observations differ between fresh processes')
        print(case['name']+': two fresh PI4 runs and two read-only raw captures match',flush=True);return {'name':case['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    document={'reference':'Unmodified PI4, ZIN and SHUNTR commands twice per case; read-only GDB stage, admittance and pi captures twice per case','executable_sha256':ORIGINAL_SHA256,'libm_sha256':hashlib.sha256(Path('/lib/i386-linux-gnu/libm.so.6').read_bytes()).hexdigest(),'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'raw_matrix_order':'Column major for capacitance, inductance and pad capacitance; row major for both admittances; contiguous PiEquivalent fields','cases':captured}
    args.output.write_text(json.dumps(document,indent=2,allow_nan=False)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
