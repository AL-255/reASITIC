#pragma once
#include "asitic/node_equations.hpp"
namespace asitic {
struct ThreePortImpedance { ComplexValue z11,z12,z13,z21,z22,z23,z31,z32,z33; };
// Original 0x08054bf8. The first matrix is overwritten with row-sum diagonals
// and negative off-diagonals. The dense 2*n system has three RHS columns;
// first_count and second_count select the two winding boundaries. Allocation,
// link writes and extraction require the same valid/backed indices as native.
// Factor/solve errors are reported and ignored. Output changes before release.
bool solve_three_port_equations(ComplexValue* capacitance,const ComplexValue* inductance,int first_count,int second_count,int n,
    ThreePortImpedance& output,SolverMemory memory,const NodeEquationOptions& options = {});
struct ThreePortEquivalent { double inductance[3],resistance[3],mutual[3],cross_resistance[3],coupling[3]; };
// Original 0x08089d58: uses upper-triangle entries and retains extended L/M
// values for coupling ratios after writing their binary64 output fields.
ThreePortEquivalent extract_three_port_equivalent(const ThreePortImpedance& impedance,double frequency);
}
