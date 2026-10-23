#pragma once
#include "asitic/combined_network.hpp"
#include "asitic/composite_geometry.hpp"
namespace asitic {
struct LaterTwoPortImpedance { ComplexValue z11,z12,z21,z22; };
// Original 0x0808800c. The native reciprocal conversion ignores y12 and
// preserves its historical first/second port ordering.
LaterTwoPortImpedance invert_transformer_admittance(const TwoPortAdmittance& admittance);
struct TransformerEquivalent {
    double first_inductance,second_inductance,first_resistance,second_resistance;
    double ground_resistance,mutual,coupling;
};
// Original 0x08089cb0, including the impedance scratch-state update.
TransformerEquivalent extract_transformer_equivalent(const TwoPortAdmittance& admittance,double frequency,
    LaterTwoPortImpedance& impedance);
struct LaterAnalysisState {
    GeometryShape* shapes;
    PotentialCapacitanceState capacitance;
    TwoPortAdmittance ports;
    LaterTwoPortImpedance impedance;
    TransformerEquivalent model;
};
struct LaterAnalysisOptions {
    GeometryDiscretization discretization;
    GeometryTechnology technology;
    PotentialCapacitanceOptions capacitance;
    InductanceRunOptions inductance;
    EddyRunOptions eddy;
    NodeEquationOptions nodes;
    GeometryCommandRuntime commands;
    WarningSink warnings;
    bool print;
    void (*report)(const TransformerEquivalent&,double,void*);
    void* report_user;
};
// CALCTRANS, original 0x08051280. Two winding discretizations are copied,
// joined, terminated and solved before extracting the narrowband model.
bool solve_transformer_network(const GeometryShape& first,const GeometryShape& second,const GeometryShape& reference,
    double frequency,double termination,LaterAnalysisState& state,const LaterAnalysisOptions& options);
}
