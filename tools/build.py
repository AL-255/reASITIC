"""Build the browser kernel from the pinned C++ recovery and SoftFloat."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from port_geometry import generate,SOURCES
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--emxx',default=os.environ.get('EMXX',shutil.which('em++') or str(Path.home()/'emsdk/upstream/emscripten/em++')));args=parser.parse_args()
    manifest=json.loads((ROOT/'vendor/asitic/provenance.json').read_text())
    for vendor in ['asitic','softfloat']:
        manifest=json.loads((ROOT/'vendor'/vendor/'provenance.json').read_text())
        for name,digest in manifest['files'].items():
            if hashlib.sha256((ROOT/'vendor'/vendor/name).read_bytes()).hexdigest()!=digest:raise RuntimeError('Reference snapshot changed: '+vendor+'/'+name)
    clang=Path(shutil.which(args.emxx) or args.emxx).resolve().parent.parent/'bin/clang++'
    port=generate(clang=clang);dist=ROOT/'dist';shutil.copytree(ROOT/'web',dist,dirs_exist_ok=True)
    inc=['-I'+str(port),'-I'+str(ROOT/'kernel'),'-I'+str(ROOT/'vendor/softfloat/include'),'-I'+str(ROOT/'vendor/softfloat/8086')]
    flags=['-O2','-fno-fast-math','-ffp-contract=off','-DSOFTFLOAT_FAST_INT64','-DINLINE_LEVEL=5','-DSOFTFLOAT_ROUND_ODD']
    version=subprocess.check_output([args.emxx,'--version'],text=True)
    fingerprint=hashlib.sha256((version+repr(flags)+(ROOT/'kernel/platform.h').read_text()+json.dumps(manifest,sort_keys=True)).encode()).hexdigest()
    objects=[]
    objdir=ROOT/'build/obj'/fingerprint[:16];objdir.mkdir(parents=True,exist_ok=True)
    for p in sorted((ROOT/'vendor/softfloat').rglob('*.c')):
        obj=objdir/(p.parent.name+'_'+p.stem+'.o');objects.append(str(obj))
        if not obj.exists() or p.stat().st_mtime>obj.stat().st_mtime:
            subprocess.run([args.emxx,'-x','c',*flags,*inc,'-c',str(p),'-o',str(obj)],check=True)
    cmd=[args.emxx,'-std=c++17',*flags,*inc,'-include',str(ROOT/'kernel/soft_real.hpp'),'-fpack-struct=4',
         *[str(port/(n+'.cpp')) for n in SOURCES],str(ROOT/'kernel/bridge.cpp'),str(ROOT/'kernel/simulation.cpp'),str(ROOT/'kernel/capacitance.cpp'),str(ROOT/'kernel/transcendentals.cpp'),str(ROOT/'kernel/math_probe.cpp'),str(ROOT/'kernel/command_input.cpp'),*objects,
         '-sMODULARIZE=1','-sEXPORT_ES6=1','-sENVIRONMENT=web,worker,node','-sALLOW_MEMORY_GROWTH=1','-sFILESYSTEM=0',
         '-sEXPORTED_FUNCTIONS=["_malloc","_free","_generate","_create_geometry","_set_layer_name","_set_geometry_options","_set_ring_gap_radians","_normalize_value","_reset_technology","_set_substrate","_set_substrate_material","_finalize_technology","_set_metal","_set_via","_last_warnings","_analyze","_simulate","_simulate_capacitance","_probe_math"]',
         '-sEXPORTED_RUNTIME_METHODS=["UTF8ToString","HEAPF64"]','-o',str(dist/'asitic.js')]
    subprocess.run(cmd,check=True)
    print('Built',dist/'asitic.wasm')
if __name__=='__main__':main()
