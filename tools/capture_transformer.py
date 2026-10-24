"""Repeat unmodified CALCTRANS/2PORTTRANS commands and retain original outputs."""
import argparse,concurrent.futures,hashlib,json,math,os,re,resource,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_eddy import COMMANDS
from capture_network import matrix

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-transformer.json');args=parser.parse_args()
    reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable differs')
    source=ROOT/'tests/fixtures/transformer-cases.json';technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();cases=json.loads(source.read_text())['cases'];base=ROOT/'build/original-transformer';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(case):
        tech=re.sub(r'(?mi)^\s*eddy\s*=.*$',lambda _: '\n'.join('eddy = '+str(i) for i in case['layers']),text)
        for key,value in zip(['fftx','ffty'],case['grid']):tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
        l,w,t=case['cells'];cl,cw=case['contact_cells'];ground=case.get('ground','');ground_name='A' if ground=='@1' else 'B' if ground=='@2' else 'G' if ground else ''
        script=f'CELL {l} {w} {t}\nCCELL {cl} {cw}\nEDDY '+('TRUE' if case['eddy_mode'] else 'FALSE')+'\nSAVEMAT TRUE\n'
        for creator in [case['primary'],case['secondary']]:
            if creator:script+=COMMANDS[creator['id']]+' '+creator['keywords']+'\n'
        if ground and not ground.startswith('@'):script+='WIRE '+ground+'\n'
        suffix=(' '+ground_name+' '+str(case['termination'])) if ground_name else ''
        f=case['frequency_ghz'];script+=f'CALCTRANS A B {f}'+suffix+f'\n2PORTTRANS A B {f} {f} .1 Y RECT ports.y'+suffix+'\n';snapshots=[]
        for repeat in range(2):
            directory=work/case['name']/str(repeat);directory.mkdir(parents=True);(directory/'CMOS.tek').write_text(tech);(directory/'input.asit').write_text(script)
            process=subprocess.run([str(binary),'-g','-t','CMOS.tek','-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=120,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
            if process.returncode:raise RuntimeError(case['name']+': original status '+str(process.returncode)+': '+process.stderr)
            pattern=r'L1=\s*(\S+)\s+R1=\s*(\S+)\s+L2=\s*(\S+)\s+R2=\s*(\S+)\s+M=\s*(\S+) \(k=\s*(\S+)\) RG =\s*(\S+)'
            report=re.search(pattern,process.stdout)
            if not report:raise RuntimeError('Missing transformer report: '+process.stdout)
            records=[]
            for line in (directory/'ports.y').read_text().splitlines():
                fields=line.split()
                if len(fields)==9:
                    try:records.append([float(x) for x in fields])
                    except ValueError:pass
            if len(records)!=1:raise RuntimeError('Expected one original two-port record')
            cache=next(directory.glob('*.dat')).read_bytes();payload=cache[16+2*40+5*236:];nx,ny=case['grid']
            if len(payload)!=15*((nx+1)*(ny+1)+2*nx+2*ny)*16:raise RuntimeError('Unexpected Green payload size')
            checksum=14695981039346656037
            for byte in payload:checksum=((checksum^byte)*1099511628211)&((1<<64)-1)
            data={'model_printed':list(report.groups()),'admittance_record':records[0],'node_capacitance_matrix':matrix(directory/'Ycap.out'),'green_table_hash':f'{checksum:016x}','raw_cache_bytes':len(payload),'raw_cache_sha256':hashlib.sha256(payload).hexdigest(),'quadpack_nonconvergence_messages':process.stdout.count('function did not converge.')//2,
                  'stage_sha256':{name:hashlib.sha256((directory/name).read_bytes()).hexdigest() for name in ['Ycap.out','ports.y']}}
            snapshots.append(data)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(case['name']+': outputs differ between fresh original processes')
        print(case['name']+': two identical whole-program transformer runs',flush=True);return {'name':case['name'],**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:captured=list(pool.map(capture,cases))
    document={'reference':'Unmodified CALCTRANS and 2PORTTRANS commands, two fresh processes per case','executable_sha256':ORIGINAL_SHA256,'libm_sha256':hashlib.sha256(Path('/lib/i386-linux-gnu/libm.so.6').read_bytes()).hexdigest(),'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'matrix_precision':'Native node snapshots %19.10g; two-port records first 8 fields %20.15e, last field %20.15g; model %6.3g','cases':captured}
    args.output.write_text(json.dumps(document,indent=2,allow_nan=False)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
