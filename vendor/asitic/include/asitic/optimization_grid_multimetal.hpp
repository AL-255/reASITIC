#pragma once
#include "asitic/optimization_grid.hpp"
#include "asitic/analysis_reports.hpp"
namespace asitic {
struct MultimetalOptimizationGridState {
    OptimizationGridState grid;
    FrequencyGeometryLimits cells;
};
struct MultimetalOptimizationGridOptions {
    OptimizationGridOptions grid;
    GeometryCommandRuntime commands;
    AnalysisReportRuntime frequency_runtime;
    bool progress;
};
// Original 0x08071cec. Uses kinds18/20, always the combined analysis and no
// per-device report. Automatic cells are updated before the first grid point,
// even when all ranges are empty. Interactive simulation_type is incoming
// history: this variant never prompts or initializes that local word.
void run_multimetal_optimization_grid(MultimetalOptimizationGridState&,
    const MultimetalOptimizationGridOptions&,int batch_mode);
}
