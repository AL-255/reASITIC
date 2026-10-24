"""Capture original dense-contact CAP/PI2 solves, without modifying the mesh."""
import argparse,concurrent.futures,gzip,hashlib,json,math,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_network import matrix

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/contact-budget-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases']
    base=ROOT/'build/original-contact-budget';base.mkdir(exist_ok=True,parents=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(64*1024*1024,64*1024*1024))
    def capture(c):
        snapshots=[];tech=text
        for key,value in zip(['fftx','ffty'],c['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        l,w,t=c['cells'];cl,cw=c['contact_cells'];ground=' G' if c.get('ground') else ''
        script=f'EDDY FALSE\nCELL {l} {w} {t}\nCCELL {cl} {cw}\nSAVEMAT TRUE\nWIRE '+c['keywords']+'\n'
        if ground:script+='WIRE '+c['ground']+'\n'
        script+=c['model']+' A '+str(c['frequency_ghz'])+ground+'\n'
        if c['model']=='PI2':script+='ZIN S\nZIN X\nZIN D\nSHUNTR S\nSHUNTR D\n'
        for repeat in range(2):
            directory=work/c['name']/str(repeat);directory.mkdir(parents=True);(directory/'process.tek').write_text(tech);(directory/'input.asit').write_text(script);(directory/'stdin.txt').write_text('EXIT\n')
            with (directory/'stdout.log').open('w') as out,(directory/'stderr.log').open('w') as err:
                p=subprocess.run([str(binary),'-g','-t','process.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,stdout=out,stderr=err,start_new_session=True,preexec_fn=limits,timeout=180)
            log=(directory/'stdout.log').read_text();assert p.returncode==0 and 'Error:' not in log,(c['name'],p.returncode,log[-2000:])
            dims=re.search(r'Generating capacitance matrix \((\d+)x(\d+)\)',log);assert dims and dims[1]==dims[2];contacts=int(dims[1]);assert contacts>512
            cache=next(directory.glob('*.dat')).read_bytes();payload=cache[16+2*40+5*236:];nx,ny=c['grid'];assert len(payload)==15*((nx+1)*(ny+1)+2*nx+2*ny)*16
            checksum=14695981039346656037
            for byte in payload:checksum=((checksum^byte)*1099511628211)&((1<<64)-1)
            stages=['YC.out','Pmat.out'] if c['model']=='CAP' else ['ZC.out','YM.out','Ycap.out']
            captured={name:matrix(directory/name) for name in stages}
            assert all(isinstance(v,(int,float)) and math.isfinite(v) for m in captured.values() for row in m for z in row for v in z)
            data=dict(contacts=contacts,green_table_hash=f'{checksum:016x}',raw_cache_bytes=len(payload),raw_cache_sha256=hashlib.sha256(payload).hexdigest(),stages=captured,stage_sha256={name:hashlib.sha256((directory/name).read_bytes()).hexdigest() for name in stages})
            if c['model']=='CAP':
                assert len(captured['Pmat.out'])==contacts
                totals=re.search(r'Total Capacitance =\s*([-\d.]+) \(fF\)\s*Total Resistance =\s*([-\d.]+)',log);assert totals
                data.update(capacitance_ff_printed=float(totals[1]),resistance_printed=float(totals[2].rstrip('.')))
            else:
                numeric=lambda value:re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?|[-+]?nan|[-+]?inf',value,re.I)
                quality=re.search(r'Pi Model at f=(\S+) GHz:  Q = ([^\n]+)',log);assert quality
                data.update(quality_printed=numeric(quality[2]),pi_printed=[re.search(pattern,log)[1].strip() for pattern in [r'L  =\s*(\S+) nH',r'R  =([^\n]+)',r'Cs1=\s*(\S+) fF',r'Rs1=([^\n]+)',r'Cs2=\s*(\S+) fF',r'Rs2=([^\t]+)',r'f_res = ([^G]+)GHz']],input_impedance_printed=[numeric(line) for line in re.findall(r'Zin = ([^\n]+)',log)],shunt_printed=re.findall(r'The shunt resistance is ([^\n]+)',log))
            snapshots.append(data)
        assert snapshots[0]==snapshots[1],c['name']+' original outputs vary'
        print(c['name']+': '+str(contacts)+' contacts; two original solves, all selected matrices, Green bytes and reports repeat exactly',flush=True)
        return dict(name=c['name'],model=c['model'],**snapshots[0])
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:captured=list(pool.map(capture,cases))
    document=dict(reference='Two fresh unmodified original whole-program CAP/PI2 solves per case; unchanged mesh, native printed matrices/reports and raw Green hashes',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),matrix_precision='Native SAVEMAT %19.10g',cases=captured)
    output=ROOT/'tests/fixtures/original-contact-budget.json.gz';output.write_bytes(gzip.compress(json.dumps(document,separators=(',',':'),allow_nan=False).encode(),mtime=0));print(output);print('Raw proof:',work)
if __name__=='__main__':main()
