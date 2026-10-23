#pragma once
#include "asitic/analysis_reports.hpp"
#include "asitic/command_runtime.hpp"
#include "asitic/resistance.hpp"
#include "asitic/extended_twoport.hpp"
namespace asitic {
// Originals0x0804ce78 and0x0804d03c. The estimates are in nH. Coupling
// translates segment endpoints in place; an error retains those translations.
long double shape_inductance_estimate(const ShapeFilamentList&,const ResistanceTechnology&,WarningSink = {});
long double shape_coupling_estimate(const ShapeFilamentList&,const ShapeFilamentList&,double& mutual,const ResistanceTechnology&,WarningSink = {});
struct SelfResonanceRuntime {
    // Native8052b60 boundary. The canonical binding always requests progress.
    void (*analyze)(const GeometryShape&,double,int,const GeometryShape&,void*);
    void* user;
    const TwoPortAdmittance* admittance;
    int (*progress)(const char*,...);
    int (*flush)(void*);
    void* output;
    AnalysisReportRuntime report;
};
// Original0x0804e590. The iteration-limit argument is ignored: native observes
// at most100 samples, and can execute101 analyses. Returns an R80 frequency.
long double find_self_resonant_frequency(const GeometryShape&,double first,double second,double step,
    int differential,int fast,int ignored_iteration_limit,const GeometryShape&,const SelfResonanceRuntime&);
// Original0x0804ebd0. Converts the currentY matrix and prints its impedance.
void report_input_impedance(const TwoPortAdmittance&,bool differential,int port,const AnalysisReportRuntime&);
struct SimpleAnalysisCommandBackends {
    long double (*inductance)(const ShapeFilamentList&,const ResistanceTechnology&,WarningSink)=shape_inductance_estimate;
    long double (*coupling)(const ShapeFilamentList&,const ShapeFilamentList&,double&,const ResistanceTechnology&,WarningSink)=shape_coupling_estimate;
    long double (*resistance_dc)(const ShapeFilamentList&,const ResistanceTechnology&)=shape_resistance_dc;
    long double (*resistance_hf)(const ShapeFilamentList&,double,const ResistanceTechnology&)=shape_resistance_high_frequency;
    bool (*analytical)(const ShapeFilamentList&,double,const AnalyticalModelOptions&,AnalyticalModelHistory&,AnalyticalModelResult&,WarningSink)=solve_analytical_model;
    bool (*capacitive)(const GeometryShape&,const GeometryShape&,double,CapacitiveAnalysisState&,const CapacitiveAnalysisOptions&,SolverMemory)=solve_capacitive_analysis;
    void (*matrix_report)(const GeometryShape&,double,const char*,GeometryShape*&,const InductanceMatrixReportOptions&)=report_inductance_matrix;
    bool (*grounded)(const GeometryShape&,double,int,const GeometryShape&,unsigned char,ExtendedTwoPortDispatchContext&)=analyze_grounded_twoport;
    long double (*shunt)(const PiEquivalent&,int)=pi_shunt_resistance;
    long double (*self_resonance)(const GeometryShape&,double,double,double,int,int,int,const GeometryShape&,const SelfResonanceRuntime&)=find_self_resonant_frequency;
    void (*input_impedance)(const TwoPortAdmittance&,bool,int,const AnalysisReportRuntime&)=report_input_impedance;
};
// Native ZIN with exactly two arguments reads an incoming stack double for
// frequency. The embedding caller supplies that history explicitly.
struct SimpleAnalysisCommandHistory { double zin_frequency,coupling_mutual; };
struct SimpleAnalysisCommandContext {
    ResistanceTechnology resistance;
    ExtendedTwoPortDispatchContext* twoport;
    TwoPortAdmittance* admittance;
    PiEquivalent* pi;
    GeometryShape** shapes;
    InductanceMatrixReportOptions* matrix_options;
    CharacterTables characters;
    AnalysisReportRuntime report;
    SelfResonanceRuntime resonance;
    SimpleAnalysisCommandHistory history;
    SimpleAnalysisCommandBackends backends;
};
// Native0x0805bd40 cases500..505,527,531,533..535. Numerical defaults bind
// the canonical production APIs. Text options normalize in place. The enclosing
// command runner owns script recording; false means an unhandled command id.
bool execute_simple_analysis_command(int,CommandArguments&,SimpleAnalysisCommandContext&);
}
