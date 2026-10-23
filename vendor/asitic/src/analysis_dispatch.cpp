#include "asitic/analysis_dispatch.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double number(const CommandArgument& value) {
    long double r;asm volatile("flds %1":"=t"(r):"m"(value.value));return r;
}
GeometryShape& shape(const CommandArgument& value) { return *reinterpret_cast<GeometryShape*>(value.value); }
void update_options(AnalysisCommandContext& context) {
    const auto& c=context.cells;
    if(context.combined_options) {
        auto& o=*context.combined_options;o.discretization={c.limits.max_length,c.limits.capacitance_max_length};
        o.capacitance.contacts.length_step=c.limits.capacitance_max_length;o.capacitance.contacts.width_step=c.limits.capacitance_max_width;
        o.inductance.filaments.edge_cell_size=c.limits.max_width;o.inductance.thickness_cell=c.limits.max_thickness;
    }
    if(context.transformer_options) {
        auto& o=*context.transformer_options;o.discretization={c.limits.max_length,c.limits.capacitance_max_length};
        o.capacitance.contacts.length_step=c.limits.capacitance_max_length;o.capacitance.contacts.width_step=c.limits.capacitance_max_width;
        o.inductance.filaments.edge_cell_size=c.limits.max_width;o.inductance.thickness_cell=c.limits.max_thickness;
    }
}
}
void configure_automatic_cells(double frequency,AutomaticCellState& state,const AutomaticCellRuntime& runtime) {
    static_assert(sizeof(MetalLayer)==sizeof(TechnologyMetal));
    const AnalysisReportRuntime report{runtime.message,runtime.format,runtime.output,nullptr,nullptr,nullptr,nullptr};
    update_frequency_geometry(frequency,reinterpret_cast<const TechnologyMetal*>(runtime.metals),runtime.metal_count,
        state.alpha,state.beta,state.limits,state.print!=0,report);
}

bool execute_analysis_command(int id,const CommandArguments& arguments,AnalysisCommandContext& context) {
    if(id!=515 && id!=517 && id!=523)return false;
    const auto& a=arguments.arguments;const int count=static_cast<int>(arguments.command.value);
    GeometryShape& first=shape(a[0]);GeometryShape* second=id==515?nullptr:&shape(a[1]);
    const double frequency=x87::binary64(number(a[id==515?1:2]));
    if(context.automatic_cells) {
        configure_automatic_cells(frequency,context.cells,context.cell_runtime);update_options(context);
    }
    if(id==523) {
        GeometryShape& reference=count>3?shape(a[3]):first;
        const double termination=count>4?x87::binary64(number(a[4])):0.;
        auto options=*context.transformer_options;options.print=true;
        context.backends.transformer(first,*second,reference,frequency,termination,*context.transformer_state,options);
    } else {
        if(id==515)second=count>2?&shape(a[2]):&first;
        auto options=*context.combined_options;options.print=true;
        context.backends.combined(first,*second,frequency,*context.combined_state,options,context.geometry);
    }
    return true;
}
}
