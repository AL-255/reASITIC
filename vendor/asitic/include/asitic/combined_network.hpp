#pragma once
#include "asitic/geometry.hpp"
#include "asitic/potential_solver.hpp"
#include "asitic/node_equations.hpp"
#include "asitic/eddy.hpp"

namespace asitic {
struct CombinedNetworkState {
    GeometryShape* shapes;
    PotentialCapacitanceState capacitance;
    TwoPortAdmittance ports;
    PiEquivalent pi;
};
struct CombinedNetworkRuntime {
    // Ordinary time/difftime/printf and pi-report boundaries. Time is read on
    // entry even when timing output is disabled. Report callbacks run before
    // the two local matrices are destroyed.
    int (*time)(int*,void*);
    double (*difference)(int,int,void*);
    void (*elapsed)(double,void*);
    void (*report)(const PiEquivalent&,double,void*);
    void* user;
};
struct CombinedNetworkOptions {
    GeometryDiscretization discretization;
    PotentialCapacitanceOptions capacitance;
    InductanceRunOptions inductance;
    EddyRunOptions eddy;
    NodeEquationOptions nodes;
    WarningSink warnings;
    bool timing,print;
    void (*snapshot)(const char*,const ComplexMatrix&,void*);
    void* snapshot_user;
    CombinedNetworkRuntime runtime;
};
// Original 0x08050b2c: clone/discretize, capacitance, inductance, remove clone,
// optional ZC/YM snapshots, node equations and pi extraction. Preserves stage
// failures, partial state and ordered matrix destruction. Numerical routines
// retain their own progress/snapshot services from their respective options.
bool solve_combined_network(const GeometryShape& first,const GeometryShape& second,double frequency,
    CombinedNetworkState& state,const CombinedNetworkOptions& options,const GeometryRuntime& runtime);
}
