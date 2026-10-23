#include "asitic/extended_twoport.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
void destroy(const ComplexMatrix& matrix,SolverMemory memory) { if(!matrix.vector_reference)memory.put(matrix.data); }
void start(InductanceTimer& timer,const ExtendedTwoPortOptions& o) { if(o.time)o.time(timer.first,timer.zone,o.user); }
void stop(InductanceTimer& timer,const ExtendedTwoPortOptions& o) {
    if(o.time)o.time(timer.last,timer.zone,o.user);
    timer.accumulated=static_cast<int>(static_cast<unsigned>(timer.accumulated)+static_cast<unsigned>(inductance_elapsed_milliseconds(timer)));
}
void set_options(CombinedNetworkOptions& o,const ExtendedTwoPortDispatchContext& context) {
    const auto& c=context.limits;
    o.discretization={c.max_length,c.capacitance_max_length};o.capacitance.contacts.length_step=c.capacitance_max_length;
    o.capacitance.contacts.width_step=c.capacitance_max_width;o.inductance.filaments.edge_cell_size=c.max_width;o.inductance.thickness_cell=c.max_thickness;
    o.capacitance.progress=context.progress!=0;o.inductance.reporting.progress=context.progress!=0;o.nodes.progress=context.progress!=0;
}
bool dispatch(bool extended,const GeometryShape& shape,double frequency,int analytical,const GeometryShape& ground,unsigned char progress,ExtendedTwoPortDispatchContext& c) {
    const unsigned char saved=c.progress;c.progress=progress;
    if(c.automatic_cells)update_frequency_geometry(frequency,reinterpret_cast<const TechnologyMetal*>(c.analytical_options.metals),c.analytical_options.metal_count,c.alpha,c.beta,c.limits,c.progress!=0,c.report);
    const int kind=*reinterpret_cast<const int*>(shape.filaments.prefix+0x50);
    const bool known=kind==0 || kind==1 || kind==16 || kind==17 || kind==18;
    bool result;
    if(kind==4) {
        auto& o=*c.capacitive_options;o.capacitance.progress=c.progress!=0;
        o.capacitance.contacts.length_step=c.limits.capacitance_max_length;o.capacitance.contacts.width_step=c.limits.capacitance_max_width;
        auto options=o;options.print=true;result=solve_capacitive_analysis(shape,ground,frequency,*c.capacitive_state,options,c.geometry.memory);
    } else {
        if(!known)c.report.warnings.emit("Warning:  User defined structure type may not be compatible with 2port command.");
        if(analytical) {
            auto options=c.analytical_options;options.kind=kind;options.sides=*reinterpret_cast<const int*>(shape.filaments.prefix+0x64);
            options.spacing=*reinterpret_cast<const double*>(shape.filaments.prefix+0x90);options.turns=*reinterpret_cast<const double*>(shape.filaments.prefix+0x98);
            result=solve_analytical_model(shape.filaments,frequency,options,*c.analytical_history,*c.analytical_result,c.report.warnings);
            if(result && c.analytical_report)c.analytical_report(c.analytical_result->pi,frequency,c.analytical_report_user);
        } else if(extended && known) {
            auto& o=*c.extended_options;set_options(o.network,c);auto options=o;options.network.print=false;
            result=solve_extended_twoport_network(shape,ground,frequency,*c.extended_state,options);
        } else {
            auto& o=*c.combined_options;set_options(o,c);auto options=o;options.print=false;
            result=solve_combined_network(shape,ground,frequency,*c.combined_state,options,c.geometry);
        }
    }
    c.progress=saved;
    if(c.combined_options) { c.combined_options->capacitance.progress=saved!=0;c.combined_options->inductance.reporting.progress=saved!=0;c.combined_options->nodes.progress=saved!=0; }
    if(c.extended_options) { c.extended_options->network.capacitance.progress=saved!=0;c.extended_options->network.inductance.reporting.progress=saved!=0;c.extended_options->network.nodes.progress=saved!=0; }
    if(c.capacitive_options)c.capacitive_options->capacitance.progress=saved!=0;
    return result;
}
}
bool solve_extended_twoport_network(const GeometryShape& first,const GeometryShape& ground,double frequency,
    ExtendedTwoPortState& state,const ExtendedTwoPortOptions& o) {
    auto& s=state.network;const auto& options=o.network;const auto memory=o.geometry.memory;
    InductanceTimer total{},cap_time{},ind_time{},node_time{};start(total,o);
    long double f;asm volatile("fldl %1":"=t"(f):"m"(frequency));state.frequency=x87::binary64(f);
    auto* clone=clone_geometry_shape(s.shapes,first,options.discretization,o.geometry);
    start(cap_time,o);ComplexMatrix capacitance{};auto cap_options=options.capacitance;
    if(!solve_unfolded_capacitance(clone->filaments,ground.filaments,capacitance,frequency,s.capacitance,cap_options,memory)) {
        options.warnings.emit("Error:  Could not generate capacitance matrix.");remove_geometry_shape(s.shapes,clone,memory);destroy(capacitance,memory);return false;
    }
    stop(cap_time,o);start(ind_time,o);ComplexMatrix inductance{};auto ind_options=options.inductance;ind_options.impedance=true;
    if(!solve_inductance(clone->filaments,inductance,frequency,ind_options,options.eddy,memory,options.warnings)) {
        options.warnings.emit("Error:  Could not generate inductance matrix.");remove_geometry_shape(s.shapes,clone,memory);destroy(inductance,memory);destroy(capacitance,memory);return false;
    }
    stop(ind_time,o);start(node_time,o);remove_geometry_shape(s.shapes,clone,memory);
    auto nodes=o.nodes;nodes.frequency=state.frequency;solve_extended_node_equations(capacitance,inductance,s.ports,memory,nodes);
    stop(node_time,o);stop(total,o);char buffer[512];char* message=o.message?o.message:buffer;
    o.format(message,"Calc Times (ms):  total = %5.2ld, cap = %5.2ld, ind = %5.2ld, node = %5.2ld",total.accumulated,cap_time.accumulated,ind_time.accumulated,node_time.accumulated);
    if(options.timing)options.warnings.emit(message);
    s.pi=extract_pi_equivalent(s.ports,frequency);if(options.print && options.runtime.report)options.runtime.report(s.pi,frequency,options.runtime.user);
    destroy(inductance,memory);destroy(capacitance,memory);return true;
}
bool analyze_extended_twoport(const GeometryShape& shape,double frequency,int analytical,const GeometryShape& ground,unsigned char progress,ExtendedTwoPortDispatchContext& c) { return dispatch(true,shape,frequency,analytical,ground,progress,c); }
bool analyze_grounded_twoport(const GeometryShape& shape,double frequency,int analytical,const GeometryShape& ground,unsigned char progress,ExtendedTwoPortDispatchContext& c) { return dispatch(false,shape,frequency,analytical,ground,progress,c); }
}
