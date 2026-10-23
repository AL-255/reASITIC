#include "asitic/optimization_prompt.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(value):"st");return out; }
double multiply(const double& left,const double& right) {
    double out;asm volatile("fldl %1; fmull %2; fstpl %0":"=m"(out):"m"(left),"m"(right):"st");return out;
}
void copy(char* out,const char* source) { while((*out++=*source++)) {} }
}
void prompt_optimization_target(OptimizationSweepState& state,const OptimizationPromptRuntime& runtime,
    OptimizationPromptHistory& history) {
    const auto& shape=runtime.shape;char* line=shape.command.message;
    const auto read=[&](const char* prompt) { runtime.read(prompt,line,runtime.user); };
    const auto warning=[&](const char* message) { shape.command.warnings.emit(message); };
    const long double threshold=load(1e-10);
    int scanned;
    do {
        read("Desired inductance and accuracy in percent (e.g. '2 5' for 2nH within 5%)? (nH) ");
        scanned=shape.command.scan(line,"%lf %lf",&history.target,&history.accuracy);
    }while(x87::abs(load(history.target))<threshold || scanned<2);
    do {
        read("Name? ");normalize_input_line(line,0xae,shape.command.characters);
        if(find_geometry_shape(shape.command.shapes,shape.command.selected,line)) {
            warning("Another existing spiral has the same name.");line[0]=0;
        } else shape.command.scan(line,"%s",history.name);
    }while(!*line);
    do {
        read("Length? ");scanned=shape.command.scan(line,"%lf %lf",&history.length,&history.length_multiplier);
        if(scanned==1)history.length_multiplier=1.;
    }while(x87::abs(load(history.length))<threshold);
    do {
        read("Min/Step/Max Spacing? ");
        scanned=shape.command.scan(line,"%lf %lf %lf",&history.spacing_min,&history.spacing_increment,&history.spacing_max);
        if(load(history.spacing_increment)<threshold)warning("Need positive step-size.");
        if(load(history.spacing_min)>load(history.spacing_max))warning("Can't specify smax < smin.");
    }while(scanned<3 || load(history.spacing_min)>load(history.spacing_max) || load(history.spacing_increment)<threshold);
    do {
        read("Min/Step/Max Width? ");
        scanned=shape.command.scan(line,"%lf %lf %lf",&history.width_min,&history.width_increment,&history.width_max);
        if(load(history.width_increment)<threshold)warning("Need positive step-size.");
        if(load(history.width_min)>load(history.width_max))warning("Can't specify wmax < wmin.");
    }while(scanned<3 || load(history.width_min)>load(history.width_max) || load(history.width_increment)<threshold);
    int metal,exit;
    do { read("Metal layer? ");metal=find_shape_prompt_metal(line,shape,history.layer); }
    while(metal<0 || metal>=shape.technology.metal_count);
    do { read("Exit metal layer? ");exit=find_shape_prompt_metal(line,shape,history.layer); }
    while(exit==metal || exit<0 || exit>=shape.technology.metal_count);
    do { read("Frequency? (GHz) ");shape.command.scan(line,"%lf",&history.frequency); }
    while(load(history.frequency)<=threshold);
    history.frequency=x87::binary64(load(history.frequency)*load(1e9));
    copy(state.shape_name,history.name);
    state.inductance_min=x87::binary64(load(history.target));
    state.inductance_max=x87::binary64(load(history.accuracy));
    state.minimum_quality=x87::binary64(load(history.length));
    state.absolute_tolerance=multiply(history.length,history.length_multiplier);
    state.spacing_min=x87::binary64(load(history.spacing_min));
    state.spacing_increment=x87::binary64(load(history.spacing_increment));
    state.spacing_max=x87::binary64(load(history.spacing_max));
    state.width_min=x87::binary64(load(history.width_min));
    state.width_increment=x87::binary64(load(history.width_increment));
    state.width_max=x87::binary64(load(history.width_max));
    state.metal=metal;state.exit_metal=exit;state.metric_kind=1;state.metric_terminal=0;
    state.frequency=x87::binary64(load(history.frequency));
}
}
