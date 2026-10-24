"""Capture complete edited physical geometry, including parallel branches."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256,matrix
from capture_eddy import COMMANDS

def physical_layout(data):
    offset=20;shapes=[]
    while offset+4<=len(data):
        marker=struct.unpack_from('<I',data,offset)[0];offset+=4
        if marker==0x8b:break
        if marker!=0x88:raise ValueError('Expected original shape marker')
        raw=data[offset:offset+0xbc];offset+=0xbc
        shape=dict(name=raw[:80].split(b'\0')[0].decode(),origin=list(struct.unpack_from('<2d',raw,0x54)),segments=[])
        if raw[0xb8]:shape['hidden']=True
        while offset+4<=len(data):
            marker=struct.unpack_from('<I',data,offset)[0]
            if marker not in (0x89,0x8a):break
            offset+=4;raw=data[offset:offset+0xf0];offset+=0xf0;metal,nx,ny=struct.unpack_from('<3i',raw,0xdc)
            segment=dict(metal=metal,width=struct.unpack_from('<d',raw,0xcc)[0],points=[list(struct.unpack_from('<3d',raw,i)) for i in (0,24,0x44,0x5c,0x88,0xa0)])
            if metal<5:segment['thickness']=struct.unpack_from('<d',raw,0xd4)[0]
            else:segment.update(nx=nx,ny=ny)
            if marker==0x89:shape['segments'].append(segment)
            else:shape['segments'][-1].setdefault('branches',[]).append(segment)
        shapes.append(shape)
    return shapes

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/geometry-edit-cases.json';technology=ROOT/'web/technology/CMOS.tek';cases=json.loads(source.read_text())['cases']
    base=ROOT/'build/original-geometry-edits';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        c=case['creator'];script=COMMANDS[c['id']]+' '+c['keywords']+'\n'+case['commands']+'\nBSAVE raw\n';snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'input.asit').write_text(script)
            p=subprocess.run([str(binary),'-g','-t',str(technology),'-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=90,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(p.stdout);(directory/'stderr.log').write_text(p.stderr)
            if p.returncode:raise RuntimeError(case['name']+': '+p.stdout+p.stderr)
            files=list(directory.glob('raw.*'));assert len(files)==1
            shapes=physical_layout(files[0].read_bytes());eligible=[s for s in shapes if any(p['metal']<5 for p in s['segments'])]
            solve_script=script+'EDDY FALSE\nCELL 1000 1000 1000\nCCELL 1000 1000\n'
            for i,s in enumerate(eligible):solve_script+=f"LMAT {s['name']} 1 solve-{i}.out\n"
            (directory/'matrix.asit').write_text(solve_script)
            q=subprocess.run([str(binary),'-g','-t',str(technology),'-x','matrix.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=90,start_new_session=True,preexec_fn=limits)
            (directory/'matrix-stdout.log').write_text(q.stdout);(directory/'matrix-stderr.log').write_text(q.stderr)
            if q.returncode:raise RuntimeError(case['name']+': '+q.stdout+q.stderr)
            counts=re.findall(r'Generating inductance matrix \((\d+)x(\d+)\)',q.stdout);assert len(counts)==len(eligible)
            matrices={s['name']:dict(filaments=int(counts[i][0]),matrix=matrix(directory/f'solve-{i}.out')) for i,s in enumerate(eligible)}
            snapshots.append(dict(shapes=shapes,matrices=matrices))
        assert snapshots[0]==snapshots[1],case['name']+' original physical edits differ between runs'
        print(case['name']+': '+str(len(snapshots[0]['shapes']))+' shapes; physical records and LMAT files repeat exactly',flush=True)
        return dict(name=case['name'],**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-geometry-edits.json';output.write_text(json.dumps(dict(reference='Unmodified original geometry edit batches in two fresh processes; defined raw BSAVE physical fields and parallel branches',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),cases=captured),indent=2)+'\n');print(output);print('Raw proof:',work)

if __name__=='__main__':main()
