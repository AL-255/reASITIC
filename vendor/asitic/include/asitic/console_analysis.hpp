#pragma once
#include "asitic/technology_text.hpp"
#include "asitic/analysis_commands.hpp"
#include "asitic/analysis_dispatch.hpp"
#include "asitic/extended_analysis.hpp"
#include "asitic/extended_analysis_dispatch.hpp"
#include "asitic/three_port_analysis.hpp"
#include "asitic/port_dispatch.hpp"
#include "asitic/port_output.hpp"
#include "asitic/two_port_output.hpp"

namespace asitic {
struct ConsoleAnalysisControls {
    FrequencyGeometryLimits limits;
    double alpha,beta;
    int maximum_width_cells,eddy_mode;
    unsigned char automatic_cells,progress,timing,dump_matrices,verbose;
    unsigned char technology_io=0;
};
// Ordinary host storage for the reconstructed numerical command contexts.
// No reference executable or foreign numerical runtime is loaded. The cache
// bank is shared among command drivers just as in the native session.
struct ConsoleAnalysisState {
    TechnologyInitialization* technology;
    GeometryShape** shapes;
    GeometryRuntime geometry;
    GeometryCommandRuntime commands;
    AnalysisReportRuntime report;
    WarningSink status;
    WarningSink progress_output; // native804bf94: inline dots, mirrored to logs
    CharacterTables characters;
    ConsoleAnalysisControls controls;
    PotentialCapacitanceState capacitance;
    CombinedNetworkState combined_state;
    LaterAnalysisState transformer_state;
    ExtendedTwoPortState extended_state;
    CapacitiveAnalysisState capacitive_state;
    ThreePortAnalysisState three_port_state;
    AnalyticalModelHistory analytical_history;
    AnalyticalModelResult analytical_result;
    TwoPortAdmittance admittance;
    PiEquivalent pi;
    CombinedNetworkOptions combined_options;
    LaterAnalysisOptions transformer_options;
    ExtendedTwoPortOptions extended_options;
    CapacitiveAnalysisOptions capacitive_options;
    ThreePortAnalysisOptions three_port_options;
    PadDeembeddingOptions pad_options;
    InductanceMatrixReportOptions matrix_options;
    ExtendedTwoPortDispatchContext twoport;
    SimpleAnalysisCommandContext simple;
    AnalysisCommandContext analysis;
    ExtendedAnalysisCommandContext extended_analysis;
    PortSweepSettings port_settings;
    PortDispatchRuntime ports;
    PortFileHeaderState port_header;
    PortFileHeaderHistory port_header_history;
    TwoPortOutputState two_port_output;
    ThreePortOutputState three_port_output;
    GreenIntegrationWorkspace green_workspace;
    bool cache_allocated;
};
bool initialize_console_analysis(ConsoleAnalysisState&,TechnologyInitialization&,GeometryShape*&,
    const GeometryRuntime&,const GeometryCommandRuntime&,const AnalysisReportRuntime&,CharacterTables);
// Copies current control fields into all driver contexts. Embedding code may
// update controls after CELL/AUTOCELL/MAXNW/EDDY/TIME/SAVEMAT/VERBOSE commands.
void refresh_console_analysis(ConsoleAnalysisState&);
bool execute_console_analysis(int,CommandArguments&,ConsoleAnalysisState&,GeometryShape*&,const CommandRuntime&);
void release_console_analysis(ConsoleAnalysisState&);
}
