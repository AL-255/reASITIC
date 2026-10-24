"""Compare original LF Q, LMAT conductor Q and direct PI2 port Q.

Each frequency runs twice in a fresh, unmodified original process. No fit,
processor profile, or expected peak is supplied to the numerical model.
"""
import argparse, concurrent.futures, hashlib, json, math, os, re, resource, struct, subprocess, tempfile
from pathlib import Path
from capture_lmat import ROOT, ORIGINAL_SHA256, matrix as lmat_matrix
from capture_network import matrix

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--cases',type=Path,default=ROOT/'tests/fixtures/quality-sweep-cases.json');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-quality-sweep.json');args=parser.parse_args()
    binary=Path.home()/'github/asitic-re/run/asitic.linux.2.2'
    assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=args.cases;definition=json.loads(source.read_text())
    technologies={c['technology']:(ROOT/'web/technology'/(c['technology']+'.tek')).read_text() for c in definition['cases']}
    base=ROOT/'build/original-quality-sweep';base.mkdir(exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(binary.parent/'libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(task):
        case,frequency=task;text=technologies[case['technology']]
        for key,value in zip(['fftx','ffty'],definition['grid']):text=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),text)
        cells=' '.join(map(str,definition['cells']));contacts=' '.join(map(str,definition['contact_cells']))
        script=f'CELL {cells}\nCCELL {contacts}\nEDDY FALSE\nSAVEMAT TRUE\n'+case['command']+f'\nQ A {frequency}\nLMAT A {frequency} lmat.out\nPI2 A {frequency}\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(frequency)/str(repeat);directory.mkdir(parents=True);(directory/'process.tek').write_text(text);(directory/'input.asit').write_text(script)
            p=subprocess.run([str(binary),'-g','-t','process.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(p.stdout);(directory/'stderr.log').write_text(p.stderr);assert p.returncode==0,(case['name'],frequency,p.returncode)
            q=re.search(r'Pi Model at f=\S+ GHz:\s+Q = ([^\n]+)',p.stdout);assert q,p.stdout
            values=[float(v) for v in q[1].split(',')];assert len(values)==3 and all(map(math.isfinite,values))
            lmat=lmat_matrix(directory/'lmat.out');impedance=[sum(z[k] for row in lmat for z in row) for k in range(2)]
            saved_frequency=struct.unpack_from('<d',next(directory.glob('*.dat')).read_bytes(),8)[0]
            snapshots.append(dict(frequency_ghz=frequency,frequency_hz=saved_frequency,quality_low_frequency_printed=float(re.search(r'LF Extrapolated Q of A = (\S+)\.',p.stdout)[1]),series_impedance=impedance,series_q=impedance[1]/impedance[0],quality_printed=values,stages={key:matrix(directory/name) for key,name in [('capacitance_matrix','ZC.out'),('inductance_admittance_matrix','YM.out'),('node_capacitance_matrix','Ycap.out')]},lmat_sha256=hashlib.sha256((directory/'lmat.out').read_bytes()).hexdigest()))
        assert snapshots[0]==snapshots[1],(case['name'],frequency,'original outputs vary')
        print(case['name'],frequency,'GHz: two matching original LF/LMAT/PI2 results',flush=True);return case['name'],snapshots[0]
    tasks=[(c,f) for c in definition['cases'] for f in definition['frequencies_ghz']]
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,tasks))
    document=dict(reference=__doc__,executable_sha256=ORIGINAL_SHA256,case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),technology_sha256={name:hashlib.sha256(text.encode()).hexdigest() for name,text in technologies.items()},cases=[dict(name=c['name'],samples=[sample for name,sample in captured if name==c['name']]) for c in definition['cases']])
    output=args.output;output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(document,indent=2)+'\n');print(output);print('Raw proof:',work)
if __name__=='__main__':main()
