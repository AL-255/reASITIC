#pragma once
#include "asitic/later_analysis.hpp"
#include "asitic/three_port.hpp"

namespace asitic {
struct ThreePortAnalysisState {
    GeometryShape* shapes;
    PotentialCapacitanceState capacitance;
    ThreePortImpedance impedance;
    ThreePortEquivalent model;
};
struct ThreePortAnalysisOptions {
    GeometryDiscretization discretization;
    GeometryTechnology technology;
    PotentialCapacitanceOptions capacitance;
    InductanceRunOptions inductance;
    EddyRunOptions eddy;
    NodeEquationOptions nodes;
    GeometryCommandRuntime commands;
    bool print;
    void (*report)(const ThreePortEquivalent&,double,void*);
    void* report_user;
};
// Original 0x08051b80. Clones three windings, copies each with a "__" name,
// reverses the third, then joins second/third and first/second. A differently
// named reference has both mesh counts overwritten with ten. Native matrix
// preallocation and the reference-inclusive node count are retained, including
// backed overreads when that count exceeds the reduced matrix dimension.
bool solve_three_port_network(const GeometryShape& first,const GeometryShape& second,
    const GeometryShape& third,GeometryShape& reference,double frequency,
    ThreePortAnalysisState& state,const ThreePortAnalysisOptions& options);
}
