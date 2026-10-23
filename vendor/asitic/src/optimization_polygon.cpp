#include "asitic/optimization_polygon.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1;fstpt %0":"=m"(out):"m"(value):"st");return out; }
double quiet(const double& value) { double out;asm volatile("fldl %1;fstpl %0":"=m"(out):"m"(value):"st");return out; }
void integer(GeometryShape& shape,unsigned offset,int value) { __builtin_memcpy(shape.filaments.prefix+offset,&value,4); }
void number(GeometryShape& shape,unsigned offset,long double value) { *reinterpret_cast<double*>(shape.filaments.prefix+offset)=x87::binary64(value); }
void message(const PolygonOptimizationRuntime& runtime,const char* text) { if(runtime.message)runtime.message(text,true,runtime.user); }
WireConstructionHistory construction_history(const PolygonOptimizationHistory& h,unsigned index) {
    return h.construction_at?*h.construction_at(index,h.user):h.construction[index];
}
void prompt(PolygonOptimizationParameters& p,GeometryShape& arguments,const PolygonOptimizationOptions& options,bool& maximize) {
    const auto& r=options.runtime;char* line=r.line;line[0]=0;
    const auto read=[&](const char* text) { r.read(text,line,r.user); };
    int scanned;
    do { read("Desired Inductance, Accuracy (+/-)? (nH) ");scanned=r.prompt.command.scan(line,"%lf %lf",&p.target_inductance_nh,&p.accuracy_fraction); }
    while(x87::abs(load(p.target_inductance_nh))<load(1e-10) || scanned<2);
    p.accuracy_fraction=x87::binary64(load(0.01)*load(p.accuracy_fraction));maximize=load(p.target_inductance_nh)<0.L;
    do {
        read("Name? ");normalize_input_line(line,0xae,r.prompt.command.characters);
        if(find_geometry_shape(r.prompt.command.shapes,r.prompt.command.selected,line)) { message(r,"Another existing spiral has the same name.");line[0]=0; }
        else r.prompt.command.scan(line,"%s",arguments.filaments.prefix);
    }while(!*line);
    do { read("Length (2*Radius)? ");r.prompt.command.scan(line,"%lf",&p.diameter); }
    while(x87::abs(load(p.diameter))<load(1e-10));
    do { read("How many sides? ");r.prompt.command.scan(line,"%d",&p.sides); }while(p.sides<3);
    do { read("Spacing? ");scanned=r.prompt.command.scan(line,"%lf",&p.spacing); }while(scanned<1);
    do { read("Min/Step/Max Width? ");scanned=r.prompt.command.scan(line,"%lf %lf %lf",&p.width.minimum,&p.width.step,&p.width.maximum); }while(scanned<3);
    int layer=options.history.layer;
    do { read("Metal layer? ");p.metal=find_shape_prompt_metal(line,r.prompt,layer); }while(p.metal<0 || p.metal>=options.technology.metal_count);
    do { read("Frequency? (GHz) ");r.prompt.command.scan(line,"%lf",&p.frequency); }while(load(p.frequency)<=load(1e-10));
    p.frequency=x87::binary64(load(p.frequency)*load(1e9));
}
void analyze(PolygonOptimizationState& state,const PolygonOptimizationOptions& options,const PolygonOptimizationParameters& p,unsigned call,bool report) {
    auto model=options.analytical;model.kind=1;model.sides=p.sides;model.spacing=p.spacing;
    __builtin_memcpy(&model.turns,state.shapes->filaments.prefix+0x98,8);
    auto history=options.history.analytical_at?*options.history.analytical_at(call,options.history.user):options.history.analytical[call];
    solve_analytical_model(state.shapes->filaments,p.frequency,model,history,state.model,options.warnings);
    if(report && options.runtime.model_report)options.runtime.model_report(state.model.pi,options.runtime.user);
}
}
int optimize_polygon_spiral(PolygonOptimizationState& state,const PolygonOptimizationOptions& options,int skip_prompt) {
    auto p=skip_prompt?state.parameters:options.history.prompt;
    GeometryShape arguments;__builtin_memcpy(&arguments,&options.history.arguments,sizeof(arguments));bool maximize=false;
    if(!skip_prompt) { p.target_inductance_nh=0.;prompt(p,arguments,options,maximize);p.chip_x=state.parameters.chip_x;p.chip_y=state.parameters.chip_y; }
    else {
        p.target_inductance_nh=quiet(p.target_inductance_nh);p.accuracy_fraction=quiet(p.accuracy_fraction);
        const char name[]="TestOpt";__builtin_memcpy(arguments.filaments.prefix,name,8);
        p.diameter=quiet(p.diameter);p.spacing=quiet(p.spacing);p.width.minimum=quiet(p.width.minimum);p.width.step=quiet(p.width.step);p.width.maximum=quiet(p.width.maximum);p.frequency=quiet(p.frequency);
    }
    integer(arguments,0x50,1);integer(arguments,0x64,p.sides);integer(arguments,0x68,p.metal);integer(arguments,0x6c,-1);
    number(arguments,0x70,load(p.diameter));number(arguments,0x78,load(p.diameter));number(arguments,0x80,load(p.diameter)*0.5L);
    number(arguments,0x90,load(p.spacing));number(arguments,0xa0,0.L);arguments.attributes=0;
    number(arguments,0x54,load(p.chip_x)*0.5L);number(arguments,0x5c,load(p.chip_y)*0.5L);
    if(load(1e-10)>load(p.width.step))p.width.step=1.;
    const auto& runtime=options.runtime;message(runtime,"Simulation started at:  ");if(runtime.system)runtime.system("date",runtime.user);
    double best_quality=-1.,best_width=0.,best_turns=0.;bool stop=false;unsigned calls=0;
    long double width_comparison=load(p.width.minimum);double width=x87::binary64(width_comparison);
    while(width_comparison<=load(p.width.maximum) && !stop) {
        const long double cosine=x87::cos(load(0x1.921fb54442d18p+1)/static_cast<long double>(p.sides));
        const double numerator=x87::binary64((load(*reinterpret_cast<const double*>(arguments.filaments.prefix+0x80))-load(p.spacing))*cosine);
        const double quotient=x87::binary64(load(numerator)/(load(width)+load(p.spacing)));
        const long double initial_extended=load(quotient)-2.L;const double initial_limit=x87::binary64(initial_extended);
        double limit=initial_limit;
        if(!(initial_extended<1.L))for(long double turns=1.L;turns<=load(limit);turns+=1.L/static_cast<long double>(p.sides)) {
            number(arguments,0x98,turns);number(arguments,0x88,load(width));
            build_polygon_spiral(state.shapes,arguments,options.technology,options.construction,construction_history(options.history,calls));
            analyze(state,options,p,calls,false);++calls;
            const long double measured=load(state.model.pi.inductance)/load(0x1.12e0be826d695p-30),quality=load(state.model.pi.quality_first);
            if(!maximize && measured>load(p.target_inductance_nh))limit=x87::binary64(turns);
            if(x87::abs(turns-load(initial_limit))<=1.L/static_cast<long double>(p.sides) && measured<load(p.target_inductance_nh))stop=true;
            bool acceptable=maximize;
            if(!maximize) { const long double error=x87::abs(measured-load(p.target_inductance_nh)),tolerance=load(p.target_inductance_nh)*load(p.accuracy_fraction);acceptable=!(tolerance<=error); }
            if(acceptable && quality>=load(best_quality)) {
                best_quality=x87::binary64(quality);best_turns=x87::binary64(turns);best_width=quiet(width);
            }
            remove_geometry_shape(state.shapes,state.shapes,options.construction.geometry.memory);
        }
        width_comparison=load(width)+load(p.width.step);width=x87::binary64(width_comparison);
    }
    message(runtime,"Simulation ended at:  ");if(runtime.system)runtime.system("date",runtime.user);
    if(load(best_quality)>load(1e-10)) {
        number(arguments,0x98,load(best_turns));number(arguments,0x88,load(best_width));
        state.selected_width=quiet(best_width);state.selected_turns=quiet(best_turns);
        build_polygon_spiral(state.shapes,arguments,options.technology,options.construction,construction_history(options.history,calls));
        analyze(state,options,p,calls,true);
        runtime.format(runtime.line,"Found:  n =%5.4lf, w =%5.4lf, s =%5.4lf, L =%5.4lf (nH), Q = %5.4lf",best_turns,best_width,p.spacing,x87::binary64(load(state.model.pi.inductance)/load(0x1.12e0be826d695p-30)),best_quality);
    }else runtime.format(runtime.line,"Cannot find a spiral with the desired inductance.");
    if(!skip_prompt)message(runtime,runtime.line);
    return 0;
}
}
