#include "asitic/optimization_area.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(value):"st");return out; }
double& field(GeometryShape& shape,unsigned offset) { return *reinterpret_cast<double*>(shape.filaments.prefix+offset); }
void integer(GeometryShape& shape,unsigned offset,int value) { __builtin_memcpy(shape.filaments.prefix+offset,&value,4); }
void number(GeometryShape& shape,unsigned offset,long double value) { field(shape,offset)=x87::binary64(value); }
const SquareConstructionHistory& construction_history(const AreaOptimizationHistory& history,unsigned call) {
    return *(history.construction_at?history.construction_at(call,history.user):history.construction+call);
}
const AnalyticalModelHistory& analytical_history(const AreaOptimizationHistory& history,unsigned call) {
    return *(history.analytical_at?history.analytical_at(call,history.user):history.analytical+call);
}
void analyze(AreaOptimizationState& state,const AreaOptimizationOptions& options,const GeometryShape& args,
             double frequency,const AnalyticalModelHistory& incoming,bool report) {
    auto model=options.analytical;model.kind=0;model.sides=4;
    __builtin_memcpy(&model.spacing,args.filaments.prefix+0x90,8);__builtin_memcpy(&model.turns,args.filaments.prefix+0x98,8);
    auto history=incoming;solve_analytical_model(state.shapes->filaments,frequency,model,history,state.model,options.warnings);
    if(report && options.runtime.model_report)options.runtime.model_report(state.model.pi,options.runtime.user);
}
}
bool optimize_area(AreaOptimizationState& state,const AreaOptimizationOptions& options,AreaOptimizationHistory& history) {
    const auto& runtime=options.runtime;const auto& prompt=runtime.prompts;auto& scan=history.scans;auto& args=history.arguments;
    char* line=prompt.command.message;
    const auto read=[&](const char* message) { runtime.read(message,line,runtime.user); };
    const auto message=[&](const char* text) { runtime.message(text,true,runtime.user); };
    const long double threshold=load(1e-10);scan.target=0.;line[0]=0;int scanned;
    do {
        read("Desired inductance and accuracy in percent (e.g. 5 2 for 5nH within 2%)? ");
        scanned=prompt.command.scan(line,"%lf %lf",&scan.target,&scan.accuracy);
    }while(load(scan.target)<threshold || scanned<2);
    scan.accuracy=x87::binary64(load(0.01)*load(scan.accuracy));
    do {
        read("Name? ");normalize_input_line(line,0xae,prompt.command.characters);
        if(find_geometry_shape(prompt.command.shapes,prompt.command.selected,line)) {
            message("Another existing spiral has the same name.");line[0]=0;
        }else prompt.command.scan(line,"%s",args.filaments.prefix);
    }while(!*line);
    do { read("Spacing? ");scanned=prompt.command.scan(line,"%lf",&scan.spacing); }while(scanned<1);
    do {
        read("Min/Step/Max Width? ");scanned=prompt.command.scan(line,"%lf %lf %lf",&scan.width_min,&scan.width_step,&scan.width_max);
    }while(scanned<3);
    do {
        read("Min/Step/Max Length? ");scanned=prompt.command.scan(line,"%lf %lf %lf",&scan.length_min,&scan.length_step,&scan.length_max);
        if(load(scan.width_max)>load(scan.length_min))message("Min Length Can't be less than Max Width!");
    }while(scanned<3);
    int metal,exit;
    do { read("Metal layer? ");metal=find_shape_prompt_metal(line,prompt,history.layer); }
    while(metal<0 || metal>=prompt.technology.metal_count);
    do { read("Exit metal layer? ");exit=find_shape_prompt_metal(line,prompt,history.layer); }
    while(exit==metal || exit<0 || exit>=prompt.technology.metal_count);
    do { read("Frequency? (GHz) ");prompt.command.scan(line,"%lf",&scan.frequency); }while(load(scan.frequency)<=threshold);
    do { read("Minimum Q? ");prompt.command.scan(line,"%lf",&scan.quality); }while(load(scan.quality)<=threshold);
    scan.frequency=x87::binary64(load(1e9)*load(scan.frequency));
    integer(args,0x50,0);integer(args,0x64,4);integer(args,0x68,metal);integer(args,0x6c,-1);
    const long double origin=-load(scan.length_max)*load(0.5);number(args,0x54,origin);number(args,0x5c,origin);
    number(args,0x90,load(scan.spacing));number(args,0xa0,0.L);args.attributes=0;
    if(load(scan.length_step)<threshold)scan.length_step=1.;
    if(load(scan.width_step)<threshold)scan.width_step=1.;
    double best_quality=-1.,best_length=0.,best_width=0.,best_turns=0.,previous_inductance=0.;unsigned call=0;
    long double next_length=load(scan.length_min);number(args,0x70,next_length);
    while(next_length<=load(scan.length_max)) {
        __builtin_memcpy(&field(args,0x78),&field(args,0x70),8);
        for(long double width=load(scan.width_min);width<=load(scan.width_max);width+=load(scan.width_step)) {
            const long double sum=width+load(scan.spacing);
            const long double ratio=load(field(args,0x70))/(sum+sum+load(1.));
            const long double whole=x87::truncate_int(ratio);
            const long double quarter=(static_cast<long double>(x87::truncate_int(ratio*load(4.)))-whole*load(4.))*load(0.25)+whole;
            if(!(quarter<load(1.))) {
                long double limit=quarter-load(0.25);
                for(long double turns=1.L;turns<=limit;turns+=load(0.25)) {
                    number(args,0x98,turns);number(args,0x88,width);
                    build_square_spiral(state.shapes,args,3,options.technology,options.construction,construction_history(history,call));
                    if(load(previous_inductance)>load(scan.target))limit=turns;
                    analyze(state,options,args,scan.frequency,analytical_history(history,call++),false);
                    const long double measured=load(state.model.pi.inductance)/load(1e-9);
                    previous_inductance=x87::binary64(measured);
                    const long double quality=load(state.model.pi.quality_first);
                    if(!(x87::abs(measured-load(scan.target))>=load(scan.target)*load(scan.accuracy))) {
                        if(quality>=load(scan.quality)) {
                            runtime.format(line,"Found:  Len = %5.4lf, n =%5.4lf, w =%5.4lf, s =%5.4lf, L =%5.4lf (nH), Q = %5.4lf",
                                field(args,0x70),x87::binary64(turns),x87::binary64(width),scan.spacing,previous_inductance,x87::binary64(quality));message(line);
                        }
                        if(quality>=load(best_quality)) {
                            best_quality=x87::binary64(quality);best_length=field(args,0x70);best_width=x87::binary64(width);best_turns=x87::binary64(turns);
                        }
                    }
                    remove_geometry_shape(state.shapes,state.shapes,options.construction.geometry.memory);
                }
            }
        }
        next_length=load(field(args,0x70))+load(scan.length_step);number(args,0x70,next_length);
    }
    if(!(load(best_quality)>threshold)) {
        runtime.format(line,"Cannot find a spiral with the desired inductance.");state.selected_turns=-1.;message(line);return false;
    }
    number(args,0x70,load(best_length));number(args,0x78,load(best_length));number(args,0x88,load(best_width));number(args,0x98,load(best_turns));
    state.selected_spacing=x87::binary64(load(scan.spacing));state.selected_width=x87::binary64(load(best_width));state.selected_turns=x87::binary64(load(best_turns));
    build_square_spiral(state.shapes,args,3,options.technology,options.construction,construction_history(history,call));
    analyze(state,options,args,scan.frequency,analytical_history(history,call),true);
    runtime.format(line,"Found:  n =%5.4lf, w =%5.4lf, s =%5.4lf, L =%5.4lf (nH), Q = %5.4lf",best_turns,best_width,scan.spacing,
        x87::binary64(load(state.model.pi.inductance)/load(1e-9)),state.model.pi.quality_first);message(line);return true;
}
}
