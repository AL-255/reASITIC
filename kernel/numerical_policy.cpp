#include "numerical_policy.hpp"
namespace { bool reference_mode=false; }
namespace reasitic {
bool reference_numerics(){return reference_mode;}
ReferenceNumericsScope::ReferenceNumericsScope(bool enabled):saved(reference_mode){reference_mode=enabled;}
ReferenceNumericsScope::~ReferenceNumericsScope(){reference_mode=saved;}
}
// Explicit diagnostic boundary for retained original arithmetic and matrix
// dimensions. Production uses guarded integrals and consistent node dimensions.
extern "C" void set_reference_numerics(int enabled){reference_mode=enabled!=0;}
