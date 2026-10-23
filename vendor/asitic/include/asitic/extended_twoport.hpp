#pragma once
#include "asitic/combined_network.hpp"
#include "asitic/extended_node.hpp"
#include "asitic/extended_unfolded.hpp"
#include "asitic/analysis_reports.hpp"
#include "asitic/model.hpp"
namespace asitic {
struct ExtendedTwoPortState { CombinedNetworkState network;double frequency; };
struct ExtendedTwoPortOptions {
    CombinedNetworkOptions network;
    ExtendedNodeOptions nodes;
    GeometryRuntime geometry;
    int (*time)(int*,int*,void*);
    int (*format)(char*,const char*,...);
    char* message;
    void* user;
};
// PIX, original 0x080527a4. Unfolded capacitance and impedance-form inductance
// feed the complete extended node system; timers and report precede cleanup.
bool solve_extended_twoport_network(const GeometryShape&,const GeometryShape&,double,
    ExtendedTwoPortState&,const ExtendedTwoPortOptions&);
struct ExtendedTwoPortDispatchContext {
    unsigned char progress,automatic_cells;
    FrequencyGeometryLimits limits;double alpha,beta;
    AnalysisReportRuntime report;
    GeometryRuntime geometry;
    CombinedNetworkState* combined_state;CombinedNetworkOptions* combined_options;
    ExtendedTwoPortState* extended_state;ExtendedTwoPortOptions* extended_options;
    CapacitiveAnalysisState* capacitive_state;CapacitiveAnalysisOptions* capacitive_options;
    AnalyticalModelHistory* analytical_history;AnalyticalModelResult* analytical_result;
    AnalyticalModelOptions analytical_options;
    void (*analytical_report)(const PiEquivalent&,double,void*);
    void* analytical_report_user;
};
// Originals 0x08052a68 / 0x08052b60. Save progress, optionally update the five
// frequency-dependent geometry limits, dispatch by the actual shape kind, then
// restore progress on a normal return. The X variant uses PIX for known coils;
// Ground uses the ordinary combined network. Native faults retain progress.
bool analyze_extended_twoport(const GeometryShape&,double,int,const GeometryShape&,unsigned char,
    ExtendedTwoPortDispatchContext&);
bool analyze_grounded_twoport(const GeometryShape&,double,int,const GeometryShape&,unsigned char,
    ExtendedTwoPortDispatchContext&);
}
