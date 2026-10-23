#pragma once
#include "asitic/optimization.hpp"
#include "asitic/optimization_search.hpp"
#include "asitic/combined_network.hpp"
#include "asitic/port_output.hpp"
namespace asitic {
struct OptimizationGridState {
    OptimizationSweepState settings;
    GeometryShape* shapes;
    AnalyticalModelResult model;
    PotentialCapacitanceState capacitance;
};
struct OptimizationGridHistory {
    GeometryShape arguments;
    char line[300],filename[80];
    OptimizationSweepState prompted;
    int metal_scan;
    const SquareConstructionHistory* construction;
    const AnalyticalModelHistory* analytical;
    const SquareConstructionHistory* (*construction_at)(unsigned,void*)=nullptr;
    const AnalyticalModelHistory* (*analytical_at)(unsigned,void*)=nullptr;
    void* user=nullptr;
};
struct OptimizationGridRuntime {
    void (*read)(const char*,char*,bool,void*);
    void* (*open)(const char*,const char*,void*);
    int (*print)(void*,const char*,...);
    int (*flush)(void*,void*);
    int (*close)(void*,void*);
    void (*message)(const char*,bool,void*);
    int (*format)(char*,const char*,...);
    void* user;
};
struct OptimizationGridOptions {
    ShapePromptRuntime prompts;
    SpiralConstructionRuntime construction;
    AnalyticalModelOptions analytical;
    CombinedNetworkOptions combined;
    PortFileHeaderState header;
    OptimizationGridRuntime runtime;
    OptimizationGridHistory history;
};
// Original 0x0807043c, the actual SWEEP engine. The historical export labels
// it set_cell_size_normal. A nonzero batch_mode consumes settings; zero asks
// the original prompts. File creation precedes name validation. Batch mode
// scans its incoming local text into settings.shape_name, retaining the native
// unwritten constructor-name history. Analysis failures do not stop the grid.
void run_optimization_grid(OptimizationGridState&,const OptimizationGridOptions&,int batch_mode);
}
