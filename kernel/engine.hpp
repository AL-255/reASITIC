#pragma once
#include "asitic/geometry.hpp"
#include "asitic/technology.hpp"
namespace reasitic {
struct EngineView {
    asitic::GeometryShape* shapes;
    const asitic::MetalLayer* metals;
    const asitic::ViaLayer* vias;
    int metal_count;
    asitic::WarningSink warnings;
    const asitic::SubstrateLayer* substrate;
    int substrate_count;
    const double* metal_heights;
};
EngineView engine_view();
void clear_diagnostics();
void release_simulation_result();
void release_capacitance_result();
}
