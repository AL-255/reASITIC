#pragma once
#include "asitic/green_integration.hpp"

namespace asitic {
#pragma pack(push,4)
struct SubstrateLayer {
    unsigned char prefix[12];
    double resistivity,height,thickness;
    unsigned char suffix[4];
};
#pragma pack(pop)
static_assert(sizeof(SubstrateLayer)==40);
static_assert(__builtin_offsetof(SubstrateLayer,resistivity)==12);
static_assert(__builtin_offsetof(SubstrateLayer,thickness)==28);
// Original 0x0807dc20. The index array has at least max(eddy_count,1) cells.
// Count 1 retains the old second coefficient/depth; other counts except 2 use
// the first index and a fixed depth, even when count is zero or negative.
// Only upper bounds are validated, as in the original. Every accessed index
// must therefore refer to actual storage, including any negative indices.
void configure_green_layers(GreenIntegrationState& state,int& cached_count,const SubstrateLayer* substrate,
    int substrate_count,const int* eddy_indices,int eddy_count,WarningSink warnings = {});
}
