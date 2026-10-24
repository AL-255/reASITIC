"""Capture original creators beyond former web turn and side/count ceilings."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256,matrix
from capture_geometry_edits import physical_layout

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--counts',action='store_true');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    proof='creator-counts' if args.counts else 'creator-ranges';definitions='creator-count' if args.counts else 'creator-range'
    source=ROOT/('tests/fixtures/'+definitions+'-cases.json');cases=json.loads(source.read_text())['cases'];technologies={name:(ROOT/'web/technology'/(name+'.tek')).read_text() for name in ['CMOS','BiCMOS']}
    base=ROOT/('build/original-'+proof);base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        # Allow large LMAT files while bounding malformed prompt transcripts.
        resource.setrlimit(resource.RLIMIT_FSIZE,(64*1024*1024,64*1024*1024))
    def run(directory,label,script,stdin):
        (directory/(label+'.asit')).write_text(script)
        (directory/(label+'-stdin.txt')).write_text(stdin)
        with (directory/(label+'-stdout.log')).open('w') as out,(directory/(label+'-stderr.log')).open('w') as err:
            p=subprocess.run([str(binary),'-g','-t','process.tek','-x',label+'.asit'],cwd=directory,env=env,input=stdin,text=True,stdout=out,stderr=err,timeout=30,start_new_session=True,preexec_fn=limits)
        native=(directory/'asitic.LOG').read_text() if (directory/'asitic.LOG').exists() else ''
        (directory/(label+'-native.log')).write_text(native)
        log=(directory/(label+'-stdout.log')).read_text()+native
        assert p.returncode in (0,-11) and 'Error:' not in log,(directory,p.returncode,log[-2000:])
        if p.returncode:assert 'EXIT' in log,(directory,'Failure before exit')
        return p.returncode,log
    def capture(case):
        tech=technologies[case['technology']];metals=len(re.findall(r'(?im)^\s*<metal>',tech));snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'process.tek').write_text(tech)
            status,log=run(directory,'creator',case['command']+'\n'+case.get('edits','')+'\nBSAVE raw\n'+''.join('RESIS '+name+'\n' for name in case['names']),case['answers']+'EXIT\n')
            files=list(directory.glob('raw.*'));assert len(files)==1,(case['name'],'Missing BSAVE')
            shapes=physical_layout(files[0].read_bytes(),metals);assert sorted(s['name'] for s in shapes)==sorted(case['names'])
            assert all(s['segments'] for s in shapes)
            dc=dict(re.findall(r'Resistance of (\S+) =\s*([-+\d.eE]+) \(Ohms\)',log));assert set(dc)==set(case['names'])
            # Fresh original BLOAD checks the saved physical fields separately
            # from creator allocation history. No recovered source is changed.
            eligible=case.get('matrix_names',[])
            run(directory,'reload','EDDY FALSE\nCELL 1000 1000 1000\nCCELL 1000 1000\nBLOAD raw\nBSAVE reloaded\n'+''.join(f'LMAT {name} 1 matrix-{i}.out\n' for i,name in enumerate(eligible)),'EXIT\n')
            loaded=list(directory.glob('reloaded.*'));assert len(loaded)==1
            assert sorted(physical_layout(loaded[0].read_bytes(),metals),key=lambda s:s['name'])==sorted(shapes,key=lambda s:s['name'])
            snapshots.append(dict(shapes=shapes,resistance_dc_printed=dc,creator_exit_status=status,matrices={name:matrix(directory/f'matrix-{i}.out') for i,name in enumerate(eligible)}))
        assert snapshots[0]==snapshots[1],case['name']+' original outputs vary'
        print(case['name']+': '+str(sum(len(s['segments']) for s in shapes))+' physical segments repeat across two original creators and fresh reloads',flush=True)
        return dict(name=case['name'],technology=case['technology'],metal_count=metals,**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:captured=list(pool.map(capture,cases))
    output=ROOT/('tests/fixtures/original-'+proof+'.json');output.write_text(json.dumps(dict(reference='Two fresh unmodified original creator/BSAVE/DC processes and two fresh BLOAD/BSAVE processes per case; selected LMAT matrices from the reloads',executable_sha256=ORIGINAL_SHA256,case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},cases=captured),separators=(',',':'),allow_nan=False)+'\n')
    print(output);print('Raw proof:',work)
if __name__=='__main__':main()
