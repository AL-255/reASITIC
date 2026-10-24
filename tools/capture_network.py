"""Repeat original PI2 commands and retain stage matrices and pi reports."""
import argparse,concurrent.futures,hashlib,json,math,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS

def matrix(path):
    rows=[]
    for line in path.read_text().splitlines():
        fields=line.split()
        if len(fields)!=4:continue
        i,j=int(fields[0])-1,int(fields[1])-1
        while len(rows)<=i:rows.append([])
        if len(rows[i])!=j:raise RuntimeError('Unexpected matrix order: '+str(path))
        row=[]
        for token in fields[2:]:
            value=float(token);row.append(value if math.isfinite(value) else token.lower())
        rows[i].append(row)
    if not rows or any(len(row)!=len(rows) for row in rows):raise RuntimeError('Incomplete matrix: '+str(path))
    return rows

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-network.json');args=parser.parse_args()
    reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable differs')
    source=ROOT/'tests/fixtures/network-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases'];base=ROOT/'build/original-network';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        tech=re.sub(r'(?mi)^\s*eddy\s*=.*$',lambda _: '\n'.join('eddy = '+str(i) for i in case['layers']),text)
        for key,value in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        l,w,t=case['cells'];cl,cw=case['contact_cells'];ground=' A' if case.get('self_ground') else ' G' if case.get('ground') else ''
        script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY '+('TRUE' if case['eddy_mode'] else 'FALSE')+'\nSAVEMAT TRUE\n'+COMMANDS[case['id']]+' '+case['keywords']+'\n'
        if case.get('ground'):script+='WIRE '+case['ground']+'\n'
        script+=f'PI2 A {case["frequency_ghz"]}'+ground+'\nZIN S\nZIN X\nZIN D\nSHUNTR S\nSHUNTR D\n';snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script)
            process=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
            if process.returncode:raise RuntimeError(case['name']+': original status '+str(process.returncode)+': '+process.stderr)
            quality=re.search(r'Pi Model at f=(\S+) GHz:  Q = ([^\n]+)',process.stdout)
            report=re.search(r'L  =([^\n]+)\nCs1=([^\n]+)\nCs2=([^\n]+)',process.stdout)
            if not quality or not report:raise RuntimeError('Missing pi report: '+process.stdout)
            numeric=lambda value:re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?|[-+]?nan|[-+]?inf',value,re.I)
            cache=next(directory.glob('*.dat')).read_bytes();payload=cache[16+2*40+5*236:]
            nx,ny=case['grid']
            if len(payload)!=15*((nx+1)*(ny+1)+2*nx+2*ny)*16:raise RuntimeError('Unexpected Green payload size')
            checksum=14695981039346656037
            for byte in payload:checksum=((checksum^byte)*1099511628211)&((1<<64)-1)
            data={'green_table_hash':f'{checksum:016x}','raw_cache_bytes':len(payload),'raw_cache_sha256':hashlib.sha256(payload).hexdigest(),'capacitance_matrix':matrix(directory/'ZC.out'),'inductance_admittance_matrix':matrix(directory/'YM.out'),'node_capacitance_matrix':matrix(directory/'Ycap.out'),
                'quality_printed':numeric(quality[2]),'pi_printed':[re.search(pattern,process.stdout)[1].strip() for pattern in [r'L  =\s*(\S+) nH',r'R  =([^\n]+)',r'Cs1=\s*(\S+) fF',r'Rs1=([^\n]+)',r'Cs2=\s*(\S+) fF',r'Rs2=([^\t]+)',r'f_res = ([^G]+)GHz']],
                'input_impedance_printed':[numeric(line) for line in re.findall(r'Zin = ([^\n]+)',process.stdout)],
                'shunt_printed':re.findall(r'The shunt resistance is ([^\n]+)',process.stdout),
                'quadpack_nonconvergence_messages':process.stdout.count('function did not converge.'),
                'stage_sha256':{name:hashlib.sha256((directory/name).read_bytes()).hexdigest() for name in ['ZC.out','YM.out','Ycap.out']}}
            snapshots.append(data)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(case['name']+': outputs differ between fresh original processes')
        print(case['name']+': two identical whole-program network solves',flush=True);return {'name':case['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    document={'reference':'Unmodified original PI2, ZIN and SHUNTR commands, two fresh processes per case','executable_sha256':ORIGINAL_SHA256,'libm_sha256':hashlib.sha256(Path('/lib/i386-linux-gnu/libm.so.6').read_bytes()).hexdigest(),'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'matrix_precision':'Native matrix snapshots %19.10g','cases':captured}
    args.output.write_text(json.dumps(document,indent=2)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
