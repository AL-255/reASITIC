#include "asitic/extended_analysis_dispatch.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double number(const CommandArgument& argument) { long double value;asm volatile("flds %1":"=t"(value):"m"(argument.value));return value; }
GeometryShape& shape(const CommandArgument& argument) { return *reinterpret_cast<GeometryShape*>(argument.value); }
void update(CombinedNetworkOptions& options,const FrequencyGeometryLimits& cells) {
    options.discretization={cells.max_length,cells.capacitance_max_length};
    options.capacitance.contacts.length_step=cells.capacitance_max_length;options.capacitance.contacts.width_step=cells.capacitance_max_width;
    options.inductance.filaments.edge_cell_size=cells.max_width;options.inductance.thickness_cell=cells.max_thickness;
}
void configure(double frequency,ExtendedAnalysisCommandContext& context) {
    if(!context.automatic_cells)return;
    configure_automatic_cells(frequency,context.cells,context.cell_runtime);
    if(context.pad_options)update(context.pad_options->combined,context.cells.limits);
    if(context.extended_options)update(context.extended_options->network,context.cells.limits);
}
void grounded_sweep(PortSweepKind,const GeometryShape* const* shapes,double termination,
    const PortSweepSettings& settings,const PortSweepRuntime& runtime) {
    run_port_frequency_sweep(PortSweepKind::Ground,shapes,termination,settings,runtime);
}
}
bool execute_extended_analysis_command(int id,CommandArguments& arguments,ExtendedAnalysisCommandContext& context) {
    if(id!=518 && id!=538 && id!=528)return false;
    if(id==528) {
        // Both command rows have the same range, FAST/SLOW, filename and
        // optional-reference positions. Their native export entries differ.
        auto runtime=*context.port_runtime;runtime.sweep=grounded_sweep;
        return execute_port_command(539,arguments,*context.port_settings,runtime);
    }
    auto& a=arguments.arguments;GeometryShape& first=shape(a[0]);
    const double frequency=x87::binary64(number(a[1]));
    if(id==518) {
        GeometryShape& first_pad=shape(a[2]);GeometryShape& second_pad=shape(a[3]);
        GeometryShape& reference=static_cast<int>(arguments.command.value)>4?shape(a[4]):first;
        configure(frequency,context);
        auto options=*context.pad_options;options.print=true;
        context.backends.pad(first,first_pad,second_pad,reference,frequency,*context.pad_state,options);
    } else {
        configure(frequency,context);
        GeometryShape& reference=static_cast<int>(arguments.command.value)>2?shape(a[2]):first;
        auto options=*context.extended_options;options.network.print=true;
        context.backends.extended(first,reference,frequency,*context.extended_state,options);
    }
    return true;
}
}
