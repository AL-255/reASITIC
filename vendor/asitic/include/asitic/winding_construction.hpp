#pragma once
#include "asitic/spiral_construction.hpp"

namespace asitic {
// Originals 0x08078bf8 and 0x08078d20. Reflection includes secondary +e8
// chains, followed by the original origin update/translation traversal.
void reflect_geometry_x(GeometryShape& shape);
void reflect_geometry_y(GeometryShape& shape);
struct TransformerConstructionHistory { SquareConstructionHistory winding[2]; };
// Original 0x080576d4: two square windings in mode 3, reflected secondary,
// lead adjustments, primary-origin shift and reciprocal +b4 partner pointers.
void build_planar_transformer(GeometryShape*& head,const GeometryShape& primary,const GeometryShape& secondary,
    const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const TransformerConstructionHistory& history);
// Original 0x0805b450: polygonal ring with an angular gap (argument +90),
// rounded cosine/ribbon spills and native gap-violation runtime order.
void build_ring_shape(GeometryShape*& head,const GeometryShape& arguments,const GeometryTechnology& technology,
    const SpiralConstructionRuntime& runtime,WireConstructionHistory history);
}
