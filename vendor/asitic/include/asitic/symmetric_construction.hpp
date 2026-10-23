#pragma once
#include "asitic/spiral_construction.hpp"
namespace asitic {
// Original 0x0805a2fc. Collapse XY starts, preserve discarded trigonometric
// work and its FP status, then apply the direction-4/5/6/7 bridge displacement.
void bridge_symmetric_geometry(int direction,ConductorSegment& segment,double height,double width,double spacing);
struct SymmetricConstructionHistory {
    WireConstructionHistory mesh;
    int polygon_metal; // unwritten selector before the first polygon bridge
    // One raw record per emitted via, in emission order. Null selects zeros.
    // A nonnull array must cover every emission, including a failed allocation.
    const ViaConstructionHistory* vias;
};
// Originals 0x08059854 and 0x0805a45c: both symmetric winding builders,
// including metal crossover state, vias, center transitions and endpoint order.
void build_symmetric_square(GeometryShape*& head,const GeometryShape& arguments,const GeometryTechnology& technology,
    const SpiralConstructionRuntime& runtime,const SymmetricConstructionHistory& history);
void build_symmetric_polygon(GeometryShape*& head,const GeometryShape& arguments,const GeometryTechnology& technology,
    const SpiralConstructionRuntime& runtime,const SymmetricConstructionHistory& history);
// Originals 0x0805b154 and 0x0805b348. Both require a nonempty main list.
// The first extends to a local bounding edge plus width, not the chip boundary.
void extend_geometry_tail_to_bounds(GeometryShape& shape);
void extend_geometry_tail(GeometryShape& shape);
}
