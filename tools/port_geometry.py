"""Generate WASM bodies from hash-pinned recovered C++ without editing vendor.

Only enumerated x87 instruction patterns are translated. Unknown assembly is a
hard error. Each long double arithmetic operation uses software binary80.
"""
from pathlib import Path
import re,json
from implicit_stores import expose
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['geometry','shape_construction','spiral_construction','symmetric_construction','winding_construction','transformer3d','composite_geometry','resistance','numerics','rectangular','separation','parallel','intersection','canonicalize','planar','spatial','segment','estimate','metal_area','filaments','filament_setup','impedance','vias','complex','symmetric','blocked','general','solver','argument_value','input_normalization','shape_prompts','shape_prompt_symmetric','shape_prompt_transformer','shape_prompt_ring_via','shape_creation_commands','shape_creation_primitives','shape_creation_symmetric','shape_creation_sympoly','shape_creation_transformer','shape_creation_mirror','native_creators','legacy_complex','potential','potential_tables','potential_cache','contacts','potential_solver','green','green_integration','quadrature','quadrature_wrappers','oscillatory','eddy','technology','network','node_equations','combined_network','later_analysis','three_port','three_port_analysis','three_port_transforms','two_port_output','port_sweep','port_records','extended_node','extended_driver','extended_analysis','frequency_geometry','geometry_parser','geometry_edit','geometry_dispatch','self_resonance','capacitor_ports','optimization','optimization_radius','optimization_plan']

