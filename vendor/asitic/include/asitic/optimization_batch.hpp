#pragma once
#include "asitic/optimization_search.hpp"
namespace asitic {
struct OptimizationBatchState {
    SquareOptimizationState search;
    int sides;
};
struct OptimizationBatchRuntime {
    void (*read)(const char*,char*,void*);
    void* (*open)(const char*,const char*,void*);
    int (*print)(void*,const char*,...);
    int (*flush)(void*,void*);
    int (*close)(void*,void*);
    void* user;
};
struct OptimizationBatchHistory { char filename[80]; };
struct OptimizationBatchOptions {
    // One record per native query: target 5,10,15,20 nH, each with dimensions
    // 100..500 in steps of25. Each query retains its own unwritten frames.
    const SquareOptimizationOptions* search;
    const AnalyticalModelHistory* retained_analytical;
    OptimizationBatchRuntime runtime;
};
// Original0x080711cc. Runs all68 native OPTL queries and writes every result.
// File-open failure is not checked before the first fprintf boundary.
void run_optimization_batch(OptimizationBatchState&,const OptimizationBatchOptions&,OptimizationBatchHistory&);
}
