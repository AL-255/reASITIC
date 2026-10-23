#pragma once
#include "asitic/analysis_dispatch.hpp"
#include "asitic/extended_analysis.hpp"
#include "asitic/extended_twoport.hpp"
#include "asitic/port_dispatch.hpp"
namespace asitic {
struct ExtendedAnalysisCommandBackends {
    bool (*pad)(const GeometryShape&,const GeometryShape&,const GeometryShape&,const GeometryShape&,double,
        CombinedNetworkState&,const PadDeembeddingOptions&)=solve_pad_deembedded_network;
    bool (*extended)(const GeometryShape&,const GeometryShape&,double,ExtendedTwoPortState&,
        const ExtendedTwoPortOptions&)=solve_extended_twoport_network;
};
struct ExtendedAnalysisCommandContext {
    unsigned char automatic_cells;
    AutomaticCellState cells;AutomaticCellRuntime cell_runtime;
    CombinedNetworkState* pad_state;PadDeembeddingOptions* pad_options;
    ExtendedTwoPortState* extended_state;ExtendedTwoPortOptions* extended_options;
    PortSweepSettings* port_settings;PortDispatchRuntime* port_runtime;
    ExtendedAnalysisCommandBackends backends;
};
// Native 0x0805bd40 cases PI4(518), PIX(538) and 2PORT(528). PI4/PIX
// always request model output and ignore the numerical return byte. 2PORT
// shares the extended sweep argument convention but selects the canonical
// grounded sweep; its file, formatting and analysis services remain explicit.
// The enclosing dispatcher owns script recording. False means unhandled.
bool execute_extended_analysis_command(int,CommandArguments&,ExtendedAnalysisCommandContext&);
}
