#include "asitic/optimization_batch.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1;fstpt %0":"=m"(out):"m"(value):"st");return out; }
double divide(const double& numerator,const double& denominator) {
    double out;asm volatile("fldl %2;fdivrl %1;fstpl %0":"=m"(out):"m"(numerator),"m"(denominator):"st");return out;
}
}
void run_optimization_batch(OptimizationBatchState& state,const OptimizationBatchOptions& options,OptimizationBatchHistory& history) {
    const auto& io=options.runtime;auto& p=state.search.parameters;
    io.read("Filename for output file? ",history.filename,io.user);
    void* file=io.open(history.filename,"w",io.user);
    p.metal=2;state.sides=5;p.spacing={3.,1.,3.};p.target_inductance_nh=1.;p.accuracy_percent=5.;p.frequency=2500000000.;
    io.print(file,"#\nLen\tQ\tW\tN\tL\tR\tC1\tC2\tR1\tR2");
    unsigned query=0;
    for(double target=5.;target<=20.;target=x87::binary64(load(target)+5.L)) {
        io.print(file,"\nData for Ldes = %lf @ f = %lf",target,divide(p.frequency,1e9));
        p.target_inductance_nh=x87::binary64(load(target));
        for(double length=100.;length<=500.;length=x87::binary64(load(length)+25.L),++query) {
            p.width.minimum=1.;
            const long double maximum=load(length)*0.5L;
            p.width.maximum=x87::binary64(maximum);
            p.width.step=x87::binary64((maximum-1.L)/80.L);
            p.dimension_x=x87::binary64(load(length));
            const auto& search_options=options.search[query];
            optimize_square_spiral(state.search,search_options,1);
            if(load(state.search.selected_turns)>0.L) {
                auto model_options=search_options.analytical;
                const auto* shape=state.search.shapes;
                __builtin_memcpy(&model_options.kind,shape->filaments.prefix+0x50,4);
                __builtin_memcpy(&model_options.sides,shape->filaments.prefix+0x64,4);
                __builtin_memcpy(&model_options.spacing,shape->filaments.prefix+0x90,8);
                __builtin_memcpy(&model_options.turns,shape->filaments.prefix+0x98,8);
                auto analytical=options.retained_analytical[query];
                solve_analytical_model(shape->filaments,p.frequency,model_options,analytical,state.search.model,search_options.warnings);
                if(search_options.runtime.model_report)search_options.runtime.model_report(state.search.model.pi,search_options.runtime.user);
                const auto& pi=state.search.model.pi;
                io.print(file,"\n%5.2lf\t%5.2lf\t%5.2lf\t%5.2lf\t%5.2lf\t%5.2lf\t%5.2lf\t%5.2lf\t%5.2lf\t%5.2lf",
                    length,pi.quality_first,state.search.selected_width,state.search.selected_turns,divide(pi.inductance,1e-9),pi.resistance,
                    divide(pi.capacitance_second,1e-15),divide(pi.capacitance_first,1e-15),pi.resistance_second,pi.resistance_first);
                remove_geometry_shape(state.search.shapes,state.search.shapes,search_options.construction.geometry.memory);
            } else io.print(file,"\n#%7.5lf, could not find",length);
            io.flush(file,io.user);
        }
    }
    io.close(file,io.user);
}
}
