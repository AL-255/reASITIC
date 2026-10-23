#include "asitic/combined_network.hpp"

namespace asitic {
namespace {
void destroy_matrix(const ComplexMatrix& m,SolverMemory memory) { if(!m.vector_reference)memory.put(m.data); }
}
bool solve_combined_network(const GeometryShape& first,const GeometryShape& second,double frequency,
    CombinedNetworkState& state,const CombinedNetworkOptions& options,const GeometryRuntime& runtime) {
    int start=0;
    if(options.runtime.time)options.runtime.time(&start,options.runtime.user);
    auto* clone=clone_geometry_shape(state.shapes,first,options.discretization,runtime);
    ComplexMatrix capacitance{};auto cap_options=options.capacitance;cap_options.verbose=false;
    if(!solve_capacitance(clone->filaments,second.filaments,capacitance,frequency,state.capacitance,cap_options,runtime.memory)) {
        options.warnings.emit("Error:  Could not generate capacitance matrix.");
        remove_geometry_shape(state.shapes,clone,runtime.memory);destroy_matrix(capacitance,runtime.memory);return false;
    }
    ComplexMatrix inductance{};auto ind_options=options.inductance;ind_options.impedance=false;
    if(!solve_inductance(clone->filaments,inductance,frequency,ind_options,options.eddy,runtime.memory,options.warnings)) {
        options.warnings.emit("Error:  Could not generate inductance matrix.");
        remove_geometry_shape(state.shapes,clone,runtime.memory);destroy_matrix(inductance,runtime.memory);destroy_matrix(capacitance,runtime.memory);return false;
    }
    remove_geometry_shape(state.shapes,clone,runtime.memory);
    if(options.snapshot) {
        options.snapshot("ZC.out",capacitance,options.snapshot_user);options.snapshot("YM.out",inductance,options.snapshot_user);
    }
    if(!solve_node_equations(capacitance.data,inductance.data,capacitance.rows,state.ports,runtime.memory,options.nodes)) {
        destroy_matrix(inductance,runtime.memory);destroy_matrix(capacitance,runtime.memory);return false;
    }
    if(options.timing) {
        int end=0;if(options.runtime.time)options.runtime.time(&end,options.runtime.user);
        const double elapsed=options.runtime.difference?options.runtime.difference(end,start,options.runtime.user):static_cast<double>(end)-start;
        if(options.runtime.elapsed)options.runtime.elapsed(elapsed,options.runtime.user);
    }
    state.pi=extract_pi_equivalent(state.ports,frequency);
    if(options.print && options.runtime.report)options.runtime.report(state.pi,frequency,options.runtime.user);
    destroy_matrix(inductance,runtime.memory);destroy_matrix(capacitance,runtime.memory);return true;
}
}
