"""Capture every creator's raw physical fields from the pinned original binary.

Uses two fresh isolated processes per family. Native pointers, padding and
unwritten scalar/mesh words are deliberately excluded from physical evidence.
"""
import argparse,concurrent.futures,hashlib,json,os,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256,physical_geometry

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re')
    parser.add_argument('--library-path');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-creator-binary.json')
    args=parser.parse_args();reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable hash differs')
    source=ROOT/'tests/fixtures/original-creators.json';technology=ROOT/'web/technology/CMOS.tek'
    cases=json.loads(source.read_text())['fixtures'];base=ROOT/'build/original-creators';base.mkdir(parents=True,exist_ok=True)
    work=Path(tempfile.mkdtemp(prefix='capture-',dir=base));env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None)
    env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        command=case['script'].splitlines()[2];script='CELL 1000 1000 1000\nCCELL 1000 1000\n'+command+'\nBSAVE raw\n';snapshots=[]
        for repeat in range(2):
            directory=work/case['family']/str(repeat);directory.mkdir(parents=True);(directory/'input.asit').write_text(script)
            process=subprocess.run([str(binary),'-g','-t',str(technology),'-x','input.asit'],cwd=directory,env=env,input=case['stdin'],text=True,capture_output=True,timeout=60,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
            if process.returncode!=0:raise RuntimeError(f'{case["family"]}: original status {process.returncode}: {process.stderr}')
            shapes={}
            for path in directory.glob('raw.*'):
                data=path.read_bytes();offset=20
                while offset+4<=len(data) and struct.unpack_from('<I',data,offset)[0]==0x88:
                    begin=offset;name=data[offset+4:offset+84].split(b'\0')[0].decode();offset+=4+0xbc
                    while offset+4<=len(data) and struct.unpack_from('<I',data,offset)[0] in (0x89,0x8a):offset+=4+0xf0
                    shapes[name]=physical_geometry(data[:20]+data[begin:offset]+struct.pack('<I',0x8b))
            expected={s['fields']['NAME'] for s in case['shapes']}
            if set(shapes)!=expected:raise RuntimeError(f'{case["family"]}: missing binary shapes: {shapes.keys()} != {expected}; {process.stdout}')
            snapshots.append(shapes)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(f'{case["family"]}: physical fields vary between fresh runs')
        print(case['family']+': two identical original raw physical captures',flush=True)
        return {'family':case['family'],'command':command,'answers':case['stdin'].removesuffix('EXIT\n'),'shapes':snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:captured=list(pool.map(capture,cases))
    document={'reference':'Two fresh isolated runs of the unmodified original executable per creator family','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'geometry_precision':'Raw BSAVE binary64; defined physical fields only','cases':captured}
    args.output.write_text(json.dumps(document,indent=2)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