def translate(name,s):
    if name=='optimization_radius':
        begin=s.index('double floor_stored(');end=s.index('\n}',begin)+2
        s=s[:begin]+'double floor_stored(long double value) { return x87::binary64(x87::floor(value)); }'+s[end:]
        s=s.replace('asm volatile("fstl %0" : "=m"(discarded) : "t"(product));','discarded=x87::binary64(product);')
    # Loads/stores of finite double values. Signaling NaN status is not exposed.
    s=re.sub(r'asm volatile\("fldl %1; fstpl %0"\s*:\s*"=m"\(out\)\s*:\s*"m"\(in\)\s*:\s*"st"(?:,"memory")?\);','out=in;',s)
    s=s.replace('asm volatile("fldl %1; fstpl %0" : "=m"(*static_cast<double*>(out)) : "m"(*static_cast<const double*>(in)) : "st");','*static_cast<double*>(out)=*static_cast<const double*>(in);')
    s=s.replace('asm volatile("fldl %1; fstpt %0" : "=m"(out) : "m"(in) : "st","memory");','out=in;')
    s=s.replace('asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(value) : "st");','out=value;')
    s=s.replace('asm volatile("fldl %1; fsubl %2; fstpl %0" : "=m"(out) : "m"(first),"m"(second) : "st");','out=x87::binary64(static_cast<long double>(first)-second);')
    s=s.replace('asm volatile("fldl %1" : "=t"(result) : "m"(value));','result=value;')
    s=s.replace('asm volatile("fldl %1; fsubl %2" : "=t"(result) : "m"(first),"m"(second));','result=static_cast<long double>(first)-second;')
    s=s.replace('asm volatile("fmull %2" : "=t"(result) : "0"(first),"m"(second));','result=first*second;')
    s=s.replace('asm volatile("faddl %2" : "=t"(result) : "0"(first),"m"(second));','result=first+second;')
    for op, instruction in [('-', 'fsubl'),('+','faddl')]:
        s=s.replace(f'asm volatile("fldl %1; {instruction} %2" : "=t"(result) : "m"(segment.endpoints[index]),"m"(origin[index%3]));',f'result=static_cast<long double>(segment.endpoints[index]){op}origin[index%3];')
    s=s.replace('asm volatile("fldl %1; fstpl %0" : "=m"(mutual) : "m"(sum) : "st","memory");','mutual=sum;')
    s=s.replace('asm volatile("fdivrl %2" : "=t"(result) : "0"(denominator),"m"(sum));','result=static_cast<long double>(sum)/denominator;')
    s=s.replace('asm volatile("fstpl %0" : "=m"(destination) : "t"(value) : "st");','destination=x87::binary64(value);')
    s=s.replace('asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(n):"st");','out=n;')
    # Creator load/compare helpers preserve x87 width and status-word tests.
    for output,input_ in [(out,inp) for out in ('out','result','r','y') for inp in ('n','value','d','x','in')]:
        s=s.replace(f'asm volatile("fldl %1; fstpt %0":"=m"({output}):"m"({input_}):"st");',f'{output}={input_};')
    for output,input_ in [('out','a.value'),('y','a.value'),('result','value.value'),('out','value')]:
        s=s.replace(f'asm volatile("flds %1; fstpt %0":"=m"({output}):"m"({input_}):"st");',f'{output}=x87::load_float_word({input_});')
    s=s.replace('asm volatile("fldl %1; fsubl %2; fstpt %0":"=m"(result):"m"(upper),"m"(lower):"st");','result=static_cast<long double>(upper)-lower;')
    s=s.replace('asm volatile("fldl %1; fsubl %2; fstpl %0":"=m"(result):"m"(upper),"m"(lower):"st");','result=x87::binary64(static_cast<long double>(upper)-lower);')
    for flag in ['status']:
        s=s.replace(f'asm volatile("fldt %2; fldt %1; fcompp; fnstsw %%ax":"=a"({flag}):"m"(a),"m"(b):"st","st(1)");',f'{flag}=x87::comparison(a,b)<<8;')
    s=s.replace('asm volatile("fptan; fstp %%st(0)":"=t"(tangent):"0"(half));','tangent=x87::tan(half);')
    s=s.replace('asm volatile("fldt %1; fstl %0; fstp %%st(0)":"=m"(discarded):"m"(product):"st");','discarded=x87::binary64(product);')
    for value in ['n','value']:
        s=s.replace(f'asm volatile("fldt %1; fcos; fstpt %0":"=m"(out):"m"({value}):"st");',f'out=x87::cos({value});')
    # Whole helpers containing control-word changes, whose intended rounding
    # mode is explicit in the recovered source.
    helper={'ceiling':'ceil','floor_binary64':'floor','floor_spill':'floor','truncate64':'truncate64','floor64':'floor','down':'floor','integer':'truncate_int','truncate_integer':'truncate_int'}
    for func,op in helper.items():
        pattern=r'((?:long double|double|long long|int) '+func+r'\((?:long double|double) (\w+)\) \{)[^{}]*?asm[^{}]*?\n\}'
        s=re.sub(pattern,lambda m:m[1]+f' return '+('x87::binary64(' if func in ('floor_binary64','floor_spill','floor64','down') else '')+f'x87::{op}({m[2]})'+(')' if func in ('floor_binary64','floor_spill','floor64','down') else '')+';\n}',s)
    s=s.replace('asm volatile("fldl %1; fsubl %2; fstpl %0" : "=m"(direction[i]) : "m"(s.endpoints[i+3]),"m"(s.endpoints[i]) : "st","memory");','direction[i]=binary64(static_cast<long double>(s.endpoints[i+3])-s.endpoints[i]);')
    s=s.replace('asm volatile("fptan; fstp %%st(0); fstp %%st(0)" :: "t"(half) : "st","st(1)","memory");','(void)half; // discarded tangent; FP status is not yet exposed')
    s=s.replace('asm volatile("fldz; fsubrl %1; fstpt %0" : "=m"(rounded_height) : "m"(height) : "st","memory");','rounded_height=static_cast<long double>(height)-0.L;')
    s=s.replace('asm volatile("fldt %1; fldt %2; fmulp; fstpt %0" : "=m"(dy) : "m"(rounded_height),"m"(sy) : "st","st(1)","memory");','dy=rounded_height*sy;')
    for instr,op in [('fadd','+'),('fsub','-'),('fmul','*'),('fdiv','/')]:
        s=s.replace(f'asm volatile("{instr} %%st(1),%%st" : "=t"(r) : "0"(a),"u"(b));',f'r=a{op}b;')
    s=s.replace('asm volatile("fstpl %0" : "=m"(out) : "t"(value) : "st");','out=x87::binary64(value);')
    for output,input_ in [('result','value'),('value','fraction')]:
        s=s.replace(f'asm("fmulp" : "=t"({output}) : "0"({input_}),"u"(sign) : "st(1)");',f'{output}={input_}*sign;')
    s=s.replace('asm volatile("fstl %0" : "=m"(slot) : "t"(value));','slot=x87::binary64(value);')
    s=s.replace('asm volatile("fldl %1" : "=t"(out) : "m"(x));','out=x;')
    s=s.replace('asm volatile("fmull %2" : "=t"(out) : "0"(x),"m"(y));','out=x*y;')
    s=s.replace('asm("fld1; fxch; fyl2xp1" : "=t"(logarithm) : "0"(delta) : "st(1)");','logarithm=x87::log2p1(delta);')
    s=s.replace('asm("fld1; fxch; fyl2x" : "=t"(logarithm) : "0"((long double)x) : "st(1)");','logarithm=x87::log2(static_cast<long double>(x));')
    if name=='metal_area':
        begin=s.index('    asm volatile(',s.index('void segment_length_width('));end=s.index('\n}',begin)
        s=s[:begin]+'''    const long double dx=static_cast<long double>(s.endpoints[3])-s.endpoints[0],
        dy=static_cast<long double>(s.endpoints[4])-s.endpoints[1],
        dz=static_cast<long double>(s.endpoints[5])-s.endpoints[2];
    length=x87::sqrt((dx*dx+dy*dy)+dz*dz);width=s.width;'''+s[end:]
        for instruction,operator in [('fadd','+'),('fmul','*')]:
            s=s.replace('asm volatile("'+instruction+' %%st(1),%%st":"=t"(r):"0"(a),"u"(b));','r=a'+operator+'b;')
    if name=='eddy':
        begin=s.index('int rounded_integer(');end=s.index('const ConductorSegment* next_segment',begin)
        s=s[:begin]+'int rounded_integer(long double value,unsigned short mode) { return x87::truncate_int(x87::binary64(mode==0x0800u?x87::ceil(value):x87::floor(value))); }\n'+s[end:]
    s=s.replace('asm("fyl2x" : "=t"(logarithm) : "0"(base),"u"(1.0L) : "st(1)");','logarithm=x87::log2(base);')
    s=s.replace('asm("fldl %1":"=t"(r):"m"(x));','r=x;')
    s=s.replace('asm("fld1; fdivl %1":"=t"(shunt):"m"(termination));','shunt=1.L/static_cast<long double>(termination);')
    s=s.replace('asm("faddl %2":"=t"(updated):"0"(shunt),"m"(cell.real));','updated=shunt+cell.real;')
    s=s.replace('asm("fldl %1; faddl %2":"=t"(updated):"m"(cell.imag),"m"(zero));','updated=static_cast<long double>(cell.imag)+zero;')
    for input_ in ['in','value']:
        s=s.replace(f'asm volatile("fldl %1" : "=t"(out) : "m"({input_}));',f'out={input_};')
    s=s.replace('asm volatile("fldl %1":"=t"(out):"m"(value));','out=value;')
    for out,inp in [('r','x'),('f','frequency')]:
        s=s.replace(f'asm volatile("fldl %1":"=t"({out}):"m"({inp}));',f'{out}={inp};')
    s=s.replace('asm volatile("fldl %1; faddl %2":"=t"(r):"m"(a),"m"(b));','r=static_cast<long double>(a)+b;')
    for instruction,op in [('faddl','+'),('fsubl','-')]:
        s=s.replace(f'asm("fldl %1; {instruction} %2":"=t"(value):"m"(a),"m"(b));',f'value=static_cast<long double>(a){op}b;')
    s=s.replace('asm("fldl %2; fsubrl %1":"=t"(value):"m"(a),"m"(b));','value=static_cast<long double>(a)-b;')
    s=s.replace('asm("fldl %1; fchs":"=t"(value):"m"(a));','value=-static_cast<long double>(a);')
    s=s.replace('asm volatile("fldl %1; fstpl %0":"=m"(out):"m"(value):"st");','out=value;')
    if name=='self_resonance':
        # Keep the native reciprocal, floor-mode operation and binary64 spill.
        s=s.replace('asm volatile("fld1; fdivl %1" : "=t"(reciprocal) : "m"(step));','reciprocal=1.L/static_cast<long double>(step);')
        s=s.replace('long double rounded;unsigned short saved,down;','long double rounded;')
        s=s.replace('    asm volatile("fnstcw %0" : "=m"(saved));down=(saved&~0x0c00u)|0x0400u;\n','')
        s=s.replace('asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(rounded) : "0"(units),"m"(down),"m"(saved));','rounded=x87::floor(units);')
        s=s.replace('asm volatile("fldl %1; fmull %2" : "=t"(out) : "m"(spilled),"m"(step));','out=static_cast<long double>(spilled)*step;')
    s=s.replace('asm("fldl %1":"=t"(out):"m"(x));','out=x;')
    s=s.replace('asm volatile("flds %1; fstpt %0":"=m"(n):"m"(a.value):"st");','n=x87::load_float_word(a.value);')
    s=s.replace('asm volatile("fldl %0; fadds %1; fstpl %0":"+m"(n):"m"(a.value):"st","memory");','n=x87::binary64(static_cast<long double>(n)+x87::load_float_word(a.value));')
    s=s.replace('asm volatile("fldl %1; fsubl %2; fstpl %0":"=m"(out):"m"(a),"m"(b):"st");','out=x87::binary64(static_cast<long double>(a)-b);')
    s=s.replace('asm volatile("fmull %2" : "=t"(out) : "0"(left),"m"(right));','out=left*right;')
    if name=='two_port_output':
        s=s.replace('asm volatile("fstpl %0":"=m"(destination):"t"(value):"st");','destination=x87::binary64(value);')
        s=s.replace('asm volatile("fldl %1; fabs; fldl %2; fabs; fcompp; fnstsw %0" : "=a"(status) : "m"(denominator.real),"m"(denominator.imag) : "st","st(1)");','status=x87::comparison(x87::abs(static_cast<long double>(denominator.imag)),x87::abs(static_cast<long double>(denominator.real)))<<8;')
    if re.search(r'\basm\s*(?:volatile)?\s*\(',s):raise ValueError(f'Untranslated assembly in {name}')
    s=s.replace('#include "x87.hpp"','#include "x87_port.hpp"')
    # Preserve all literal bits using a binary128 host literal before conversion.
    s=re.sub(r'(?<![\w.])(?:0x[\da-f]+(?:\.[\da-f]*)?p[+-]?\d+|\d+(?:\.\d*)?(?:e[+-]?\d+)?)L\b',lambda m:'SoftReal('+m[0]+')',s,flags=re.I)
    s=s.replace('long double','SoftReal').replace('constexpr SoftReal','const SoftReal').replace('volatile SoftReal','SoftReal')
    return '// Generated by tools/port_geometry.py; do not edit.\n#include \"x87_port.hpp\"\n'+s

