#include "asitic/later_analysis.hpp"
#include "asitic/legacy_complex.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& x) { long double r;asm("fldl %1":"=t"(r):"m"(x));return r; }
void raw_copy(ComplexValue& dst,const ComplexValue& src) { __builtin_memcpy(&dst,&src,sizeof(dst)); }
bool equal_name(const char* a,const char* b) { while(*a && *a==*b){++a;++b;}return *a==*b; }
void destroy(const ComplexMatrix& matrix,SolverMemory memory) { if(!matrix.vector_reference)memory.put(matrix.data); }
}
LaterTwoPortImpedance invert_transformer_admittance(const TwoPortAdmittance& y) {
    const long double ar=load(y.y11.real),ai=load(y.y11.imag),br=load(y.y22.real),bi=load(y.y22.imag);
    const long double pr=br*ar-bi*ai,pi=ar*bi+br*ai;
    // These native stores round a discarded copy while the live products
    // remain extended. Retain the stores for their floating-point effects.
    volatile double dead_pr=pr,dead_pi=pi;(void)dead_pr;(void)dead_pi;
    const long double cr=load(y.y21.real),ci=load(y.y21.imag);
    const long double sr=cr*cr-ci*ci,si=(cr*ci)+(cr*ci);
    volatile double dead_sr=sr,dead_si=si;(void)dead_sr;(void)dead_si;
    const ComplexValue determinant{x87::binary64(pr-sr),x87::binary64(pi-si)};
    LaterTwoPortImpedance out;
    // The original labels use y11 for z11, rather than swapping diagonals.
    out.z11=legacy::divide(y.y11,determinant);out.z22=legacy::divide(y.y22,determinant);
    const ComplexValue negative{x87::binary64(-load(y.y21.real)),x87::binary64(-load(y.y21.imag))};
    out.z12=legacy::divide(negative,determinant);raw_copy(out.z21,out.z12);return out;
}
TransformerEquivalent extract_transformer_equivalent(const TwoPortAdmittance& y,double frequency,LaterTwoPortImpedance& z) {
    const long double f=load(frequency);z=invert_transformer_admittance(y);
    const long double ai=load(z.z11.imag),ci=load(z.z12.imag);
    volatile double dead_first=ai-ci;(void)dead_first;
    const long double cr=load(z.z12.real),ar=load(z.z11.real);
    const long double omega=f*0x1.921fb54442d18p+2L;
    const long double l1=ai/omega,r1=ar-cr;
    volatile double dead_r1=r1;(void)dead_r1;
    const long double bi=load(z.z22.imag),l2=bi/omega,m=-ci/omega;
    const long double r2=load(z.z22.real)-cr;
    volatile double dead_r2=r2,dead_second=bi-ci;(void)dead_r2;(void)dead_second;
    // Coupling uses the live extended inductances, before their output spills.
    const long double coupling=m/x87::sqrt(l1*l2);
    return {x87::binary64(l1),x87::binary64(l2),x87::binary64(r1),x87::binary64(r2),x87::binary64(cr),x87::binary64(m),x87::binary64(coupling)};
}
bool solve_transformer_network(const GeometryShape& first,const GeometryShape& second,const GeometryShape& reference,
    double frequency,double termination,LaterAnalysisState& state,const LaterAnalysisOptions& o) {
    const auto& runtime=o.commands.construction.geometry;const auto memory=runtime.memory;
    auto* a=clone_geometry_shape(state.shapes,first,o.discretization,runtime);
    auto* b=clone_geometry_shape(state.shapes,second,o.discretization,runtime);
    const int boundary=count_geometry_metals(*a,o.technology.metal_count);
    auto* joined=copy_geometry_shape(state.shapes,*a,nullptr,1e15,-1e15,-1,o.technology,o.commands);
    copy_geometry_shape(state.shapes,*b,nullptr,1e15,-1e15,-1,o.technology,o.commands);
    reverse_geometry_segments(*state.shapes);
    join_geometry_shapes(state.shapes,*joined,*state.shapes,o.technology,o.commands);
    remove_geometry_shape(state.shapes,a,memory);remove_geometry_shape(state.shapes,b,memory);
    const GeometryShape* ground=equal_name(reinterpret_cast<const char*>(&first),reinterpret_cast<const char*>(&reference))?joined:&reference;
    ComplexMatrix capacitance{};auto cap_options=o.capacitance;cap_options.verbose=false;
    if(!solve_capacitance(joined->filaments,ground->filaments,capacitance,frequency,state.capacitance,cap_options,memory)) {
        o.warnings.emit("Error:  Could not generate capacitance matrix.");remove_geometry_shape(state.shapes,joined,memory);destroy(capacitance,memory);return false;
    }
    long double shunt;
    if(0x1.b7cdfd9d7bdbbp-34L>load(termination))shunt=1e15L;
    else asm("fld1; fdivl %1":"=t"(shunt):"m"(termination));
    volatile double dead_shunt=shunt;(void)dead_shunt;
    auto& cell=capacitance.data[boundary*capacitance.stride+boundary];
    long double updated;asm("faddl %2":"=t"(updated):"0"(shunt),"m"(cell.real));cell.real=x87::binary64(updated);
    const double zero=0;asm("fldl %1; faddl %2":"=t"(updated):"m"(cell.imag),"m"(zero));cell.imag=x87::binary64(updated);
    ComplexMatrix inductance{};auto ind_options=o.inductance;ind_options.impedance=false;
    if(!solve_inductance(joined->filaments,inductance,frequency,ind_options,o.eddy,memory,o.warnings)) {
        o.warnings.emit("Error:  Could not generate inductance matrix.");remove_geometry_shape(state.shapes,joined,memory);destroy(inductance,memory);destroy(capacitance,memory);return false;
    }
    remove_geometry_shape(state.shapes,joined,memory);
    if(!solve_node_equations(capacitance.data,inductance.data,capacitance.rows,state.ports,memory,o.nodes)) { destroy(inductance,memory);destroy(capacitance,memory);return false; }
    state.model=extract_transformer_equivalent(state.ports,frequency,state.impedance);
    if(o.print && o.report)o.report(state.model,frequency,o.report_user);
    destroy(inductance,memory);destroy(capacitance,memory);return true;
}
}
