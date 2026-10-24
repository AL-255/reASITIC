#include "engine.hpp"
#include "asitic/analysis_reports.hpp"
extern "C" int sprintf(char*,const char*,...);

// This is a frequency-dependent policy, not an input dimension. The five
// computed binary64 limits must reach the solvers without grid/f32 reparsing.
extern "C" const char* automatic_cell_limits(double frequency,double alpha,double beta){
    static char output[768];
    const auto engine=reasitic::engine_view();
    if(!__builtin_isfinite(frequency)||frequency<=0||!__builtin_isfinite(alpha)||alpha<=0||!__builtin_isfinite(beta)||beta<=0)
        return "{\"error\":\"Automatic cells require positive finite frequency, alpha and beta\"}";
    if(engine.metal_count<1)return "{\"error\":\"Load a technology before calculating automatic cells\"}";
    static_assert(sizeof(asitic::MetalLayer)==sizeof(asitic::TechnologyMetal));
    asitic::FrequencyGeometryLimits limits{};
    asitic::update_frequency_geometry(frequency,reinterpret_cast<const asitic::TechnologyMetal*>(engine.metals),engine.metal_count,alpha,beta,limits,false,{});
    if(!__builtin_isfinite(limits.max_length)||limits.max_length<=0||!__builtin_isfinite(limits.max_width)||limits.max_width<=0)
        return "{\"error\":\"Automatic cell sizes are outside the supported finite positive range\"}";
    sprintf(output,"{\"frequency_hz\":%.17g,\"alpha\":%.17g,\"beta\":%.17g,\"metal\":%d,\"mesh\":{\"length\":%.17g,\"width\":%.17g,\"thickness\":%.17g,\"subdivision_trigger\":%.17g},\"contact_mesh\":{\"length\":%.17g,\"width\":%.17g}}",frequency,alpha,beta,engine.metal_count-1,limits.max_length,limits.max_width,limits.max_thickness,limits.capacitance_max_length,limits.capacitance_max_length,limits.capacitance_max_width);
    return output;
}
