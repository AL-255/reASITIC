"""Capture original 512-grid thin-wire solves and the collapsed 256-grid case."""
import argparse,hashlib,json,math,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_network import matrix

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');a=p.parse_args()
    binary=a.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    definitions=ROOT/'tests/fixtures/fft-budget-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(definitions.read_text())['cases'];work=Path(tempfile.mkdtemp(prefix='fft-budget-',dir=ROOT/'build'))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(a.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(128*1024*1024,128*1024*1024))
    def run(c,nx,ny,repeat):
        d=work/f'{c["model"]}-{nx}x{ny}-{repeat}';d.mkdir();tech=text
        for key,value in [('fftx',nx),('ffty',ny)]:tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        script='EDDY FALSE\nCELL '+' '.join(map(str,c['cells']))+'\nCCELL '+' '.join(map(str,c['contact_cells']))+'\nSAVEMAT TRUE\nWIRE '+c['keywords']+'\n'+c['model']+' A '+str(c['frequency_ghz'])+'\n'
        if c['model']=='PI2':script+='ZIN S\nZIN X\nZIN D\nSHUNTR S\nSHUNTR D\n'
        (d/'process.tek').write_text(tech);(d/'input.asit').write_text(script)
        with (d/'stdout.log').open('w') as out,(d/'stderr.log').open('w') as err:
            r=subprocess.run([str(binary),'-g','-t','process.tek','-x','input.asit'],cwd=d,env=env,input='EXIT\n',text=True,stdout=out,stderr=err,start_new_session=True,preexec_fn=limits,timeout=180)
        assert r.returncode==0,(d,r.returncode)
        return d,(d/'stdout.log').read_text()
    collapsed=[]
    for repeat in range(2):
        d,log=run(cases[0],256,256,repeat);assert re.search(r'Total Capacitance =\s*-?nan',log,re.I)
        collapsed.append({'returncode':0,'finite_capacitance':False,'reported_capacitance':re.search(r'Total Capacitance =([^\n]+)',log)[1].strip()})
    assert collapsed[0]==collapsed[1]
    captured=[];hashes={}
    for c in cases:
        nx,ny=c['grid'];snapshots=[]
        for repeat in range(2):
            d,log=run(c,nx,ny,repeat);assert 'Error:' not in log
            raw=next(d.glob('*.dat')).read_bytes();payload=raw[16+2*40+5*236:];assert len(payload)==15*((nx+1)*(ny+1)+2*nx+2*ny)*16;sha=hashlib.sha256(payload).hexdigest()
            if sha not in hashes:
                fnv=14695981039346656037
                for b in payload:fnv=((fnv^b)*1099511628211)&((1<<64)-1)
                hashes[sha]=f'{fnv:016x}'
            names=['Pmat.out','YC.out'] if c['model']=='CAP' else ['ZC.out','YM.out','Ycap.out'];stages={name:matrix(d/name) for name in names}
            assert all(math.isfinite(v) for m in stages.values() for row in m for z in row for v in z)
            data={'frequency_hz':struct.unpack_from('<d',raw,8)[0],'contacts':int(re.search(r'Generating capacitance matrix \((\d+)x',log)[1]),'green_table_hash':hashes[sha],'raw_cache_bytes':len(payload),'raw_cache_sha256':sha,'stages':stages}
            if c['model']=='CAP':
                totals=re.search(r'Total Capacitance =\s*([-\d.]+) \(fF\)\s*Total Resistance =\s*([-\d.]+)',log);assert totals
                data.update(capacitance_ff_printed=float(totals[1]),resistance_printed=float(totals[2].rstrip('.')))
            else:
                numeric=lambda s:re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?|[-+]?nan|[-+]?inf',s,re.I)
                data.update(quality_printed=numeric(re.search(r'Pi Model at f=\S+ GHz:  Q = ([^\n]+)',log)[1]),pi_printed=[re.search(pattern,log)[1].strip() for pattern in [r'L  =\s*(\S+) nH',r'R  =([^\n]+)',r'Cs1=\s*(\S+) fF',r'Rs1=([^\n]+)',r'Cs2=\s*(\S+) fF',r'Rs2=([^\t]+)',r'f_res = ([^G]+)GHz']],input_impedance_printed=[numeric(line) for line in re.findall(r'Zin = ([^\n]+)',log)],shunt_printed=re.findall(r'The shunt resistance is ([^\n]+)',log))
            snapshots.append(data)
        assert snapshots[0]==snapshots[1],c['name']+' original outputs vary'
        captured.append({'name':c['name'],**snapshots[0]});print(c['name']+': repeated original matrices, reports and all Green payload bytes match',flush=True)
    out=ROOT/'tests/fixtures/original-fft-budget.json';out.write_text(json.dumps({'reference':'Two fresh unmodified original CAP/PI2 solves per 512-grid case; collapsed 256-grid outcome retained separately','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(definitions.read_bytes()).hexdigest(),'collapsed_grid':[256,256],'collapsed_grid_runs':collapsed,'cases':captured},indent=2,allow_nan=False)+'\n');print(out);print('Raw proof:',work)
if __name__=='__main__':main()
