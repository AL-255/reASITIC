"""Generate WASM bodies from hash-pinned recovered C++ without editing vendor.

Only enumerated x87 instruction patterns are translated. Unknown assembly is a
hard error. Each long double arithmetic operation uses software binary80.
"""
from pathlib import Path
import re,json
from implicit_stores import expose
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['geometry','shape_construction','spiral_construction','symmetric_construction','winding_construction','transformer3d','composite_geometry','resistance','numerics','rectangular','separation','parallel','intersection','canonicalize','planar','spatial','segment','estimate','filaments','filament_setup','impedance','vias','complex','symmetric','blocked','general','solver','argument_value','input_normalization','shape_prompts','shape_prompt_symmetric','shape_prompt_transformer','shape_prompt_ring_via','shape_creation_commands','shape_creation_primitives','shape_creation_symmetric','shape_creation_sympoly','shape_creation_transformer','shape_creation_mirror','native_creators','legacy_complex','potential','potential_tables','potential_cache','contacts','potential_solver','green','green_integration','quadrature','quadrature_wrappers','oscillatory','eddy','technology','network','node_equations','combined_network','later_analysis','three_port','three_port_analysis','three_port_transforms','port_records']

def translate(name,s):
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
    if re.search(r'\basm\s*(?:volatile)?\s*\(',s):raise ValueError(f'Untranslated assembly in {name}')
    s=s.replace('#include "x87.hpp"','#include "x87_port.hpp"')
    # Preserve all literal bits using a binary128 host literal before conversion.
    s=re.sub(r'(?<![\w.])(?:0x[\da-f]+(?:\.[\da-f]*)?p[+-]?\d+|\d+(?:\.\d*)?(?:e[+-]?\d+)?)L\b',lambda m:'SoftReal('+m[0]+')',s,flags=re.I)
    s=s.replace('long double','SoftReal').replace('constexpr SoftReal','const SoftReal').replace('volatile SoftReal','SoftReal')
    return '// Generated by tools/port_geometry.py; do not edit.\n#include \"x87_port.hpp\"\n'+s

def source(name):
    if name=='estimate':
        # A verbatim extraction of the original INDUCTANCE command and helpers.
        # Exclude unrelated command dispatch to avoid pulling in the whole host.
        s=(ROOT/'vendor/asitic/src/analysis_commands.cpp').read_text()
        return s[:s.index('long double shape_coupling_estimate(')]+'}\n'
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
    if name=='port_records':
        s=(ROOT/'vendor/asitic/src/port_output.cpp').read_text()
        return s[:s.index('void write_port_shape_summary(')]+'}\n'
    if name=='three_port_analysis':
        s=(ROOT/'vendor/asitic/src/three_port_analysis.cpp').read_text();signature='bool solve_three_port_network('
        begin=s.index('    const auto& runtime=o.commands.construction.geometry;',s.index(signature));end=s.index('    const GeometryShape* ground=',begin)
        prefix=s[begin:end].replace('const int first_count=','first_count=').replace('const int second_count=','second_count=').replace('unsigned n=','n=')
        helper='GeometryShape* prepare_three_port_geometry(const GeometryShape& first,const GeometryShape& second,const GeometryShape& third,GeometryShape& reference,ThreePortAnalysisState& state,const ThreePortAnalysisOptions& o,int& first_count,int& second_count,unsigned& n) {\n'+prefix+'    return joined;\n}\n'
        s=s[:begin]+'    const auto& runtime=o.commands.construction.geometry;const auto memory=runtime.memory;\n    int first_count,second_count;unsigned n;auto* joined=prepare_three_port_geometry(first,second,third,reference,state,o,first_count,second_count,n);\n'+s[end:]
        s=s.replace(signature,helper+signature,1)
        needle='    auto inductance=matrix(static_cast<int>(n),memory,runtime.errors);';assert s.count(needle)==1
        s=s.replace(needle,'    if(o.inductance.snapshot)o.inductance.snapshot("ZC.out",capacitance,o.inductance.snapshot_user);\n'+needle)
        needle='    remove_geometry_shape(state.shapes,joined,memory);\n    if(!solve_three_port_equations(';assert s.count(needle)==1
        s=s.replace(needle,'    if(o.inductance.snapshot)o.inductance.snapshot("YM.out",inductance,o.inductance.snapshot_user);\n'+needle)
        s=s.replace('namespace asitic {','namespace asitic {\nbool run_three_port_equations(ComplexMatrix&,const ComplexMatrix&,int,int,int,ThreePortImpedance&,SolverMemory,const NodeEquationOptions&);',1)
        return s.replace('solve_three_port_equations(capacitance.data,inductance.data,','run_three_port_equations(capacitance,inductance,',1)
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
        if name.startswith('shape_') or name=='native_creators' or name in ('legacy_complex','potential','potential_tables','potential_cache','contacts','potential_solver','green','green_integration','quadrature','quadrature_wrappers','oscillatory','eddy','technology','network','node_equations','combined_network','later_analysis','three_port','three_port_analysis','three_port_transforms','port_records'):
            native,proof[name]=expose(name,native,clang)
        (target/(name+'.cpp')).write_text(translate(name,native))
    evidence=ROOT/'build/evidence';evidence.mkdir(exist_ok=True)
    (evidence/'implicit-stores.json').write_text(json.dumps(proof,indent=2)+'\n')
    return target
if __name__=='__main__':generate()
