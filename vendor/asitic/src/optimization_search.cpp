#include "asitic/optimization_search.hpp"
#include "asitic/optimization_radius.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
double loaded_number(const double& value) {
    double result;asm volatile("fldl %1;fstpl %0" : "=m"(result) : "m"(value) : "st");return result;
}
void put_integer(GeometryShape& shape,unsigned offset,int value) {
    __builtin_memcpy(shape.filaments.prefix+offset,&value,4);
}
void put_number(GeometryShape& shape,unsigned offset,long double value) {
    *reinterpret_cast<double*>(shape.filaments.prefix+offset)=x87::binary64(value);
}
void copy_name(char* destination,const char* source) {
    while((*destination++=*source++)) {}
}
void message(const SquareOptimizationRuntime& runtime,const char* text) {
    if(runtime.message)runtime.message(text,true,runtime.user);
}
const SquareConstructionHistory& construction_history(const SquareOptimizationHistory& history,unsigned call) {
    return history.construction_at?*history.construction_at(call,history.user):history.construction[call];
}
void analyze(SquareOptimizationState& state,const SquareOptimizationOptions& options,
             const GeometryShape& arguments,double frequency,unsigned call,bool report) {
    auto model=options.analytical;
    model.kind=0;model.sides=4;
    __builtin_memcpy(&model.spacing,arguments.filaments.prefix+0x90,8);
    __builtin_memcpy(&model.turns,arguments.filaments.prefix+0x98,8);
    auto history=options.history.analytical_at?*options.history.analytical_at(call,options.history.user):options.history.analytical[call];
    solve_analytical_model(state.shapes->filaments,frequency,model,history,state.model,options.warnings);
    if(report && options.runtime.model_report)options.runtime.model_report(state.model.pi,options.runtime.user);
}
}
bool optimize_square_spiral(SquareOptimizationState& state,const SquareOptimizationOptions& options,int skip_prompt) {
    if(!skip_prompt && options.runtime.prompt)options.runtime.prompt(state,options.runtime.user);
    auto p=state.parameters;
    p.target_inductance_nh=loaded_number(p.target_inductance_nh);
    volatile long double accuracy_value=static_cast<long double>(p.accuracy_percent)*0x1.47ae147ae147bp-7L;
    p.dimension_x=loaded_number(p.dimension_x);p.dimension_y=loaded_number(p.dimension_y);p.frequency=loaded_number(p.frequency);
    p.spacing.minimum=loaded_number(p.spacing.minimum);p.spacing.step=loaded_number(p.spacing.step);p.spacing.maximum=loaded_number(p.spacing.maximum);
    p.width.minimum=loaded_number(p.width.minimum);p.width.step=loaded_number(p.width.step);p.width.maximum=loaded_number(p.width.maximum);
    GeometryShape arguments;__builtin_memcpy(&arguments,&options.history.arguments,sizeof(arguments));
    copy_name(reinterpret_cast<char*>(arguments.filaments.prefix),p.name);
    const long double accuracy=accuracy_value;
    double lower,upper;
    if(!(static_cast<long double>(p.target_inductance_nh)<0x1.b7cdfd9d7bdbbp-34L)) {
        lower=x87::binary64((1.L-accuracy)*p.target_inductance_nh);
        upper=x87::binary64((accuracy+1.L)*p.target_inductance_nh);
    } else { lower=0.;upper=1e15; }
    put_integer(arguments,0x50,0);put_integer(arguments,0x64,4);
    put_number(arguments,0x54,static_cast<long double>(p.chip_x)*0.5L-static_cast<long double>(p.dimension_x)*0.5L);
    put_number(arguments,0x5c,static_cast<long double>(p.chip_y)*0.5L-static_cast<long double>(p.dimension_x)*0.5L);
    put_integer(arguments,0x68,p.metal);put_integer(arguments,0x6c,p.underpass_metal);
    put_number(arguments,0x70,p.dimension_x);put_number(arguments,0x78,p.dimension_y);
    put_number(arguments,0xa0,0.L);arguments.attributes=0;
    double best_metric=-1.,best_spacing=0.,best_width=0.,best_turns=0.;
    const double* metric=p.quality_metric?(p.differential?&state.model.pi.quality_differential:&state.model.pi.quality_first):
        (p.differential?&state.model.pi.shunt_differential:&state.model.pi.shunt_single);
    message(options.runtime,"Simulation started at:  ");
    if(options.runtime.system)options.runtime.system("date",options.runtime.user);
    unsigned calls=0,iterations=0;
    for(long double spacing=p.spacing.minimum;spacing<=p.spacing.maximum;spacing+=p.spacing.step)
        for(long double width=p.width.minimum;width<=p.width.maximum;width+=p.width.step) {
            long double limit=maximum_spiral_turns(p.dimension_x,x87::binary64(width),x87::binary64(spacing),4,0);
            for(long double turns=1.L;turns<=limit;turns+=0.25L) {
                ++iterations;
                put_number(arguments,0x98,turns);put_number(arguments,0x88,width);put_number(arguments,0x90,spacing);
                build_square_spiral(state.shapes,arguments,3,options.technology,options.construction,construction_history(options.history,calls));
                analyze(state,options,arguments,p.frequency,calls,false);
                ++calls;remove_geometry_shape(state.shapes,state.shapes,options.construction.geometry.memory);
                const long double measured=static_cast<long double>(state.model.pi.inductance)*1e9L;
                if(measured>upper)limit=turns;
                // The original FCOM masks admit unordered inductance values,
                // while both ordered equality boundaries are excluded.
                if(!(measured<=lower) && !(measured>=upper) && *metric>=best_metric) {
                    best_metric=x87::binary64(*metric);best_turns=x87::binary64(turns);
                    best_width=x87::binary64(width);best_spacing=x87::binary64(spacing);
                }
            }
        }
    if(options.runtime.iterations)options.runtime.iterations(static_cast<int>(iterations),options.runtime.user);
    message(options.runtime,"Simulation ended at:  ");
    if(options.runtime.system)options.runtime.system("date",options.runtime.user);
    if(!(best_metric>0x1.b7cdfd9d7bdbbp-34L)) {
        if(options.runtime.format) { options.runtime.format(options.runtime.line,"Cannot find a spiral with the desired inductance.");message(options.runtime,options.runtime.line); }
        state.selected_turns=-1.;return false;
    }
    put_number(arguments,0x98,best_turns);put_number(arguments,0x88,best_width);put_number(arguments,0x90,best_spacing);
    state.selected_turns=best_turns;state.selected_width=best_width;state.selected_spacing=best_spacing;
    build_square_spiral(state.shapes,arguments,3,options.technology,options.construction,construction_history(options.history,calls));
    analyze(state,options,arguments,p.frequency,calls,true);
    if(options.runtime.format) {
        options.runtime.format(options.runtime.line,"Found:  n =%5.4lf, w =%5.4lf, s =%5.4lf",best_turns,best_width,best_spacing);
        message(options.runtime,options.runtime.line);
        options.runtime.format(options.runtime.line,"L =%5.4lf, R_sh = %5.4lf(s), %5.4lf(d)  Q = %5.4lf, %5.4lf, %5.4lf(d)",
            x87::binary64(static_cast<long double>(state.model.pi.inductance)*1e9L),state.model.pi.shunt_single,state.model.pi.shunt_differential,
            state.model.pi.quality_first,state.model.pi.quality_second,state.model.pi.quality_differential);
        message(options.runtime,options.runtime.line);
    }
    return true;
}
}
