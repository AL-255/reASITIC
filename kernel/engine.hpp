#pragma once
#include "asitic/geometry.hpp"
namespace reasitic {
struct EngineView {
    asitic::GeometryShape* shapes;
    const asitic::MetalLayer* metals;
    const asitic::ViaLayer* vias;
    int metal_count;
    asitic::WarningSink warnings;
};
EngineView engine_view();
void clear_diagnostics();
void release_simulation_result();
}
