"""Build the same portable geometry math for a native host and WebAssembly."""
import argparse,concurrent.futures,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def build_objects(compiler,target):
    vendor=ROOT/'vendor';manifests={name:json.loads((vendor/name/'provenance.json').read_text()) for name in ['softfloat','libbf']}
    for name,manifest in manifests.items():
        for file,digest in manifest['files'].items():
            if hashlib.sha256((vendor/name/file).read_bytes()).hexdigest()!=digest:raise RuntimeError('Vendor source changed: '+name+'/'+file)
    version=subprocess.check_output([compiler,'--version'],text=True)
    flags=['-x','c','-std=c11','-O2','-fno-fast-math','-ffp-contract=off','-U__AVX2__','-DSOFTFLOAT_FAST_INT64','-DINLINE_LEVEL=5','-DSOFTFLOAT_ROUND_ODD']
    inc=['-I'+str(ROOT/'kernel'),'-I'+str(vendor/'softfloat/include'),'-I'+str(vendor/'softfloat/8086'),'-I'+str(vendor/'libbf')]
    fingerprint=hashlib.sha256((version+repr(flags)+json.dumps(manifests,sort_keys=True)).encode()).hexdigest()[:16]
    objdir=ROOT/'build/portable-math'/target/fingerprint;objdir.mkdir(parents=True,exist_ok=True)
    files=[*sorted((vendor/'softfloat').rglob('*.c')),vendor/'libbf/libbf.c',vendor/'libbf/cutils.c']
    def compile_one(file):
        out=objdir/(file.parent.name+'_'+file.stem+'.o')
        if not out.exists():subprocess.run([compiler,*flags,*inc,'-c',str(file),'-o',str(out)],check=True,stdout=subprocess.DEVNULL)
        return str(out)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:objects=list(pool.map(compile_one,files))
    return objects,inc

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--emxx',default=os.environ.get('EMXX',shutil.which('em++') or str(Path.home()/'emsdk/upstream/emscripten/em++')));args=parser.parse_args()
    sources=[str(ROOT/'kernel/portable_gmd.cpp'),str(ROOT/'kernel/portable_gmd_big.cpp'),str(ROOT/'kernel/pi_shunts.cpp'),str(ROOT/'tests/extended_ports_probe.cpp')];out=ROOT/'build/portable-math';out.mkdir(parents=True,exist_ok=True)
    common=['-std=c++17','-O2','-fno-fast-math','-ffp-contract=off','-DSOFTFLOAT_FAST_INT64','-DINLINE_LEVEL=5','-DSOFTFLOAT_ROUND_ODD']
    objects,inc=build_objects(args.emxx,'wasm')
    subprocess.run([args.emxx,*common,*inc,*sources,*objects,'-sMODULARIZE=1','-sEXPORT_ES6=1','-sENVIRONMENT=node','-sALLOW_MEMORY_GROWTH=1','-sFILESYSTEM=0','-sEXPORTED_FUNCTIONS=["_probe_gmd","_probe_error_power","_probe_pi_shunts","_probe_extended_pi_shunts","_probe_extended_ports","_probe_folded_ports","_probe_pi_impedance","_probe_scattering","_loaded_port_impedance","_malloc","_free"]','-sEXPORTED_RUNTIME_METHODS=["UTF8ToString","HEAPF64"]','-o',str(out/'gmd.mjs')],check=True)
    objects,inc=build_objects('gcc','native')
    subprocess.run(['g++',*common,*inc,*sources,str(ROOT/'tests/portable_math_native.cpp'),*objects,'-lm','-o',str(out/'gmd-native')],check=True)
    print('Built portable C++ geometry math for native host and WASM')
if __name__=='__main__':main()
