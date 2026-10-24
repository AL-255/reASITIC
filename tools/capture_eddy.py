"""Capture original EDDY ON LMAT results in repeated isolated whole processes."""
import argparse,concurrent.futures,hashlib,json,math,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
COMMANDS={0:'SQUARE',1:'SPIRAL',2:'WIRE',3:'TRANS',16:'SYMSQ',17:'SYMPOLY',18:'MMSQUARE',22:'RING'}
def matrix(path):
    rows=[];nonfinite=False
    for line in path.read_text().splitlines():
        fields=line.split()
        if len(fields)!=4:continue
        i,j=int(fields[0]),int(fields[1]);values=[]
        for token in fields[2:]:
            value=float(token)
            if not math.isfinite(value):nonfinite=True;value='nan' if math.isnan(value) else 'inf' if value>0 else '-inf'
            values.append(value)
        while len(rows)<=i:rows.append([])
        if len(rows[i])!=j:raise RuntimeError('Unexpected original matrix order')
        rows[i].append(values)
    if not rows or any(len(row)!=len(rows) for row in rows):raise RuntimeError('Incomplete original matrix')
    return rows,nonfinite
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-eddy.json');args=parser.parse_args()
    reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable differs')
    source=ROOT/'tests/fixtures/eddy-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases'];base=ROOT/'build/original-eddy';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        tech=re.sub(r'(?m)^\s*eddy\s*=.*$',lambda _: '\n'.join('eddy = '+str(i) for i in case['layers']),text);l,w,t,c=case['cells'];name=case.get('winding_name','A')
        script=f'CELL {l} {w} {t}\nCCELL {c} 1000\nEDDY TRUE\n'+COMMANDS[case['id']]+' '+case['keywords']+f'\nLMAT {name} {case["frequency_ghz"]} matrix.out\n';snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script)
            process=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=90,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
            if process.returncode:raise RuntimeError(f'{case["name"]}: original status {process.returncode}: {process.stderr}')
            count=re.search(r'Generating inductance matrix \((\d+)x(\d+)\)',process.stdout)
            if not count:raise RuntimeError('Missing original filament matrix: '+process.stdout)
            values,nonfinite=matrix(directory/'matrix.out')
            data={'filaments':int(count[1]),'matrix':values,'nonfinite':nonfinite,'matrix_file_sha256':hashlib.sha256((directory/'matrix.out').read_bytes()).hexdigest(),'quadpack_nonconvergence_messages':process.stdout.count('function did not converge.')}
            snapshots.append(data)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(case['name']+': outputs differ between fresh original processes')
        print(case['name']+': two identical whole-program eddy solves',flush=True);return {'name':case['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    document={'reference':'Unmodified original ASITIC EDDY TRUE and LMAT commands, two isolated fresh processes per case','executable_sha256':ORIGINAL_SHA256,'libm_sha256':hashlib.sha256(Path('/lib/i386-linux-gnu/libm.so.6').read_bytes()).hexdigest(),'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'matrix_precision':'Native LMAT %10.20lf','cases':captured};args.output.write_text(json.dumps(document,indent=2)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