def source(name):
    if name=='optimization_plan':
        s=(ROOT/'vendor/asitic/src/optimization_grid.cpp').read_text()
        mm=(ROOT/'vendor/asitic/src/optimization_grid_multimetal.cpp').read_text()
        def extract_loop(body):
            loops=body[body.index('    for(long double next_length='):body.index('    o.runtime.close(file,')]
            begin=loops.index('                    const auto& construction=');end=loops.index('\n                }',begin)
            loops=loops[:begin]+'                    if(!emit(args,static_cast<unsigned>(iterations-1),user))return false;'+loops[end:]
            begin=loops.index('                    if(iterations%100==0)');end=loops.index('                    number(args,0x98,turns);',begin)
            loops=loops[:begin]+loops[end:]
            loops=loops.replace('o.runtime.flush(file,o.runtime.user);','').replace('o.prompts.chip_x','chip_x').replace('o.prompts.chip_y','chip_y')
            for needle in ['        const double length=', '            const double spacing=', '                number(args,0x88,width);', '                    ++iterations;']:
                assert loops.count(needle)==1
                loops=loops.replace(needle,'        if(++steps>100000)return false;\n'+needle)
            return loops
        loops=extract_loop(s);mm_loops=extract_loop(mm)
        predicate=s[s.index('if(load(pi.inductance)/'):s.index('\n                        print(file,',s.index('if(load(pi.inductance)/'))]
        mm_predicate=mm[mm.index('if(load(pi.inductance)/'):mm.index('\n                        print(file,',mm.index('if(load(pi.inductance)/'))]
        assert predicate==mm_predicate,'SWEEP and SWEEPMM acceptance rules differ'
        predicate=predicate.removeprefix('if(').removesuffix(')').replace('pi.inductance','inductance').replace('pi.quality_first','quality').replace('p.inductance_min','minimum').replace('p.inductance_max','maximum').replace('p.minimum_quality','minimum_quality')
        prefix='''#include "optimization_plan.hpp"
#include "asitic/optimization_radius.hpp"
#include "x87.hpp"
namespace reasitic { namespace {
long double load(const double& v){return static_cast<long double>(v);}
void number(asitic::GeometryShape& s,unsigned o,long double v){*reinterpret_cast<double*>(s.filaments.prefix+o)=asitic::x87::binary64(v);}
void word(asitic::GeometryShape& s,unsigned o,int v){__builtin_memcpy(s.filaments.prefix+o,&v,4);}
}
using namespace asitic;
'''
        setup='''    GeometryShape args{};__builtin_memcpy(args.filaments.prefix,p.shape_name,80);
    const int kind=KIND;word(args,0x50,kind);word(args,0x64,p.sides);word(args,0x68,p.metal);word(args,0x6c,p.exit_metal);
    number(args,0xa0,0.L);args.attributes=0;unsigned steps=0;int iterations=0;const double turn_step=x87::binary64(1.L/p.sides);
'''
        output=prefix
        for function,kind,body in [('enumerate_spiral_grid','p.sides==4?0:1',loops),('enumerate_multimetal_grid','p.sides==4?18:20',mm_loops)]:
            output+='bool '+function+'(const OptimizationSweepState& p,double chip_x,double chip_y,GridCandidateSink emit,void* user){\n'+setup.replace('KIND',kind)+body+'    return true;\n}\n'
        return output+'bool accept_spiral_grid(double inductance,double quality,double minimum,double maximum,double minimum_quality){return '+predicate+';}\n}\n'
    if name=='composite_geometry':
        s=(ROOT/'vendor/asitic/src/composite_geometry.cpp').read_text()
        # The original workspace contains shapes[0] through shapes[delta].
        # Allocate that many pointers rather than relying on allocator slack.
        old='runtime.construction.geometry.memory.arrays.get(static_cast<unsigned>(delta)*4u)'
        assert s.count(old)==1
        return s.replace(old,'runtime.construction.geometry.memory.arrays.get((static_cast<unsigned>(delta)+1u)*sizeof(GeometryShape*))')
    if name=='network':
        s=(ROOT/'vendor/asitic/src/network.cpp').read_text()
        begin=s.index('PiEquivalent extract_pi_equivalent(');end=s.rfind('\n}')
        helper=s[begin:end]
        helper=helper.replace('PiEquivalent extract_pi_equivalent(const TwoPortAdmittance& y,double frequency)',
            'PiEquivalent extract_pi_equivalent_with_shunts(const TwoPortAdmittance& y,double frequency,ComplexValue first,ComplexValue second)',1)
        a=helper.index('    const ComplexValue second{');b=helper.index('    const double angular=',a)
        helper=helper[:a]+'    const ComplexValue series{-y.y21.real,-y.y21.imag};\n'+helper[b:]
        return s[:end]+'\n'+helper+s[end:]
    if name=='self_resonance':
        s=(ROOT/'vendor/asitic/src/analysis_commands.cpp').read_text()
        start=s.index('namespace {\nlong double ac_res_load(')
        return '#include "asitic/analysis_commands.hpp"\n#include "x87.hpp"\nnamespace asitic {\n'+s[start:s.index('void report_input_impedance(',start)]+'}\n'
    if name=='capacitor_ports':
        s=(ROOT/'vendor/asitic/src/extended_unfolded.cpp').read_text()
        prefix=s[:s.index('bool solve_capacitive_analysis(')]
        start=s.index('        ComplexValue a,b,d;');end=s.index('        if(options.print)',start)
        return prefix+'void capacitor_port_admittance(const ComplexMatrix& cap,TwoPortAdmittance& ports) {\n'+s[start:end].replace('state.ports','ports')+'}\n}\n'
    if name=='geometry_parser':
        s=(ROOT/'vendor/asitic/src/command_runtime.cpp').read_text()
        s=s[:s.index('bool parse_command_argument(')]+s[s.index('int parse_command_arguments('):s.index('void dispatch_command_line(')]+'}\n'
        return s.replace('#include "command_catalog.inc"',(ROOT/'vendor/asitic/src/command_catalog.inc').read_text())
    if name=='geometry_dispatch':
        s=(ROOT/'vendor/asitic/src/geometry_dispatch.cpp').read_text()
        # JOINSHUNT's native PHASE fallthrough consumes the deleted source's
        # address as an integer. Its intended positive phase must not depend
        # on a process allocator's sign bit or retain a pointer as amplitude.
        start=s.index('        if(id==421) {');end=s.index('        }break;',start)
        section=s[start:end]
        section=section.replace('const int phase=integer(a.arguments[1]);','const int phase=reasitic::reference_numerics()?integer(a.arguments[1]):1;').replace('shape.attributes=a.arguments[1].value;','shape.attributes=reasitic::reference_numerics()?a.arguments[1].value:1u;')
        return '#include "numerical_policy.hpp"\n'+s[:start]+section+s[end:]
    if name=='frequency_geometry':
        # Only the wavelength/skin-depth updater and its two load helpers.
        # Keep the recovered physics, constants and last-metal rule verbatim.
        s=(ROOT/'vendor/asitic/src/analysis_reports.cpp').read_text()
        begin=s.index('long double divide_memory(');end=s.index('void update_frequency_geometry(',begin)
        return s[:begin]+'}\n'+s[end:s.index('void report_inductance_matrix(')]+'}\n'
    if name in ('potential','potential_tables'):
        s=(ROOT/'vendor/asitic/src'/(name+'.cpp')).read_text()
        # A rectangular cosine transform has nx+1 rows of ny+1 cells.
        # The original producer/lookup use nx+1 while its transform uses ny+1.
        # Keep the historical layout only behind the diagnostic boundary.
        if name=='potential':
            area='const int area_x=wrap_mul(wrap_sub(b.x1,b.x0),wrap_sub(a.x1,a.x0)),\n        area_y=wrap_mul(wrap_sub(b.y1,b.y0),wrap_sub(a.y1,a.y0));'
            wider='const double area_x=reasitic::reference_numerics()?wrap_mul(wrap_sub(b.x1,b.x0),wrap_sub(a.x1,a.x0)):reasitic::potential_index_product(wrap_sub(b.x1,b.x0),wrap_sub(a.x1,a.x0)),\n        area_y=reasitic::reference_numerics()?wrap_mul(wrap_sub(b.y1,b.y0),wrap_sub(a.y1,a.y0)):reasitic::potential_index_product(wrap_sub(b.y1,b.y0),wrap_sub(a.y1,a.y0));'
            assert s.count(area)==1
            s=s.replace(area,wider).replace('complex_scale(ExtendedValue a,int factor)','complex_scale(ExtendedValue a,double factor)')
            for axis in ('nx','ny'):
                product='wrap_mul(g.'+axis+',g.'+axis+')'
                assert s.count(product)==2
                s=s.replace(product,'(reasitic::reference_numerics()?'+product+':reasitic::potential_index_product(g.'+axis+',g.'+axis+'))')
            old='wrap_mul(rows[row],wrap_add(g.nx,1))'
            new='wrap_mul(rows[row],wrap_add(reasitic::reference_numerics()?g.nx:g.ny,1))'
        else:
            old='out.xy[x*(g.nx+1)+y]'
            new='out.xy[x*((reasitic::reference_numerics()?g.nx:g.ny)+1)+y]'
            scratch='memory.arrays.get(static_cast<unsigned>(nx)*32u)'
            assert s.count(scratch)==1
            s=s.replace(scratch,'memory.arrays.get(static_cast<unsigned>(reasitic::reference_numerics()?nx:(nx>ny?nx:ny))*32u)')
        assert s.count(old)==1
        return '#include "numerical_policy.hpp"\n#include "potential_grid.hpp"\n'+s.replace(old,new)
    if name=='quadrature':
        s=(ROOT/'vendor/asitic/src/quadrature.cpp').read_text();needle='double error_power(double x) {'
        assert s.count(needle)==1
        return '#include "portable_gmd.hpp"\n#include "numerical_policy.hpp"\n'+s.replace(needle,needle+'\n    if(!reasitic::reference_numerics())return reasitic::error_power_stable(x);',1)
    if name=='separation':
        s=(ROOT/'vendor/asitic/src/separation.cpp').read_text()
        needle='    using x87::binary64;\n    const long double x = dx, y = dy;'
        assert s.count(needle)==1
        return '#include "portable_gmd.hpp"\n#include "numerical_policy.hpp"\n'+s.replace(needle,'    if(!reasitic::reference_numerics())return reasitic::geometric_mean_distance(w1,w2,t1,t2,dx,dy).distance;\n'+needle)
    if name=='metal_area':
        s=(ROOT/'vendor/asitic/src/geometry_reporting.cpp').read_text()
        helpers=s[s.index('void segment_length_width('):s.index('long double subtract(')]
        body=s[s.index('long double geometry_metal_area('):s.index('void print_geometry_segments(')]
        return '#include "asitic/geometry_reporting.hpp"\n#include "x87.hpp"\nnamespace asitic { namespace {\n'+helpers+'}\n'+body+'}\n'
    if name=='estimate':
        # Original INDUCTANCE/COUPLING commands and their shared helpers.
        # Exclude unrelated command dispatch to avoid pulling in the whole host.
        s=(ROOT/'vendor/asitic/src/analysis_commands.cpp').read_text()
        return s[:s.index('\nnamespace {\nlong double ac_res_load') ]+'}\n'
    if name=='eddy':
        s=(ROOT/'vendor/asitic/src/eddy.cpp').read_text()
        s=s.replace('namespace asitic {','namespace asitic {\nbool eddy_aborted();',1)
        return s.replace('    return green_integral(', '    if(eddy_aborted())return 0.L;\n    return green_integral(',1)
    if name=='later_analysis':
        s=(ROOT/'vendor/asitic/src/later_analysis.cpp').read_text()
        signature='bool solve_transformer_network('
        begin=s.index('    const auto& runtime=o.commands.construction.geometry;',s.index(signature))
        end=s.index('    const GeometryShape* ground=',begin)
        prefix=s[begin:end]
        helper='GeometryShape* prepare_transformer_geometry(const GeometryShape& first,const GeometryShape& second,LaterAnalysisState& state,const LaterAnalysisOptions& o,int& boundary) {\n'+prefix.replace('const int boundary=','boundary=')+'    return joined;\n}\n'
        s=s[:begin]+'    const auto& runtime=o.commands.construction.geometry;const auto memory=runtime.memory;\n    int boundary;auto* joined=prepare_transformer_geometry(first,second,state,o,boundary);\n'+s[end:]
        s=s.replace(signature,helper+signature,1)
        needle='    ComplexMatrix inductance{};auto ind_options=o.inductance;ind_options.impedance=false;'
        s=s.replace(needle,'    if(o.inductance.snapshot)o.inductance.snapshot("ZC.out",capacitance,o.inductance.snapshot_user);\n'+needle,1)
        needle='    remove_geometry_shape(state.shapes,joined,memory);\n    if(!solve_node_equations('
        s=s.replace(needle,'    if(o.inductance.snapshot)o.inductance.snapshot("YM.out",inductance,o.inductance.snapshot_user);\n'+needle,1)
        return s
    if name=='extended_analysis':
        s=(ROOT/'vendor/asitic/src/extended_analysis.cpp').read_text();signature='bool solve_pad_deembedded_network('
        begin=s.index('    const auto& runtime=options.commands.construction.geometry;',s.index(signature))
        end=s.index('    options.combined.warnings.emit(',begin)
        helper='GeometryShape* prepare_pad_geometry(const GeometryShape& device,const GeometryShape& first_pad,const GeometryShape& second_pad,CombinedNetworkState& state,const PadDeembeddingOptions& options,GeometryShape*& pads) {\n'+s[begin:end].replace('auto* pads=','pads=')+'    return with_pads;\n}\n'
        s=s[:begin]+'    const auto& runtime=options.commands.construction.geometry;const auto memory=runtime.memory;\n    GeometryShape* pads;auto* with_pads=prepare_pad_geometry(device,first_pad,second_pad,state,options,pads);\n'+s[end:]
        s=s.replace(signature,helper+signature,1)
        needle='    remove_geometry_shape(state.shapes,pads,memory);'
        assert s.count(needle)==1
        return s.replace(needle,'    if(options.combined.snapshot) {\n        options.combined.snapshot("Pads.out",capacitance,options.combined.snapshot_user);\n        ComplexValue values[]={saved.y11,saved.y21,saved.y12,saved.y22};const ComplexMatrix ports{values,4,1,2,2,2,1};\n        options.combined.snapshot("WithPads.out",ports,options.combined.snapshot_user);\n    }\n'+needle)
    if name=='node_equations':
        s=(ROOT/'vendor/asitic/src/node_equations.cpp').read_text()
        begin=s.index('{',s.index('bool solve_node_equations('))+1
        guard='\n#ifdef __EMSCRIPTEN__\n    if(!reasitic::reference_numerics())return reasitic::solve_portable_folded_nodes(cap,ind,n,output,o);\n#endif\n'
        return '#ifdef __EMSCRIPTEN__\n#include "folded_network.hpp"\n#include "numerical_policy.hpp"\n#endif\n'+s[:begin]+guard+s[begin:]
    if name=='extended_node':
        s=(ROOT/'vendor/asitic/src/extended_node.cpp').read_text()
        signature='void solve_extended_node_equations('
        begin=s.index('{',s.index(signature))+1
        guard='\n#ifdef __EMSCRIPTEN__\n    if(!reasitic::reference_numerics()){reasitic::solve_portable_extended_nodes(cap,ind,output,memory,o);return;}\n#endif\n'
        return '#ifdef __EMSCRIPTEN__\n#include "extended_network.hpp"\n#include "numerical_policy.hpp"\n#endif\n'+s[:begin]+guard+s[begin:]
    if name=='extended_driver':
        s=(ROOT/'vendor/asitic/src/extended_twoport.cpp').read_text()
        predicate=re.search(r'    const bool known=([^;]+);',s).group(1)
        # Verbatim PIX driver and its timer helpers. Host command dispatch is
        # excluded, as it also requires unrelated analytical model state.
        s=s[:s.index('void set_options(')]+'}\n'+s[s.index('bool solve_extended_twoport_network('):s.index('bool analyze_extended_twoport(')]+'}\n'
        needle='    stop(ind_time,o);start(node_time,o);remove_geometry_shape(s.shapes,clone,memory);'
        assert s.count(needle)==1
        s=s.replace(needle,'    if(options.snapshot){options.snapshot("ZC.out",capacitance,options.snapshot_user);options.snapshot("YM.out",inductance,options.snapshot_user);}\n'+needle)
        # Keep command dispatch distinct from the direct PIX driver. The
        # original 2PORTX recognizes a smaller set of actual shape kinds.
        end=s.rindex('}')
        return s[:end]+'bool recognized_port_shape(int kind){return '+predicate+';}\nint numerical_port_model(int kind,bool extended){return kind==4?2:extended&&recognized_port_shape(kind)?1:0;}\n'+s[end:]
    if name=='port_sweep':
        s=(ROOT/'vendor/asitic/src/port_commands.cpp').read_text()
        return s[:s.index('int parse_port_parameter_format(')]+s[s.index('void run_port_frequency_sweep('):]
    if name=='port_records':
        s=(ROOT/'vendor/asitic/src/port_output.cpp').read_text()
        return s[:s.index('void write_port_shape_summary(')]+'}\n'
    if name=='three_port_analysis':
        s=(ROOT/'vendor/asitic/src/three_port_analysis.cpp').read_text();signature='bool solve_three_port_network('
        begin=s.index('    const auto& runtime=o.commands.construction.geometry;',s.index(signature));end=s.index('    const GeometryShape* ground=',begin)
        prefix=s[begin:end].replace('const int first_count=','first_count=').replace('const int second_count=','second_count=').replace('unsigned n=','n=')
        helper='GeometryShape* prepare_three_port_geometry(const GeometryShape& first,const GeometryShape& second,const GeometryShape& third,GeometryShape& reference,ThreePortAnalysisState& state,const ThreePortAnalysisOptions& o,int& first_count,int& second_count,unsigned& n) {\n'+prefix+'    // Reference conductors are already eliminated from both physical matrices.\n    if(!reasitic::reference_numerics())n=static_cast<unsigned>(count_geometry_metals(*joined,o.technology.metal_count));\n    return joined;\n}\n'
        s=s[:begin]+'    const auto& runtime=o.commands.construction.geometry;const auto memory=runtime.memory;\n    int first_count,second_count;unsigned n;auto* joined=prepare_three_port_geometry(first,second,third,reference,state,o,first_count,second_count,n);\n'+s[end:]
        s=s.replace(signature,helper+signature,1)
        needle='    auto inductance=matrix(static_cast<int>(n),memory,runtime.errors);';assert s.count(needle)==1
        s=s.replace(needle,'    if(o.inductance.snapshot)o.inductance.snapshot("ZC.out",capacitance,o.inductance.snapshot_user);\n'+needle)
        needle='    remove_geometry_shape(state.shapes,joined,memory);\n    if(!solve_three_port_equations(';assert s.count(needle)==1
        s=s.replace(needle,'    if(o.inductance.snapshot)o.inductance.snapshot("YM.out",inductance,o.inductance.snapshot_user);\n'+needle)
        s=s.replace('namespace asitic {','namespace asitic {\nbool run_three_port_equations(ComplexMatrix&,const ComplexMatrix&,int,int,int,ThreePortImpedance&,SolverMemory,const NodeEquationOptions&);',1)
        return '#include \"numerical_policy.hpp\"\n'+s.replace('solve_three_port_equations(capacitance.data,inductance.data,','run_three_port_equations(capacitance,inductance,',1)
    if name=='potential_solver':
        s=(ROOT/'vendor/asitic/src/potential_solver.cpp').read_text()
        s=s.replace('namespace asitic {','namespace asitic {\nvoid record_capacitance_totals(double,double);',1)
        needle='const double capacitance=binary64(-1.L/((static_cast<long double>(frequency)*0x1.921fb54442d18p+2)*z.imag));'
        assert s.count(needle)==1
        return s.replace(needle,needle+'\n        record_capacitance_totals(capacitance,z.real);')
    if name=='native_creators':
        s=(ROOT/'vendor/asitic/src/console_geometry.cpp').read_text()
        s=s[:s.index('void initialize_console_geometry(')]
        # Admission guards reject native fault/resource cases before allocation;
        # descriptor values and the original construction bodies are untouched.
        s=s.replace('namespace asitic {','namespace asitic {\nvoid guard_native_creation(int,const ConsoleGeometryState&);',1)
        s=re.sub(r'(build_\w+\(s.head)',r'guard_native_creation(id,s);\1',s)
        return s+'bool create_native_geometry(int id,CommandArguments& a,ConsoleGeometryState& state,bool& suppressed) { return create(id,a,state,suppressed); }\n}\n'
    if name=='argument_value':
        s=(ROOT/'vendor/asitic/src/command_runtime.cpp').read_text()
        helpers=s[s.index('bool equal('):s.index('const CommandDefinition* native_command_catalog()')]
        body=s[s.index('bool parse_command_argument('):s.index('int parse_command_arguments(')]
        return '#include \"asitic/command_runtime.hpp\"\n#include \"x87.hpp\"\nnamespace asitic { namespace {\n'+helpers+body+'}\n'
    if name=='input_normalization':
        s=(ROOT/'vendor/asitic/src/technology_text.cpp').read_text()
        return s[:s.index('namespace {')]+'}\n'
    return (ROOT/'vendor/asitic/src'/(name+'.cpp')).read_text()

