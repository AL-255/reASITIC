#pragma once
#include "asitic/shape_construction.hpp"

namespace asitic {
struct SpiralConstructionRuntime {
    GeometryRuntime geometry;
    int via_count;
    WarningSink warnings;
    int (*format)(char*,const char*,...);
    char* message;
    // Native empty StructureViolation exception. Its one allocated byte is
    // unwritten; the runtime service must not return.
    void (*structure_violation)(void*);
    void* user;
};
struct SquareConstructionHistory {
    WireConstructionHistory mesh;
    ViaConstructionHistory via;
};
// Original 0x0805abec: prepend a via-array segment directly into an existing
// shape; technology via index is separate from its encoded metal index.
void emit_geometry_via(GeometryShape& shape,int nx,int ny,double x,double y,int via,
    const GeometryTechnology& technology,const GeometryRuntime& runtime,const ViaConstructionHistory& history);
// Original 0x0805ad74: first matching metal pair, fitting cut count, warning
// and optional emission. Equal metals or a zero via count return immediately.
void emit_geometry_via_for_pair(GeometryShape& shape,int first_metal,int second_metal,double width,
    double x,double y,int x_direction,int y_direction,const GeometryTechnology& technology,
    const SpiralConstructionRuntime& runtime,const ViaConstructionHistory& history);
// Original 0x0805b254: move the first segment's start while retaining its
// edge offsets. Empty shapes fault, as in the original.
void extend_geometry_head(GeometryShape& shape);
// Original 0x08056670, including quarter turns, underpass/via generation,
// optional rotation, linked order, endpoint direction and mode 0/2 extension.
void build_square_spiral(GeometryShape*& head,const GeometryShape& arguments,int mode,
    const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const SquareConstructionHistory& history);
// Original 0x08057248: polygonal spiral with x87 trigonometry, fractional
// sides, radial decrement, native cleanup/fault/exception order and recentering.
void build_polygon_spiral(GeometryShape*& head,const GeometryShape& arguments,
    const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,WireConstructionHistory history);
}
