#pragma once
#include "asitic/spiral_construction.hpp"
#include "asitic/model.hpp"

namespace asitic {
struct OptimizationRange { double minimum,step,maximum; };
struct SquareOptimizationParameters {
    char name[80];
    OptimizationRange spacing,width;
    double dimension_x,dimension_y,target_inductance_nh,accuracy_percent,frequency;
    double chip_x,chip_y;
    int metal,underpass_metal,quality_metric,differential;
};
struct SquareOptimizationState {
    SquareOptimizationParameters parameters;
    GeometryShape* shapes;
    double selected_spacing,selected_width,selected_turns;
    AnalyticalModelResult model;
};
struct SquareOptimizationHistory {
    // Incoming bytes of the native optimizer's partially initialized shape.
    GeometryShape arguments;
    // Each generated candidate, followed by the final retained solution, has
    // a fresh native constructor/model frame. The caller supplies its history.
    const SquareConstructionHistory* construction;
    const AnalyticalModelHistory* analytical;
    // Optional indexed providers support callers whose search length is not
    // known beforehand. Null providers retain the array interface above.
    const SquareConstructionHistory* (*construction_at)(unsigned,void*)=nullptr;
    const AnalyticalModelHistory* (*analytical_at)(unsigned,void*)=nullptr;
    void* user=nullptr;
};
struct SquareOptimizationRuntime {
    void (*prompt)(SquareOptimizationState&,void*);
    void (*message)(const char*,bool,void*);
    int (*system)(const char*,void*);
    void (*iterations)(int,void*);
    int (*format)(char*,const char*,...);
    char* line;
    void (*model_report)(const PiEquivalent&,void*);
    void* user;
};
struct SquareOptimizationOptions {
    GeometryTechnology technology;
    SpiralConstructionRuntime construction;
    AnalyticalModelOptions analytical;
    WarningSink warnings;
    SquareOptimizationHistory history;
    SquareOptimizationRuntime runtime;
};
// Original 0x0806e0b0. A zero skip_prompt invokes the prompt callback before
// reading settings. Search loops retain x87 intermediates between candidates.
// Ordinary conductor candidates preserve the analytical model's native fault.
bool optimize_square_spiral(SquareOptimizationState&,const SquareOptimizationOptions&,int skip_prompt);
}
