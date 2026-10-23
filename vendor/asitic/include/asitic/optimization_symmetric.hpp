#pragma once
#include "asitic/optimization_search.hpp"
#include "asitic/symmetric_construction.hpp"
#include "asitic/shape_prompts.hpp"
namespace asitic {
struct SymmetricOptimizationPromptHistory {
    double target,accuracy,frequency;
    OptimizationRange spacing,width;
    int layer;
};
struct SymmetricOptimizationHistory {
    GeometryShape arguments;
    const SymmetricConstructionHistory* construction;
    const AnalyticalModelHistory* analytical;
    SymmetricOptimizationPromptHistory prompt;
    const SymmetricConstructionHistory* (*construction_at)(unsigned,void*)=nullptr;
    const AnalyticalModelHistory* (*analytical_at)(unsigned,void*)=nullptr;
    void* user=nullptr;
};
struct SymmetricOptimizationOptions {
    GeometryTechnology technology;
    SpiralConstructionRuntime construction;
    AnalyticalModelOptions analytical;
    WarningSink warnings;
    SymmetricOptimizationHistory history;
    SquareOptimizationRuntime runtime;
    const ShapePromptRuntime* prompts;
    void (*read)(const char*,char*,void*);
    void* read_user;
};
// Originals 0x0806ef08 and 0x0806f7c4. Both optimize the native first-port Q,
// retain extended loop accumulators, and leave the best shape allocated.
// A nonzero skip_prompt consumes parameters. In polygon mode the side count
// remains the incoming arguments+0x64 word, as in the original entry.
bool optimize_symmetric_square(SquareOptimizationState&,const SymmetricOptimizationOptions&,int skip_prompt);
bool optimize_symmetric_polygon(SquareOptimizationState&,const SymmetricOptimizationOptions&,int skip_prompt);
}
