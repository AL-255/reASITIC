#pragma once
#include "asitic/command_runtime.hpp"
#include "asitic/later_analysis.hpp"
#include "asitic/analysis_reports.hpp"
namespace asitic {
struct AutomaticCellState {
    FrequencyGeometryLimits limits;
    double alpha,beta;
    unsigned char print;
};
struct AutomaticCellRuntime {
    const MetalLayer* metals;int metal_count;
    int (*format)(char*,const char*,...);
    char* message;WarningSink output;
};
// Binds command state to canonical update_frequency_geometry (0x080605ec).
// Zero/negative layer counts retain its backed-index access requirements.
void configure_automatic_cells(double,AutomaticCellState&,const AutomaticCellRuntime&);
struct AnalysisCommandBackends {
    bool (*combined)(const GeometryShape&,const GeometryShape&,double,CombinedNetworkState&,
        const CombinedNetworkOptions&,const GeometryRuntime&)=solve_combined_network;
    bool (*transformer)(const GeometryShape&,const GeometryShape&,const GeometryShape&,double,double,
        LaterAnalysisState&,const LaterAnalysisOptions&)=solve_transformer_network;
};
struct AnalysisCommandContext {
    unsigned char automatic_cells;
    AutomaticCellState cells;AutomaticCellRuntime cell_runtime;
    CombinedNetworkState* combined_state;CombinedNetworkOptions* combined_options;
    LaterAnalysisState* transformer_state;LaterAnalysisOptions* transformer_options;
    GeometryRuntime geometry;
    AnalysisCommandBackends backends;
};
// Native 0x0805bd40 cases PI2(515), PI3(517) and CALCTRANS(523).
// Arguments are already parsed. The numerical drivers are the independently
// reconstructed production APIs above; their return values are ignored by the
// native command handler. Returns false for an unhandled id. Common optional
// script recording belongs to the enclosing dispatcher.
bool execute_analysis_command(int,const CommandArguments&,AnalysisCommandContext&);
}
