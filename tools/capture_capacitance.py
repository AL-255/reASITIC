"""Capture original CAP matrices, printed totals and all raw Green cache bytes.

Every case runs twice in separate directories/process groups with fresh caches.
"""
import argparse,concurrent.futures,hashlib,json,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
COMMANDS={0:'SQUARE',1:'SPIRAL',2:'WIRE',3:'TRANS',4:'CAPACITOR',5:'BALUN',14:'VIA',16:'SYMSQ',17:'SYMPOLY',18:'MMSQUARE',22:'RING'}
def matrix(path):
    rows=[]
    for line in path.read_text().splitlines():
        values=line.split()
        if len(values)!=4:continue
        i,j=int(values[0])-1,int(values[1])-1
        while len(rows)<=i:rows.append([])
        if len(rows[i])!=j:raise RuntimeError('Unexpected native dump order')
        rows[i].append([float(values[2]),float(values[3])])
    if not rows or any(len(row)!=len(rows) for row in rows):raise RuntimeError('Missing original matrix')
    return rows

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-capacitance.json');args=parser.parse_args()
    reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable differs')
    source=ROOT/'tests/fixtures/capacitance-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases']
    base=ROOT/'build/original-capacitance';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        nx,ny=case['grid'];l,w=case['cells'];tech=re.sub(r'fftx\s*=\s*128',f'fftx = {nx}',text);tech=re.sub(r'ffty\s*=\s*128',f'ffty = {ny}',tech)
        script=f'CELL 1000 1000 1000\nCCELL {l} {w}\nSAVEMAT TRUE\nTEKIO TRUE\n'+COMMANDS[case['id']]+' '+case['keywords']+'\n'
        if case.get('ground'):script+='WIRE '+case['ground']+'\n'
        script+=f'CAP A {case["frequency_ghz"]}'+(' G' if case.get('ground') else '')+'\n';snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script)
            process=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=90,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
            if process.returncode:raise RuntimeError(f'{case["name"]}: original status {process.returncode}')
            cache=next(directory.glob('*.dat')).read_bytes();offset=16+2*40+5*236;payload=cache[offset:];expected=15*((nx+1)*(ny+1)+2*nx+2*ny)*16
            if len(payload)!=expected:raise RuntimeError('Unexpected original cache size')
            checksum=14695981039346656037
            for byte in payload:checksum=((checksum^byte)*1099511628211)&((1<<64)-1)
            totals=re.search(r'Total Capacitance =\s*([-\d.]+) \(fF\)\s*Total Resistance =\s*([-\d.]+)',process.stdout)
            if not totals:raise RuntimeError('Missing original CAP report: '+process.stdout)
            pmat=matrix(directory/'Pmat.out');ymat=matrix(directory/'YC.out')
            data={'frequency_hz':struct.unpack_from('<d',cache,8)[0],'green_table_hash':f'{checksum:016x}','raw_cache_bytes':len(payload),'raw_cache_sha256':hashlib.sha256(payload).hexdigest(),'potential_matrix':pmat,'matrix':ymat,'capacitance_ff_printed':float(totals[1]),'resistance_printed':float(totals[2].rstrip('.'))}
            snapshots.append(data)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(case['name']+': physical outputs vary between fresh processes')
        print(case['name']+': two identical original CAP runs and raw table payloads',flush=True);return {'name':case['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    document={'reference':'Unmodified original ASITIC CAP command, two isolated fresh-cache sessions per case','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'matrix_precision':'Native SAVEMAT %20.10lg; cache payloads are raw binary64','cases':captured}
    args.output.write_text(json.dumps(document,indent=2)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
