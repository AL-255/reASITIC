"""Use native C++ types to expose original implicit binary64 stores in WASM.

Compiler AST offsets, rather than statement heuristics, identify only implicit
long-double -> double conversions in the source unit. Header bodies are excluded.
All other expressions and their evaluation order remain unchanged.
"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]

def compiler_path(explicit=None):
    if explicit:return str(explicit)
    sdk=os.environ.get('EMSDK')
    candidates=[Path(sdk)/'upstream/bin/clang++'] if sdk else []
    if shutil.which('clang++'):candidates.append(Path(shutil.which('clang++')))
    candidates.append(Path.home()/'emsdk/upstream/bin/clang++')
    for p in candidates:
        if p.is_file():return str(p)
    raise RuntimeError('A Clang native AST frontend is required to preserve implicit stores')

def expose(name,text,clang=None):
    clang=compiler_path(clang);encoded=text.encode();digest=hashlib.sha256(encoded).hexdigest()
    version=subprocess.check_output([clang,'--version'],text=True).splitlines()[0]
    cache=ROOT/'build/native-ast';cache.mkdir(parents=True,exist_ok=True)
    source=cache/(name+'.cpp');source.write_bytes(encoded)
    key=hashlib.sha256((digest+version+'cv-target-v3').encode()).hexdigest()
    artifact=cache/(name+'-'+key[:16]+'.json')
    if artifact.exists():locations=json.loads(artifact.read_text())
    else:
        result=subprocess.run([clang,'-target','i386-linux-gnu','-std=c++17','-fsyntax-only','-I'+str(ROOT/'vendor/asitic/include'),'-I'+str(ROOT/'vendor/asitic/src'),'-Xclang','-ast-dump=json',str(source)],capture_output=True,text=True)
        if result.returncode:raise RuntimeError(f'Native AST analysis failed for {name}:\n{result.stderr}')
        locations=[]
        def visit(node):
            if node.get('kind')=='ImplicitCastExpr' and node.get('castKind')=='FloatingCast' and node.get('type',{}).get('qualType','').replace('const ','').replace('volatile ','')=='double':
                child=node.get('inner',[{}])[0]
                if child.get('type',{}).get('qualType','').replace('const ','').replace('volatile ','')=='long double':
                    span=node['range'];a,b=span['begin'],span['end']
                    if 'includedFrom' not in a and 'includedFrom' not in b and 'offset' in a and 'offset' in b:
                        if all('file' not in point or Path(point['file']).resolve()==source.resolve() for point in [a,b]):
                            begin,end=a['offset'],b['offset']+b['tokLen']
                            if 0<=begin<end<=len(encoded):locations.append([begin,end])
            for child in node.get('inner',[]):visit(child)
        visit(json.loads(result.stdout));locations=sorted(set(map(tuple,locations)))
        artifact.write_text(json.dumps(locations)+'\n')
    prefixes={};suffixes={}
    for begin,end in locations:
        prefixes[begin]=prefixes.get(begin,0)+1;suffixes[end]=suffixes.get(end,0)+1
    output=bytearray()
    for i in range(len(encoded)+1):
        output.extend(b')'*suffixes.get(i,0));output.extend(b'x87::binary64('*prefixes.get(i,0))
        if i<len(encoded):output.append(encoded[i])
    proof={'source_sha256':digest,'compiler':version,'stores':[{'begin':a,'end':b,'expression':encoded[a:b].decode()} for a,b in locations]}
    return output.decode(),proof
