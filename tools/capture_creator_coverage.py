"""Capture creator parameter/process variants and their original DC/LMAT results."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256,matrix
from capture_geometry_edits import physical_layout


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/creator-coverage-cases.json';cases=json.loads(source.read_text())['cases'];technologies={name:(ROOT/'web/technology'/(name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
    base=ROOT/'build/original-creator-coverage';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        tech=technologies[case['technology']];metals=len(re.findall(r'(?im)^\s*<metal>',tech));snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'process.tek').write_text(tech)
            script='EDDY FALSE\nCELL 1000 1000 1000\nCCELL 1000 1000\n'+case['command']+'\nBSAVE raw\n'+''.join('RESIS '+name+'\n' for name in case['names']);(directory/'input.asit').write_text(script)
            p=subprocess.run([str(binary),'-g','-t','process.tek','-x','input.asit'],cwd=directory,env=env,input=case['answers']+'EXIT\n',capture_output=True,text=True,timeout=30,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(p.stdout);(directory/'stderr.log').write_text(p.stderr)
            if p.returncode not in (0,-11):raise RuntimeError(case['name']+': original status '+str(p.returncode))
            files=list(directory.glob('raw.*'))
            if len(files)!=1:raise RuntimeError(case['name']+': missing original geometry: '+p.stdout+((directory/'asitic.LOG').read_text() if (directory/'asitic.LOG').exists() else ''))
            shapes=physical_layout(files[0].read_bytes(),metals);assert sorted(s['name'] for s in shapes)==sorted(case['names']),(case['name'],[s['name'] for s in shapes])
            log=p.stdout+((directory/'asitic.LOG').read_text() if (directory/'asitic.LOG').exists() else '')
            assert 'Error:' not in log and all(s['segments'] for s in shapes),case['name']+' original rejected the requested structure'
            dc=dict(re.findall(r'Resistance of (\S+) =\s*([-+\d.eE]+) \(Ohms\)',log));assert set(dc)==set(case['names']),(case['name'],dc)
            if p.returncode:assert 'EXIT' in log,case['name']+' failed before completing the requested outputs'
            eligible=[s['name'] for s in shapes if any(segment['metal']<metals for segment in s['segments'])]
            # Reload the actual original BSAVE in a fresh process. This keeps
            # creator heap corruption out of subsequent numerical allocations;
            # neither the saved records nor the native solver are modified.
            solve='EDDY FALSE\nCELL 1000 1000 1000\nCCELL 1000 1000\nBLOAD raw\nBSAVE reloaded\n'+''.join(f'LMAT {name} 1 matrix-{i}.out\n' for i,name in enumerate(eligible));(directory/'solve.asit').write_text(solve)
            q=subprocess.run([str(binary),'-g','-t','process.tek','-x','solve.asit'],cwd=directory,env=env,input='EXIT\n',capture_output=True,text=True,timeout=60,start_new_session=True,preexec_fn=limits)
            (directory/'solve-stdout.log').write_text(q.stdout);(directory/'solve-stderr.log').write_text(q.stderr)
            if q.returncode:raise RuntimeError(case['name']+': LMAT original status '+str(q.returncode))
            loaded=list(directory.glob('reloaded.*'));assert len(loaded)==1
            assert sorted(physical_layout(loaded[0].read_bytes(),metals),key=lambda s:s['name'])==sorted(shapes,key=lambda s:s['name']),case['name']+' original BSAVE/BLOAD changed physical geometry'
            matrices={name:matrix(directory/f'matrix-{i}.out') for i,name in enumerate(eligible)}
            snapshots.append(dict(shapes=shapes,resistance_dc_printed=dc,matrices=matrices,creator_exit_status=p.returncode))
        assert json.dumps(snapshots[0],sort_keys=True,allow_nan=False)==json.dumps(snapshots[1],sort_keys=True,allow_nan=False),case['name']+' original results vary across runs'
        print(case['name']+': raw geometry, DC and '+str(len(snapshots[0]['matrices']))+' LMAT outputs repeat exactly',flush=True)
        return dict(name=case['name'],technology=case['technology'],metal_count=metals,**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/'tests/fixtures/original-creator-coverage.json';output.write_text(json.dumps(dict(reference='Two fresh unmodified original geometry/DC runs and two separate original BSAVE/BLOAD/LMAT runs per case; defined physical fields and native printed matrices; post-output creator exits retained',executable_sha256=ORIGINAL_SHA256,case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},cases=captured),indent=2)+'\n');print(output);print('Raw proof:',work)


if __name__=='__main__':main()
