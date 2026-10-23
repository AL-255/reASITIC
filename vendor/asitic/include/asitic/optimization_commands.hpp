#pragma once
#include "asitic/command_runtime.hpp"
#include "asitic/optimization_search.hpp"
#include "asitic/optimization_grid.hpp"
namespace asitic {
struct OptimizationCommandBackends {
    bool (*square)(SquareOptimizationState&,const SquareOptimizationOptions&,int)=optimize_square_spiral;
    void (*grid)(OptimizationGridState&,const OptimizationGridOptions&,int)=run_optimization_grid;
    void (*arguments)(char*,OptimizationSweepState&,const ShapePromptRuntime&,OptimizationSweepHistory&)=parse_optimization_sweep;
    // Other variants bind their distinct state/options through user. A missing
    // backend is reported as unhandled, without changing arguments or state.
    void (*polygon)(int,void*)=nullptr;
    void (*symmetric_square)(int,void*)=nullptr;
    void (*symmetric_polygon)(int,void*)=nullptr;
    void (*area)(void*)=nullptr;
    void (*batch)(void*)=nullptr;
    void (*multimetal_grid)(int,void*)=nullptr;
    void* user=nullptr;
};
struct OptimizationCommandContext {
    SquareOptimizationState* square;
    const SquareOptimizationOptions* square_options;
    OptimizationGridState* grid;
    const OptimizationGridOptions* grid_options;
    OptimizationSweepHistory* arguments_history;
    OptimizationCommandBackends backends;
};
// Native 0x0805bd40 cases700,706..708,711,713..715. Exactly one SWEEP or
// SWEEPMM argument invokes keyword parsing in place before batch_mode=1;
// every other signed argument count invokes interactive batch_mode=0.
// Search return values are ignored. The surrounding runner owns recording.
// False means an unhandled id or a missing concrete backend/context.
bool execute_optimization_command(int,CommandArguments&,OptimizationCommandContext&,bool* suppress_recording=nullptr);
}
