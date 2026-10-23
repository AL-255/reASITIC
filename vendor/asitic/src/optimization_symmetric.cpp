#include "asitic/optimization_symmetric.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(value):"st");return out; }
double copy_number(const double& value) { double out;asm volatile("fldl %1; fstpl %0":"=m"(out):"m"(value):"st");return out; }
double multiply_memory(long double left,const double& right) { long double out;asm volatile("fmull %2":"=t"(out):"0"(left),"m"(right));return x87::binary64(out); }
void copy(char* out,const char* in) { while((*out++=*in++)) {} }
double& number(GeometryShape& arguments,unsigned offset) { return *reinterpret_cast<double*>(arguments.filaments.prefix+offset); }
int& integer(GeometryShape& arguments,unsigned offset) { return *reinterpret_cast<int*>(arguments.filaments.prefix+offset); }
void message(const SquareOptimizationRuntime& runtime,const char* text) { if(runtime.message)runtime.message(text,true,runtime.user); }
void prompt(bool polygon,GeometryShape& arguments,SymmetricOptimizationPromptHistory& h,
    const SymmetricOptimizationOptions& o,bool& unrestricted) {
    const auto& r=*o.prompts;char* line=o.runtime.line;const auto read=[&](const char* text) { o.read(text,line,o.read_user); };
    h.target=0.;int scanned;
    do {
        read("Desired inductance and accuracy in percent (e.g. 2 5 for 2nH within 5%)? (nH) ");
        scanned=r.command.scan(line,"%lf %lf",&h.target,&h.accuracy);
    }while(x87::abs(load(h.target))<load(1e-10) || scanned<2);
    h.accuracy=multiply_memory(load(0.01),h.accuracy);unrestricted=load(h.target)<0.L;
    do {
        read("Name? ");normalize_input_line(line,0xae,r.command.characters);
        if(find_geometry_shape(r.command.shapes,r.command.selected,line)) { message(o.runtime,"Another existing spiral has the same name.");*line=0; }
        else r.command.scan(line,"%s",reinterpret_cast<char*>(arguments.filaments.prefix));
    }while(!*line);
    auto& dimension=number(arguments,polygon?0x80:0x70);
    do { read(polygon?"Radius? ":"Length? ");r.command.scan(line,"%lf",&dimension); }
    while(x87::abs(load(dimension))<load(1e-10));
    if(polygon)do { read("How many sides? ");r.command.scan(line,"%d",&integer(arguments,0x64)); }while(integer(arguments,0x64)<3);
    do { read("Min/Step/Max Spacing? ");scanned=r.command.scan(line,"%lf %lf %lf",&h.spacing.minimum,&h.spacing.step,&h.spacing.maximum); }while(scanned<3);
    do { read("Min/Step/Max Width? ");scanned=r.command.scan(line,"%lf %lf %lf",&h.width.minimum,&h.width.step,&h.width.maximum); }while(scanned<3);
    int metal,transition;
    do { read("Metal layer? ");metal=find_shape_prompt_metal(line,r,h.layer); }while(metal<0 || metal>=r.technology.metal_count);
    do { read(polygon?"Transition metal layer? ":"Exit metal layer? ");transition=find_shape_prompt_metal(line,r,h.layer); }
    while(transition==metal || transition<0 || transition>=r.technology.metal_count);
    do { read("Frequency? (GHz) ");r.command.scan(line,"%lf",&h.frequency); }while(load(h.frequency)<=load(1e-10));
    h.frequency=multiply_memory(load(h.frequency),1e9);
    number(arguments,0x78)=copy_number(number(arguments,0x70));integer(arguments,0x68)=metal;integer(arguments,0x6c)=transition;
}
void analyze(SquareOptimizationState& state,const SymmetricOptimizationOptions& o,const GeometryShape& arguments,
    double frequency,unsigned call,bool report) {
    auto model=o.analytical;__builtin_memcpy(&model.kind,arguments.filaments.prefix+0x50,4);__builtin_memcpy(&model.sides,arguments.filaments.prefix+0x64,4);
    __builtin_memcpy(&model.spacing,arguments.filaments.prefix+0x90,8);__builtin_memcpy(&model.turns,arguments.filaments.prefix+0x98,8);
    auto history=o.history.analytical_at?*o.history.analytical_at(call,o.history.user):o.history.analytical[call];
    solve_analytical_model(state.shapes->filaments,frequency,model,history,state.model,o.warnings);
    if(report && o.runtime.model_report)o.runtime.model_report(state.model.pi,o.runtime.user);
}
bool optimize(bool polygon,SquareOptimizationState& state,const SymmetricOptimizationOptions& o,int skip_prompt) {
    *o.runtime.line=0;
    GeometryShape arguments;__builtin_memcpy(&arguments,&o.history.arguments,sizeof(arguments));
    auto h=o.history.prompt;bool unrestricted=false;
    if(!skip_prompt)prompt(polygon,arguments,h,o,unrestricted);
    else {
        const auto& p=state.parameters;
        h.target=copy_number(p.target_inductance_nh);h.accuracy=multiply_memory(load(0.01),p.accuracy_percent);
        copy(reinterpret_cast<char*>(arguments.filaments.prefix),p.name);
        number(arguments,polygon?0x80:0x70)=copy_number(p.dimension_x);number(arguments,0x78)=copy_number(p.dimension_y);
        h.spacing={copy_number(p.spacing.minimum),copy_number(p.spacing.step),copy_number(p.spacing.maximum)};
        h.width={copy_number(p.width.minimum),copy_number(p.width.step),copy_number(p.width.maximum)};
        integer(arguments,0x68)=p.metal;integer(arguments,0x6c)=p.underpass_metal;h.frequency=copy_number(p.frequency);
        unrestricted=load(h.target)<load(1e-10);
    }
    integer(arguments,0x50)=polygon?17:16;if(!polygon)integer(arguments,0x64)=4;
    number(arguments,0x54)=0.;number(arguments,0x5c)=0.;number(arguments,0xa0)=0.;arguments.attributes=0;
    if(load(h.spacing.step)<load(1e-10))h.spacing.step=1.;
    if(load(h.width.step)<load(1e-10))h.width.step=1.;
    double best=-1.,best_width=0.,best_spacing=0.,best_turns=0.;unsigned calls=0;
    message(o.runtime,"Simulation started at:  ");if(o.runtime.system)o.runtime.system("date",o.runtime.user);
    const auto construct=[&]() {
        const auto& history=o.history.construction_at?*o.history.construction_at(calls,o.history.user):o.history.construction[calls];
        if(polygon)build_symmetric_polygon(state.shapes,arguments,o.technology,o.construction,history);
        else build_symmetric_square(state.shapes,arguments,o.technology,o.construction,history);
    };
    for(long double spacing=load(h.spacing.minimum);spacing<=load(h.spacing.maximum);spacing+=load(h.spacing.step))
        for(long double width=load(h.width.minimum);width<=load(h.width.maximum);width+=load(h.width.step)) {
            const long double sum=width+spacing;long double limit;
            if(polygon)limit=load(number(arguments,0x80))/sum-1.L;
            else {
                const long double raw=load(number(arguments,0x70))/(sum+sum+1.L);
                const long double whole=x87::truncate_int(raw);
                limit=(static_cast<long double>(x87::truncate_int(raw*4.L))-whole*4.L)*0.25L+whole;
                if(limit<1.L)continue;
                limit-=0.25L;
            }
            if(limit<1.L)continue;
            number(arguments,0x78)=x87::binary64(sum);
            for(long double turns=1.L;turns<=limit;turns+=1.L) {
                number(arguments,0x88)=x87::binary64(width);number(arguments,0x90)=x87::binary64(spacing);number(arguments,0x98)=x87::binary64(turns);
                construct();analyze(state,o,arguments,h.frequency,calls,false);++calls;
                const long double measured=load(state.model.pi.inductance)/load(1e-9);
                if(!unrestricted && measured>load(h.target))limit=turns;
                const long double quality=load(state.model.pi.quality_first);
                if((unrestricted || !(load(h.target)*load(h.accuracy)<=x87::abs(measured-load(h.target)))) && quality>=load(best)) {
                    best=x87::binary64(quality);best_width=x87::binary64(width);best_spacing=x87::binary64(spacing);best_turns=x87::binary64(turns);
                }
                remove_geometry_shape(state.shapes,state.shapes,o.construction.geometry.memory);
            }
        }
    message(o.runtime,"Simulation ended at:  ");if(o.runtime.system)o.runtime.system("date",o.runtime.user);
    if(!(load(best)>load(1e-10))) {
        if(o.runtime.format) { o.runtime.format(o.runtime.line,"Cannot find a spiral with the desired inductance.");message(o.runtime,o.runtime.line); }
        state.selected_turns=-1.;return false;
    }
    number(arguments,0x88)=copy_number(best_width);number(arguments,0x90)=copy_number(best_spacing);number(arguments,0x98)=copy_number(best_turns);
    state.selected_width=copy_number(best_width);state.selected_spacing=copy_number(best_spacing);state.selected_turns=copy_number(best_turns);
    construct();analyze(state,o,arguments,h.frequency,calls,true);
    if(o.runtime.format) {
        o.runtime.format(o.runtime.line,"Found:  n =%5.4lf, w =%5.4lf, s =%5.4lf, L =%5.4lf (nH), Q = %5.4lf",
            best_turns,best_width,best_spacing,x87::binary64(load(state.model.pi.inductance)/load(1e-9)),state.model.pi.quality_first);
        message(o.runtime,o.runtime.line);
    }
    return true;
}
}
bool optimize_symmetric_square(SquareOptimizationState& state,const SymmetricOptimizationOptions& options,int skip_prompt) { return optimize(false,state,options,skip_prompt); }
bool optimize_symmetric_polygon(SquareOptimizationState& state,const SymmetricOptimizationOptions& options,int skip_prompt) { return optimize(true,state,options,skip_prompt); }
}
