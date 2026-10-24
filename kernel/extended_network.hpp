#pragma once
#include "asitic/extended_node.hpp"
namespace reasitic {
void solve_portable_extended_nodes(const asitic::ComplexMatrix& cap,const asitic::ComplexMatrix& impedance,
    asitic::TwoPortAdmittance& ports,asitic::SolverMemory memory,const asitic::ExtendedNodeOptions& options);
}
