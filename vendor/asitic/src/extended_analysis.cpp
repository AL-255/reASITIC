#include "asitic/extended_analysis.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
double sum_memory(const double& a,const double& b) { long double value;asm("fldl %1; faddl %2":"=t"(value):"m"(a),"m"(b));return x87::binary64(value); }
double subtract_memory(const double& a,const double& b) { long double value;asm("fldl %1; fsubl %2":"=t"(value):"m"(a),"m"(b));return x87::binary64(value); }
double reverse_subtract_memory(const double& a,const double& b) { long double value;asm("fldl %2; fsubrl %1":"=t"(value):"m"(a),"m"(b));return x87::binary64(value); }
double negate_memory(const double& a) { long double value;asm("fldl %1; fchs":"=t"(value):"m"(a));return x87::binary64(value); }
}
bool solve_pad_deembedded_network(const GeometryShape& device,const GeometryShape& first_pad,
    const GeometryShape& second_pad,const GeometryShape& reference,double frequency,
    CombinedNetworkState& state,const PadDeembeddingOptions& options) {
    const auto& runtime=options.commands.construction.geometry;const auto memory=runtime.memory;
    auto* with_pads=copy_geometry_shape(state.shapes,first_pad,nullptr,1e15,-1e15,-1,options.technology,options.commands);
    auto* other=copy_geometry_shape(state.shapes,second_pad,nullptr,1e15,-1e15,-1,options.technology,options.commands);
    auto* winding=clone_geometry_shape(state.shapes,device,options.combined.discretization,runtime);
    join_geometry_shapes(state.shapes,*with_pads,*winding,options.technology,options.commands);
    join_geometry_shapes(state.shapes,*with_pads,*other,options.technology,options.commands);
    auto* pads=copy_geometry_shape(state.shapes,first_pad,nullptr,1e15,-1e15,-1,options.technology,options.commands);
    other=copy_geometry_shape(state.shapes,second_pad,nullptr,1e15,-1e15,-1,options.technology,options.commands);
    join_geometry_shapes(state.shapes,*pads,*other,options.technology,options.commands);
    options.combined.warnings.emit("Performing calculation with pad structure...");
    auto combined=options.combined;combined.print=true;
    solve_combined_network(*with_pads,reference,frequency,state,combined,runtime);
    remove_geometry_shape(state.shapes,with_pads,memory);
    TwoPortAdmittance saved;__builtin_memcpy(&saved,&state.ports,sizeof(saved));
    ComplexMatrix capacitance{};auto cap_options=options.combined.capacitance;cap_options.verbose=false;cap_options.contact_history=options.pad_contact_history;
    solve_capacitance(pads->filaments,reference.filaments,capacitance,frequency,state.capacitance,cap_options,memory);
    remove_geometry_shape(state.shapes,pads,memory);
    const auto& a=capacitance.data[0];const auto& b=capacitance.data[1];const auto& d=capacitance.data[capacitance.stride+1];
    const ComplexValue first{sum_memory(a.real,b.real),sum_memory(a.imag,b.imag)};
    const ComplexValue second{sum_memory(d.real,b.real),sum_memory(d.imag,b.imag)};
    const ComplexValue cross{negate_memory(b.real),negate_memory(b.imag)};
    state.ports.y22={subtract_memory(saved.y22.real,first.real),subtract_memory(saved.y22.imag,first.imag)};
    state.ports.y11={subtract_memory(saved.y11.real,second.real),subtract_memory(saved.y11.imag,second.imag)};
    state.ports.y21={subtract_memory(saved.y21.real,cross.real),subtract_memory(saved.y21.imag,cross.imag)};
    state.ports.y12={reverse_subtract_memory(saved.y12.real,cross.real),reverse_subtract_memory(saved.y12.imag,cross.imag)};
    state.pi=extract_pi_equivalent(state.ports,frequency);
    if(options.print && options.combined.runtime.report)options.combined.runtime.report(state.pi,frequency,options.combined.runtime.user);
    if(!capacitance.vector_reference)memory.put(capacitance.data);
    return true;
}
}