def generate(clang=None):
    target=ROOT/'build/port';target.mkdir(parents=True,exist_ok=True)
    # Public interfaces must use the same retained type as their bodies.
    for p in (ROOT/'vendor/asitic/include/asitic').glob('*.hpp'):
        out=target/'asitic'/p.name;out.parent.mkdir(exist_ok=True)
        out.write_text(p.read_text().replace('long double','SoftReal').replace('static_assert(__LDBL_MANT_DIG__ == 64,', 'static_assert(sizeof(SoftReal) == 16,'))
    proof={}
    for name in SOURCES:
        native=source(name)
        if name.startswith('shape_') or name=='native_creators' or name in ('legacy_complex','potential','potential_tables','potential_cache','contacts','potential_solver','green','green_integration','quadrature','quadrature_wrappers','oscillatory','eddy','technology','network','node_equations','combined_network','later_analysis','three_port','three_port_analysis','three_port_transforms','two_port_output','port_sweep','port_records','extended_node','extended_driver','extended_analysis','frequency_geometry','geometry_parser','geometry_edit','geometry_dispatch','self_resonance','capacitor_ports'):
            native,proof[name]=expose(name,native,clang)
        (target/(name+'.cpp')).write_text(translate(name,native))
    evidence=ROOT/'build/evidence';evidence.mkdir(exist_ok=True)
    (evidence/'implicit-stores.json').write_text(json.dumps(proof,indent=2)+'\n')
    return target
if __name__=='__main__':generate()
