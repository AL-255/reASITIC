"""Capture MAXNW counts and LMAT output from repeated unmodified sessions."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256,matrix

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re')
    parser.add_argument('--library-path');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    technology=ROOT/'web/technology/CMOS.tek';definitions=[]
    for maximum in [3,4,5,8,10,500]:
        definitions.append(dict(name='graded-'+str(maximum),max_width_cells=maximum,width=24,width_cell=1,thickness_cell=1,orientation=0))
    definitions.extend([dict(name='below-threshold',max_width_cells=3,width=2.9,width_cell=1,thickness_cell=1,orientation=0),dict(name='above-threshold',max_width_cells=3,width=3.1,width_cell=1,thickness_cell=1,orientation=0),dict(name='rotated-layers',max_width_cells=5,width=24,width_cell=1,thickness_cell=.25,orientation=37)])
    base=ROOT/'build/original-maxnw';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None)
    env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        keywords=f"NAME=A:LEN=40:WID={case['width']}:METAL=M5:XORG=100:YORG=100:ORIENT={case['orientation']}"
        script=f"EDDY FALSE\nCELL 1000 {case['width_cell']} {case['thickness_cell']}\nCCELL 1000 1000\nMAXNW {case['max_width_cells']}\nWIRE {keywords}\nLMAT A 1 lmat.out\n"
        snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'input.asit').write_text(script)
            p=subprocess.run([str(binary),'-g','-t',str(technology),'-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=90,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(p.stdout);(directory/'stderr.log').write_text(p.stderr)
            if p.returncode:raise RuntimeError(case['name']+': '+p.stdout+p.stderr)
            counts=re.findall(r'Generating inductance matrix \((\d+)x(\d+)\)',p.stdout);assert len(counts)==1
            snapshots.append(dict(filaments=int(counts[0][0]),matrix=matrix(directory/'lmat.out')))
        assert snapshots[0]==snapshots[1],case['name']+' original repeats differ'
        print(case['name']+': '+str(snapshots[0]['filaments'])+' filaments; raw LMAT repeats exactly',flush=True)
        return dict(**case,keywords=keywords,**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:cases=list(pool.map(capture,definitions))
    output=ROOT/'tests/fixtures/original-maxnw.json';output.write_text(json.dumps(dict(reference='Unmodified MAXNW/WIRE/LMAT in two fresh processes per case; original full-precision LMAT matrix files',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),cases=cases),indent=2)+'\n');print(output);print('Raw proof:',work)

if __name__=='__main__':main()
