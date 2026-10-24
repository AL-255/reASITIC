#pragma once
#include "asitic/node_equations.hpp"
namespace reasitic {
bool solve_portable_folded_nodes(asitic::ComplexValue* cap,const asitic::ComplexValue* ind,int n,
    asitic::TwoPortAdmittance& ports,const asitic::NodeEquationOptions& options);
}
