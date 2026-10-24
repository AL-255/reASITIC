#pragma once
#include "asitic/three_port.hpp"
namespace reasitic {
// Native node arithmetic can read past reduced matrices into live work arrays.
// A captured address layout is needed as well as their incoming bytes.
struct ThreePortHistory {
    asitic::SolverMemory memory;
    const asitic::ComplexValue* capacitance;
    const asitic::ComplexValue* inductance;
    const unsigned* addresses; // cap, ind, LU, RHS, pivots
    int cells;
    bool prefix_matches=true,contract_matches=true;
    void* blocks[5]{};
    int allocation=0,n=0;
};
class ThreePortHistoryScope {
    ThreePortHistory* previous;
public:
    explicit ThreePortHistoryScope(ThreePortHistory*);
    ~ThreePortHistoryScope();
};
}
namespace asitic {
bool run_three_port_equations(ComplexMatrix&,const ComplexMatrix&,int,int,int,ThreePortImpedance&,SolverMemory,const NodeEquationOptions&);
}
