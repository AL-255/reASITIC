#pragma once
#include "asitic/potential_solver.hpp"
#include "asitic/geometry.hpp"
#include "asitic/network.hpp"
namespace asitic {
struct CapacitiveAnalysisState {
    PotentialCapacitanceState capacitance;
    TwoPortAdmittance ports;
};
struct CapacitiveAnalysisOptions {
    PotentialCapacitanceOptions capacitance;
    const MetalLayer* metals;
    bool print;
    int (*format)(char*,const char*,...);
    int (*table)(const char*,...);
    WarningSink warnings;
};
// Original 0x080515e4. The kind-4 capacitor case updates two-port admittance
// from matrix cells 0, 1 and 3, then optionally reports its RC equivalent.
// Other shapes leave admittance untouched and print small upper triangles,
// independently of print. Capacitance generation always enables verbosity.
bool solve_capacitive_analysis(const GeometryShape& shape,const GeometryShape& ground,double frequency,
    CapacitiveAnalysisState& state,const CapacitiveAnalysisOptions& options,SolverMemory memory);
}
