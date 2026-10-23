#pragma once
#include "asitic/optimization_search.hpp"
#include "asitic/shape_prompts.hpp"
namespace asitic {
struct AreaOptimizationState {
    GeometryShape* shapes;
    double selected_spacing,selected_width,selected_turns;
    AnalyticalModelResult model;
};
struct AreaOptimizationScans {
    double quality,frequency,length_min,length_step,length_max;
    double width_min,width_step,width_max,spacing,target,accuracy;
};
static_assert(sizeof(AreaOptimizationScans)==88);
struct AreaOptimizationHistory {
    GeometryShape arguments;
    AreaOptimizationScans scans;
    int layer;
    const SquareConstructionHistory* construction;
    const AnalyticalModelHistory* analytical;
    const SquareConstructionHistory* (*construction_at)(unsigned,void*)=nullptr;
    const AnalyticalModelHistory* (*analytical_at)(unsigned,void*)=nullptr;
    void* user=nullptr;
};
struct AreaOptimizationRuntime {
    ShapePromptRuntime prompts;
    void (*read)(const char*,char*,void*);
    void (*message)(const char*,bool,void*);
    int (*format)(char*,const char*,...);
    void (*model_report)(const PiEquivalent&,void*);
    void* user;
};
struct AreaOptimizationOptions {
    GeometryTechnology technology;
    SpiralConstructionRuntime construction;
    AnalyticalModelOptions analytical;
    WarningSink warnings;
    AreaOptimizationRuntime runtime;
};
// Original 0x08071468. Prompts and searches square length/width/quarter-turn
// combinations. Native output slots are assigned only after a retained result;
// failure writes selected_turns=-1. The validated exit metal is then ignored.
bool optimize_area(AreaOptimizationState&,const AreaOptimizationOptions&,AreaOptimizationHistory&);
}
