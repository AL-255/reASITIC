"""Package a coherent, content-addressed browser runtime for static hosting.

Root files stay available for native/Node verification. The public HTML loads
its complete module/worker/WASM/technology graph from one immutable directory.
A stale page whose release has disappeared offers an uncached root reload.
"""
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MARKER='<!-- Packaged by tools/package_site.py -->'

def digest(data):return hashlib.sha256(data).hexdigest()

def package(directory):
    directory=Path(directory).resolve();source={}
    for p in sorted(directory.rglob('*')):
        relative=p.relative_to(directory)
        if relative.parts[0]=='releases' or relative.as_posix()=='build.json':continue
        if p.is_symlink():raise RuntimeError('Site artifacts must be regular files: '+str(relative))
        if p.is_file():source[relative.as_posix()]=p.read_bytes()
    if MARKER.encode() in source.get('index.html',b''):
        previous=json.loads((directory/'build.json').read_text());original=directory/'releases'/previous['release']/'index.html'
        source['index.html']=original.read_bytes()
        assert digest(source['index.html'])==previous['files']['index.html']['sha256']
    for required in ['index.html','app.mjs','worker.mjs','asitic.js','asitic.wasm','style.css','technology/CMOS.tek','technology/BiCMOS.tek']:
        if required not in source:raise RuntimeError('Missing browser artifact: '+required)
    files={name:{'sha256':digest(data),'bytes':len(data)} for name,data in source.items()}
    release=digest(json.dumps(files,sort_keys=True,separators=(',',':')).encode());prefix='releases/'+release+'/'
    snapshot=directory/prefix;snapshot.mkdir(parents=True,exist_ok=True)
    for name,data in source.items():
        target=snapshot/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    html=source['index.html'].decode()
    style='href="style.css"';entry='<script type="module" src="app.mjs"></script>'
    assert html.count(style)==1 and html.count(entry)==1 and 'id="reload-app"' in html
    bootstrap='''<script type="module">
const reload=document.getElementById('reload-app'),fresh=new URL('./',location.href);
fresh.searchParams.set('reload',Date.now().toString(36));reload.href=fresh.href;
import(ENTRY).catch(error=>{
  document.getElementById('engine').textContent='Workbench unavailable';
  document.getElementById('status').textContent='Unable to load the workbench. Reload to get the current version.';
  reload.hidden=false;document.documentElement?.classList.add('load-failed');console.error(error);
});
</script>'''.replace('ENTRY',json.dumps('./'+prefix+'app.mjs'))
    html=MARKER+'\n'+html.replace(style,'href="'+prefix+'style.css"').replace('href="licenses/','href="'+prefix+'licenses/').replace(entry,bootstrap)
    (directory/'index.html').write_text(html)
    manifest={'release':release,'entry':prefix+'app.mjs','files':files,'root_index_sha256':digest(html.encode())}
    (directory/'build.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    return manifest

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--directory',type=Path,default=ROOT/'dist');a=parser.parse_args();result=package(a.directory);print('Packaged browser release '+result['release'])
