"""Capture original creator aliases with isolated command-case and delimiter variants."""
import argparse,concurrent.futures,hashlib,json,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256,physical_geometry


def physical_bytes(shapes):
    out=bytearray(struct.pack('<I',len(shapes)))
    for name,shape in sorted(shapes.items()):
        out+=name.encode('ascii')+b'\0'+struct.pack('<2dI',*shape['origin'],len(shape['segments']))
        for segment in shape['segments']:
            out+=struct.pack('<id',segment['metal'],segment['width'])
            out+=struct.pack('<2i',segment['nx'],segment['ny']) if segment['metal']>=5 else struct.pack('<d',segment['thickness'])
            out+=struct.pack('<18d',*(v for point in segment['points'] for v in point))
    return bytes(out)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');parser.add_argument('--library-path');args=parser.parse_args()
    binary=args.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    source=ROOT/'tests/fixtures/original-creators.json';canonical=ROOT/'tests/fixtures/original-creator-binary.json';catalog=ROOT/'vendor/asitic/src/command_catalog.inc';technology=ROOT/'web/technology/CMOS.tek'
    originals={c['family']:c for c in json.loads(canonical.read_text())['cases']}
    families={c['script'].splitlines()[2].split()[0]:c for c in json.loads(source.read_text())['fixtures']}
    definitions=[]
    for line in catalog.read_text().splitlines():
        match=re.match(r'^\{(\d+),\{([^}]+)\}',line)
        if not match:continue
        aliases=json.loads('['+match[2]+']');case=families.get(aliases[0])
        if case:
            for alias in filter(None,aliases):definitions.append((int(match[1]),alias,case))
    base=ROOT/'build/original-creators';base.mkdir(parents=True,exist_ok=True)
    work=Path(tempfile.mkdtemp(prefix='capture-',dir=base))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null');env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=args.library_path or '/tmp/asitic-native-deps/readline-original:'+str(args.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu'
    def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def capture(definition):
        identifier,alias,case=definition;family=case['family'];accepted=alias==alias.upper()
        command=case['script'].splitlines()[2];keywords=command.partition(' ')[2]
        variants=[alias,alias.lower(),''.join(c.lower() if i%2 else c.upper() for i,c in enumerate(alias))]
        delimiter_command=' ,\t'+alias+(',\t'+keywords if accepted and keywords else '')+' \t'
        commands=[token+(' '+keywords if accepted and keywords else '') for token in variants]+[delimiter_command]
        captures=[];messages=[]
        for repeat,line in enumerate(commands):
            directory=work/alias/str(repeat);directory.mkdir(parents=True)
            script='CELL 1000 1000 1000\nCCELL 1000 1000\n'+line+'\nBSAVE raw\n'
            (directory/'input.asit').write_text(script)
            p=subprocess.run([str(binary),'-g','-t',str(technology),'-x','input.asit'],cwd=directory,env=env,input=case['stdin'] if accepted else 'EXIT\n',text=True,capture_output=True,timeout=60,start_new_session=True,preexec_fn=limits)
            (directory/'stdout.log').write_text(p.stdout);(directory/'stderr.log').write_text(p.stderr)
            if p.returncode:raise RuntimeError(alias+': original status '+str(p.returncode))
            shapes={}
            for path in directory.glob('raw.*'):
                data=path.read_bytes();offset=20
                while offset+4<=len(data) and struct.unpack_from('<I',data,offset)[0]==0x88:
                    begin=offset;name=data[offset+4:offset+84].split(b'\0')[0].decode('ascii');offset+=4+0xbc
                    while offset+4<=len(data) and struct.unpack_from('<I',data,offset)[0] in (0x89,0x8a):offset+=4+0xf0
                    shapes[name]=physical_geometry(data[:20]+data[begin:offset]+struct.pack('<I',0x8b))
            log=(directory/'asitic.LOG').read_text() if (directory/'asitic.LOG').exists() else p.stdout
            if accepted:
                assert set(shapes)==set(originals[family]['shapes']),(alias,shapes.keys())
                raw=physical_bytes(shapes);assert raw==physical_bytes(originals[family]['shapes']),alias+' differs from canonical original geometry'
                captures.append(hashlib.sha256(raw).hexdigest())
            else:
                assert not shapes and 'Unknown or Mistyped Commnd' in log,alias+' original rejection differs'
                captures.append(None);messages.append('Unknown or Mistyped Commnd')
        assert len(set(captures))==1,alias+' varies across command-case variants'
        print(alias+': '+('four original physical captures match' if accepted else 'four original rejections match'),flush=True)
        return dict(family=family,alias=alias,id=identifier if accepted else None,accepted=accepted,variants=variants,delimiter_command=delimiter_command,geometry_sha256=captures[0],**({'diagnostic':messages[0]} if messages else {}))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:cases=list(pool.map(capture,definitions))
    output=ROOT/'tests/fixtures/original-creator-aliases.json';output.write_text(json.dumps(dict(reference='Unmodified original commands in four fresh processes per catalog alias; original/lower/mixed command case and native comma/tab delimiters; raw BSAVE defined physical fields',executable_sha256=ORIGINAL_SHA256,technology_sha256=hashlib.sha256(technology.read_bytes()).hexdigest(),case_definition_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),canonical_geometry_sha256=hashlib.sha256(canonical.read_bytes()).hexdigest(),catalog_sha256=hashlib.sha256(catalog.read_bytes()).hexdigest(),physical_encoding='LE shape count; sorted ASCII name+NUL, origin doubles, segment count; each segment: metal int32, width double, thickness double or nx/ny int32, 18 point doubles',cases=cases),indent=2)+'\n');print(output);print('Raw proof:',work)


if __name__=='__main__':main()
