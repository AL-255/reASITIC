#include "asitic/optimization.hpp"
namespace asitic {
namespace {
void copy(char* destination,const char* source) { while((*destination++=*source++)) {} }
double number(const CommandArgument& argument) {
    float value;const auto* source=reinterpret_cast<const unsigned char*>(&argument.value);
    auto* bytes=reinterpret_cast<unsigned char*>(&value);for(unsigned i=0;i<4;++i)bytes[i]=source[i];
    return static_cast<double>(value);
}
int integer(const CommandArgument& argument) { return static_cast<int>(argument.value); }
}
void parse_optimization_sweep(char* input,OptimizationSweepState& state,
    const ShapePromptRuntime& runtime,OptimizationSweepHistory& history) {
    bool failed=false;auto& out=history.temporary;
    unsigned invocation=0;
    const auto argument=[&](const char* key,unsigned type) {
        return extract_shape_argument(input,out,key,type,runtime.command,history.scan[invocation++]);
    };
    const auto error=[&](const char* message) { runtime.command.warnings.emit(message);failed=true; };
    const double threshold=1e-10;
    normalize_input_line(input,0x8e,runtime.command.characters);
    if(argument("FNAME",3))copy(state.output_filename,out.text);
    else error("Error:  Missing Argument, must supply optimization output filename.");
    if(argument("NAME",3)) {
        copy(state.shape_name,out.text);
        if(find_geometry_shape(runtime.command.shapes,runtime.command.selected,state.shape_name))
            error("Error:  The supplied spiral name is already in use.");
    } else error("Error:  Missing Argument, must supply spiral name.");
    normalize_input_line(input,0x20,runtime.command.characters);
    if(argument("FREQ",5)) {
        state.frequency=number(out);
        if(state.frequency<threshold)error("Error:  Invalid Argument, must supply valid analysis frequency.");
    } else error("Error:  Missing Argument, must supply optimization analysis frequency.");
    if(argument("LMIN",4))state.inductance_min=number(out);
    else error("Error:  Missing Argument, must supply minimum inductance.");
    if(argument("LMAX",4)) {
        state.inductance_max=number(out);
        if(state.inductance_max<threshold || state.inductance_min<threshold || state.inductance_max<=state.inductance_min)
            error("Error:  Must specify a valid inductance range.");
    } else error("Error:  Missing Argument, must supply maximum inductance.");
    if(argument("MINQ",4)) {
        state.minimum_quality=number(out);
        if(state.minimum_quality<threshold)error("Error:  Must specify a valid Q > 0.");
    } else error("Error:  Missing Argument, must supply minimum Q.");
    if(argument("AMIN",4))state.area_min=number(out);
    else error("Error:  Missing Argument, must supply minimum area for sweep.");
    if(argument("AINC",7))state.area_increment=number(out);
    else error("Error:  Missing Argument, must supply area increment for sweep.");
    if(argument("AMAX",7))state.area_max=number(out);
    else error("Error:  Missing Argument, must supply maximum area for sweep.");
    if(!failed && (state.area_min<threshold || state.area_increment<threshold || state.area_max<threshold || state.area_max<state.area_min))
        error("Error:  Must specify a valid area sweep range.");
    if(argument("SMIN",7))state.spacing_min=number(out);
    else error("Error:  Missing Argument, must supply minimum spacing for sweep.");
    if(argument("SINC",7))state.spacing_increment=number(out);
    else error("Error:  Missing Argument, must supply spacing increment for sweep.");
    if(argument("SMAX",7))state.spacing_max=number(out);
    else error("Error:  Missing Argument, must supply maximum spacing for sweep.");
    if(!failed && (state.spacing_min<threshold || state.spacing_increment<threshold || state.spacing_max<threshold || state.spacing_max<state.spacing_min))
        error("Error:  Must specify a valid spacing sweep range.");
    if(argument("WMIN",7))state.width_min=number(out);
    else error("Error:  Missing Argument, must supply minimum width for sweep.");
    if(argument("WINC",7))state.width_increment=number(out);
    else error("Error:  Missing Argument, must supply width increment for sweep.");
    if(argument("WMAX",7))state.width_max=number(out);
    else error("Error:  Missing Argument, must supply maximum width for sweep.");
    if(!failed && (state.width_min<threshold || state.width_increment<threshold || state.width_max<threshold || state.width_max<state.width_min))
        error("Error:  Must specify a valid width sweep range.");
    if(argument("METAL",1)) {
        state.metal=find_shape_prompt_metal(out.text,runtime,history.layer[0]);
        if(state.metal<0 || state.metal>=runtime.technology.metal_count)error("Error:  Illegal metal layer.");
    } else state.metal=runtime.technology.metal_count-1;
    if(argument("EXIT",1))state.exit_metal=find_shape_prompt_metal(out.text,runtime,history.layer[1]);
    else state.exit_metal=-1;
    if(!failed && (state.exit_metal < -1 || state.exit_metal>=runtime.technology.metal_count))
        error("Error:  Illegal exit metal layer.");
    state.sides=argument("SIDES",6)?integer(out):4;
    if(state.sides<3)error("Error:  Spiral must have at least 3 sides.");
    state.simulation_type=argument("SIMTYPE",6)?integer(out):1;
    if(static_cast<unsigned>(state.simulation_type)-1u>2u)error("Error:  Unknown analysis type requested.");
    const bool ground=argument("TAP",0);
    if(ground)state.ground=reinterpret_cast<GeometryShape*>(out.value);
    if(state.simulation_type==3 && !ground)
        error("Error: Requested Pi3 analysis but did not specify valid spiral ground in TAP parameter.");
    if(failed)runtime.invalid(runtime.user);
}
}
