"""Compile pinned portable arbitrary-precision C; hardware SIMD paths disabled."""
from pathlib import Path
import hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
def build(compiler,dist):
    vendor=ROOT/'vendor/libbf';manifest=json.loads((vendor/'provenance.json').read_text())
    for name,digest in manifest['files'].items():
        if hashlib.sha256((vendor/name).read_bytes()).hexdigest()!=digest:raise RuntimeError('Pinned LibBF source changed: '+name)
    flags=['-x','c','-std=c11','-O2','-fno-fast-math','-ffp-contract=off','-U__AVX2__','-I'+str(vendor)]
    key=hashlib.sha256((subprocess.check_output([compiler,'--version'],text=True)+json.dumps(manifest,sort_keys=True)+repr(flags)).encode()).hexdigest()[:16]
    out=ROOT/'build/libbf'/key;out.mkdir(parents=True,exist_ok=True);objects=[]
    for name in ['libbf.c','cutils.c']:
        obj=out/(name+'.o');objects.append(str(obj))
        if not obj.exists():subprocess.run([compiler,*flags,'-c',str(vendor/name),'-o',str(obj)],check=True)
    notice=dist/'licenses/libbf';notice.mkdir(parents=True,exist_ok=True);shutil.copy2(vendor/'COPYING',notice/'COPYING');shutil.copy2(vendor/'readme.txt',notice/'readme.txt')
    return objects
