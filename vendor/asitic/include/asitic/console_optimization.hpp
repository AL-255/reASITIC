#pragma once
#include "asitic/console_geometry.hpp"
#include "asitic/console_analysis.hpp"
#include "asitic/optimization_commands.hpp"
#include "asitic/optimization_prompt.hpp"
#include "asitic/optimization_polygon.hpp"
#include "asitic/optimization_symmetric.hpp"
#include "asitic/optimization_area.hpp"
#include "asitic/optimization_batch.hpp"
#include "asitic/optimization_grid_multimetal.hpp"
namespace asitic {
// Host-owned explicit incoming histories. They repeat for each independently
// reconstructed call; native unwritten stack bytes have no process-independent
// default. Construction mesh counts follow initialize_console_geometry.
// Grid incoming line and argument-name histories start empty. This preserves
// the native first-use batch loop that repeatedly scans its empty local text.
// Interactive input fills that local normally. Embeddings may explicitly edit
// grid_options.history before a batch call; no substitute names are generated.
struct ConsoleOptimizationState {
    ConsoleGeometryState* geometry;
    ConsoleAnalysisState* analysis;
    TechnologyInitialization* technology;
    CommandRuntime* parser;
    void (*read)(const char*,char*,void*);
    void (*invalid)(void*);
    void* user;
    OptimizationSweepState settings;
    OptimizationPromptHistory prompt_history;
    OptimizationSweepHistory keyword_history;
    SquareOptimizationState square;
    SquareOptimizationOptions square_options;
    PolygonOptimizationState polygon;
    PolygonOptimizationOptions polygon_options;
    SquareOptimizationState symmetric_square,symmetric_polygon;
    SymmetricOptimizationOptions symmetric_square_options,symmetric_polygon_options;
    AreaOptimizationState area;
    AreaOptimizationOptions area_options;
    AreaOptimizationHistory area_history;
    OptimizationBatchState batch;
    OptimizationBatchOptions batch_options;
    OptimizationBatchHistory batch_history;
    SquareOptimizationOptions batch_search_options[68];
    AnalyticalModelHistory batch_retained_history[68];
    MultimetalOptimizationGridState multimetal;
    OptimizationGridOptions grid_options;
    MultimetalOptimizationGridOptions multimetal_options;
    SquareConstructionHistory construction_history;
    WireConstructionHistory polygon_construction_history;
    SymmetricConstructionHistory symmetric_construction_history;
    AnalyticalModelHistory analytical_history;
    OptimizationCommandContext commands;
    GeometryShape** active_shapes;
    AnalyticalModelResult* active_model;
    PotentialCapacitanceState* active_capacitance;
};
void initialize_console_optimization(ConsoleOptimizationState&,ConsoleGeometryState&,ConsoleAnalysisState&,
    TechnologyInitialization&,CommandRuntime&,void (*read)(const char*,char*,void*),void (*invalid)(void*),void* user);
// Refresh after host geometry, technology and analysis-control changes. It
// retains optimizer settings and caller-supplied histories between commands.
void refresh_console_optimization(ConsoleOptimizationState&);
// Binds the actual optimizer/sweep source bodies through the parsed-command
// adapter and synchronizes shared geometry/model/cache/control state afterward.
bool execute_console_optimization(int,CommandArguments&,ConsoleOptimizationState&,bool* suppress_recording=nullptr);
}
