"""Build the browser kernel from the pinned C++ recovery and SoftFloat."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from port_geometry import generate,SOURCES
from port_libm import build as build_libm
from port_libbf import build as build_libbf
from package_site import package
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--emxx',default=os.environ.get('EMXX',shutil.which('em++') or str(Path.home()/'emsdk/upstream/emscripten/em++')));args=parser.parse_args()
    manifest=json.loads((ROOT/'vendor/asitic/provenance.json').read_text())
    for vendor in ['asitic','softfloat']:
        manifest=json.loads((ROOT/'vendor'/vendor/'provenance.json').read_text())
        for name,digest in manifest['files'].items():
            if hashlib.sha256((ROOT/'vendor'/vendor/name).read_bytes()).hexdigest()!=digest:raise RuntimeError('Reference snapshot changed: '+vendor+'/'+name)
    clang=Path(shutil.which(args.emxx) or args.emxx).resolve().parent.parent/'bin/clang++'
    port=generate(clang=clang);dist=ROOT/'dist'
    if dist.exists():shutil.rmtree(dist)
    shutil.copytree(ROOT/'web',dist)
    inc=['-I'+str(port),'-I'+str(ROOT/'kernel'),'-I'+str(ROOT/'vendor/softfloat/include'),'-I'+str(ROOT/'vendor/softfloat/8086'),'-I'+str(ROOT/'vendor/libbf')]
    flags=['-O2','-fno-fast-math','-ffp-contract=off','-DSOFTFLOAT_FAST_INT64','-DINLINE_LEVEL=5','-DSOFTFLOAT_ROUND_ODD']
    version=subprocess.check_output([args.emxx,'--version'],text=True)
    fingerprint=hashlib.sha256((version+repr(flags)+(ROOT/'kernel/platform.h').read_text()+json.dumps(manifest,sort_keys=True)).encode()).hexdigest()
    objects=[]
    objdir=ROOT/'build/obj'/fingerprint[:16];objdir.mkdir(parents=True,exist_ok=True)
    for p in sorted((ROOT/'vendor/softfloat').rglob('*.c')):
        obj=objdir/(p.parent.name+'_'+p.stem+'.o');objects.append(str(obj))
        if not obj.exists() or p.stat().st_mtime>obj.stat().st_mtime:
            subprocess.run([args.emxx,'-x','c',*flags,*inc,'-c',str(p),'-o',str(obj)],check=True)
    objects+=build_libm(args.emxx,dist)
    objects+=build_libbf(args.emxx,dist)
    cmd=[args.emxx,'-std=c++17',*flags,*inc,'-include',str(ROOT/'kernel/soft_real.hpp'),'-fpack-struct=4',str(ROOT/'kernel/extended_network.cpp'),str(ROOT/'kernel/folded_network.cpp'),
         *[str(port/(n+'.cpp')) for n in SOURCES],str(ROOT/'kernel/bridge.cpp'),str(ROOT/'kernel/simulation.cpp'),str(ROOT/'kernel/coupling.cpp'),str(ROOT/'kernel/capacitance.cpp'),str(ROOT/'kernel/two_port_records.cpp'),str(ROOT/'kernel/port_schedule.cpp'),str(ROOT/'kernel/self_resonance.cpp'),str(ROOT/'kernel/automatic_cells.cpp'),str(ROOT/'kernel/three_history.cpp'),str(ROOT/'kernel/transcendentals.cpp'),str(ROOT/'kernel/portable_gmd.cpp'),str(ROOT/'kernel/portable_gmd_big.cpp'),str(ROOT/'kernel/pi_shunts.cpp'),str(ROOT/'kernel/numerical_policy.cpp'),str(ROOT/'kernel/math_probe.cpp'),str(ROOT/'kernel/runtime_math.cpp'),str(ROOT/'kernel/eddy_runtime.cpp'),str(ROOT/'kernel/command_input.cpp'),*objects,
         '-sMODULARIZE=1','-sEXPORT_ES6=1','-sENVIRONMENT=web,worker,node','-sALLOW_MEMORY_GROWTH=1','-sFILESYSTEM=0',
         '-sEXPORTED_FUNCTIONS=["_malloc","_free","_generate","_create_geometry","_set_layer_name","_set_geometry_options","_set_ring_gap_radians","_normalize_value","_reset_technology","_set_substrate","_set_substrate_material","_finalize_technology","_set_metal","_set_metal_rules","_set_via","_last_warnings","_analyze","_coupling","_optimization_plan","_optimization_multimetal_plan","_optimization_create","_optimization_accept","_set_potential_memo","_potential_memo_stats","_simulate","_solver_limits","_simulate_capacitance","_max_contact_count","_potential_grid_fits","_probe_math","_probe_potential_contact","_probe_gmd","_probe_error_power","_set_reference_numerics","_probe_retained_math","_probe_runtime_trig","_probe_green","_simulate_eddy","_simulate_network","_simulate_extended_network","_simulate_pad_network","_simulate_transformer","_simulate_three_port","_probe_three_port","_probe_two_port_output","_port_frequency_schedule","_automatic_cell_limits","_set_max_width_cells","_edit_geometry","_self_resonance_plan","_shape_kind","_port_analysis_plan","_capacitor_port_matrix","_capacitor_port_response"]',
         '-sEXPORTED_RUNTIME_METHODS=["UTF8ToString","HEAPF64"]','-o',str(dist/'asitic.js')]
    subprocess.run(cmd,check=True)
    site=package(dist)
    print('Built',dist/'asitic.wasm','in browser release',site['release'])
if __name__=='__main__':main()
