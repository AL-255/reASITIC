#include "numerical_policy.hpp"
namespace { bool reference_mode=false; }
namespace reasitic {
bool reference_numerics(){return reference_mode;}
ReferenceNumericsScope::ReferenceNumericsScope(bool enabled):saved(reference_mode){reference_mode=enabled;}
ReferenceNumericsScope::~ReferenceNumericsScope(){reference_mode=saved;}
}
// Explicit diagnostic boundary for the retained original arithmetic. Production
// starts with the normalized, adaptive-precision geometry integral enabled.
extern "C" void set_reference_numerics(int enabled){reference_mode=enabled!=0;}
