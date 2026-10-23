#pragma once
#include "asitic/optimization_search.hpp"
#include "asitic/shape_prompts.hpp"
namespace asitic {
struct PolygonOptimizationParameters {
    OptimizationRange width;
    double spacing,diameter,target_inductance_nh,accuracy_fraction,frequency;
    double chip_x,chip_y;
    int sides,metal;
};
struct PolygonOptimizationState {
    PolygonOptimizationParameters parameters;
    GeometryShape* shapes;
    double selected_width,selected_turns;
    AnalyticalModelResult model;
};
struct PolygonOptimizationHistory {
    GeometryShape arguments;
    PolygonOptimizationParameters prompt;
    int layer;
    const WireConstructionHistory* construction;
    const AnalyticalModelHistory* analytical;
    const WireConstructionHistory* (*construction_at)(unsigned,void*)=nullptr;
    const AnalyticalModelHistory* (*analytical_at)(unsigned,void*)=nullptr;
    void* user=nullptr;
};
struct PolygonOptimizationRuntime {
    ShapePromptRuntime prompt;
    void (*read)(const char*,char*,void*);
    void (*message)(const char*,bool,void*);
    int (*system)(const char*,void*);
    int (*format)(char*,const char*,...);
    char* line;
    void (*model_report)(const PiEquivalent&,void*);
    void* user;
};
struct PolygonOptimizationOptions {
    GeometryTechnology technology;
    SpiralConstructionRuntime construction;
    AnalyticalModelOptions analytical;
    WarningSink warnings;
    PolygonOptimizationHistory history;
    PolygonOptimizationRuntime runtime;
};
// Original 0x0806e678. Batch mode uses the literal name "TestOpt" and treats
// accuracy as an incoming fraction. Interactive mode scans percent and scales
// it by .01. Failed searches retain previous selected width/turns. Every normal
// return is zero, including a retained solution; callers inspect state instead.
int optimize_polygon_spiral(PolygonOptimizationState&,const PolygonOptimizationOptions&,int skip_prompt);
}
