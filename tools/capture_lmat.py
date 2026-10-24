"""Capture physical geometry and LMAT files from the unmodified ASITIC binary.

Runs isolated process groups, pins the executable hash, repeats every session,
and keeps raw logs/matrices below build/original-lmat. Production never uses it.
"""
import argparse,concurrent.futures,hashlib,json,math,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ORIGINAL_SHA256='72bcb93444604b89a65cb64e5825bfcff0ac740a816120d0f171855206bd572f'
KINDS={'wire':2,'square':0,'polygon':1,'ring':5,'multimetal':8,'symmetric_square':3,'symmetric_polygon':4}

def physical_geometry(data):
    if struct.unpack_from('<I',data,20)[0]!=0x88:raise ValueError('Unexpected native shape marker')
    record=data[24:24+0xbc];offset=24+0xbc;segments=[]
    while offset+4<=len(data) and struct.unpack_from('<I',data,offset)[0]==0x89:
        offset+=4;segment=data[offset:offset+0xf0];offset+=0xf0
        metal,nx,ny=struct.unpack_from('<3i',segment,0xdc)
        entry={'metal':metal,'width':struct.unpack_from('<d',segment,0xcc)[0],
               'points':[list(struct.unpack_from('<3d',segment,i)) for i in (0,24,0x44,0x5c,0x88,0xa0)]}
        if metal>=5:entry.update(nx=nx,ny=ny)
        else:entry['thickness']=struct.unpack_from('<d',segment,0xd4)[0]
        segments.append(entry)
    if not segments:raise ValueError('No original physical segments')
    return {'origin':list(struct.unpack_from('<2d',record,0x54)),'segments':segments}

def matrix(path):
    result=[]
    for line in path.read_text().splitlines():
        fields=line.split()
        if len(fields)!=4:continue
        i,j=int(fields[0]),int(fields[1]);z=[float(fields[2]),float(fields[3])]
        if not all(map(math.isfinite,z)):raise ValueError('Nonfinite original matrix')
        while len(result)<=i:result.append([])
        if len(result[i])!=j:raise ValueError('Unexpected original matrix ordering')
        result[i].append(z)
    if not result or any(len(row)!=len(result) for row in result):raise ValueError('Incomplete original matrix')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re')
    parser.add_argument('--library-path');parser.add_argument('--output',type=Path,default=ROOT/'tests/fixtures/original-lmat.json')
    args=parser.parse_args();reference=args.reference.resolve();binary=reference/'run/asitic.linux.2.2'
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=ORIGINAL_SHA256:raise RuntimeError('Original executable hash differs')
    technology=ROOT/'web/technology/CMOS.tek';source=ROOT/'tests/fixtures/connected-analysis.json'
    cases=[(i,c) for i,c in enumerate(json.loads(source.read_text())['cases']) if c['kind'] in KINDS]
    base=ROOT/'build/original-lmat';base.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null')
    env.pop('LD_PRELOAD',None)
    env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    meshes=[('coarse',1e9,[1000,1000,1000,1000]),('fine',2e9,[50,3,.5,50]),('unequal',1e9,[20,1000,1000,50])]
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(item):
        index,case=item;p=case['parameters'];name=f'{index}-{case["kind"]}'
        command=next(line for line in case['script'].splitlines() if re.match(r'^(WIRE|SQUARE|SPIRAL|RING|MMSQUARE|SYMSQ|SYMPOLY) ',line))
        options={'x':p.get('x',0),'y':p.get('y',0),'phase':p.get('phase',1)}
        # Make placement explicit, including the native SYMPOLY default.
        command+=f':XORG={options["x"]}:YORG={options["y"]}'
        script='CELL 1000 1000 1000\nCCELL 1000 1000\n'+command+'\nBSAVE geometry A\n'
        for mesh,frequency,cells in meshes:
            script+=f'CELL {cells[0]} {cells[1]} {cells[2]}\nCCELL {cells[3]} 1000\nLMAT A {frequency/1e9:g} {mesh}.out\n'
        snapshots=[]
        for repeat in range(2):
            directory=work/name/str(repeat);directory.mkdir(parents=True);(directory/'input.asit').write_text(script)
            process=subprocess.run([str(binary),'-g','-t',str(technology),'-x','input.asit'],cwd=directory,env=env,input='EXIT\n',text=True,capture_output=True,timeout=60,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(process.stdout);(directory/'stderr.log').write_text(process.stderr)
            if process.returncode!=0:raise RuntimeError(f'{name}: original status {process.returncode}: {process.stderr}')
            counts=re.findall(r'Generating inductance matrix \((\d+)x(\d+)\)',process.stdout)
            if len(counts)!=3:raise RuntimeError(f'{name}: missing original solve stages: {process.stdout}')
            geometry_file=next(directory.glob('geometry.*'));geometry_bytes=geometry_file.read_bytes()
            data={'geometry':physical_geometry(geometry_bytes),'matrices':[]}
            for (mesh,frequency,cells),count in zip(meshes,counts):
                raw=(directory/(mesh+'.out')).read_bytes()
                data['matrices'].append({'name':mesh,'frequency_hz':frequency,'cells':cells,'filaments':int(count[0]),'matrix':matrix(directory/(mesh+'.out')),'file_sha256':hashlib.sha256(raw).hexdigest()})
            snapshots.append(data)
        if snapshots[0]!=snapshots[1]:raise RuntimeError(f'{name}: original physical output changes between fresh processes')
        print(f'{name}: two identical original runs, three complete matrices',flush=True)
        return {'name':name,'kind':KINDS[case['kind']],'parameters':p,'options':options,'script':script,**snapshots[0]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:captured=list(pool.map(capture,cases))
    document={'reference':'Unmodified original ASITIC executable; repeated isolated whole-process sessions','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'case_definition_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'matrix_precision':'Native LMAT %10.20lf files; geometry is raw BSAVE binary64','cases':captured}
    args.output.write_text(json.dumps(document,indent=2)+'\n');print(args.output);print('Raw proof:',work)
if __name__=='__main__':main()
