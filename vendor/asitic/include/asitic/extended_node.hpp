#pragma once
#include "asitic/node_equations.hpp"
namespace asitic {
struct ExtendedNodeOptions {
    double frequency=0.;bool dump=false;
    int (*format)(char*,const char*,...)=nullptr;
    char* message=nullptr;WarningSink warnings;
    void (*snapshot)(const char*,const ComplexMatrix&,void*)=nullptr;
    void* user=nullptr;
    // Nonreturning native __assert_fail boundary for invalid wrapped ranges.
    void (*assertion)(const char*,const char*,unsigned,const char*,void*)=nullptr;
};
// Original 0x08055fe0: negative half-sum of adjacent columns, retaining stride.
void stamp_extended_node_columns(const ComplexMatrix& input,ComplexMatrix& output);
// Original 0x0805556c: extended two-port MNA assembly, solve and ordered cleanup.
void solve_extended_node_equations(const ComplexMatrix& capacitance,const ComplexMatrix& inductance,
    TwoPortAdmittance& output,SolverMemory memory,const ExtendedNodeOptions& options={});
}
