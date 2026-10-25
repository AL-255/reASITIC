#pragma once
#include "asitic/potential_tables.hpp"

namespace reasitic {
// The same layered Green model in binary64 arithmetic. Historical instruction
// rounding remains available through the explicit reference-numerics boundary.
void generate_portable_potential_tables(int source,int target,
    const asitic::PotentialTableGeometry& geometry,asitic::PotentialTableOutput output,
    asitic::SolverMemory memory,asitic::WarningSink status);
}
