#pragma once
#include "asitic/geometry.hpp"

namespace asitic {
struct GeometryTechnology {
    const MetalLayer* metals;
    const ViaLayer* vias;
    const double* metal_heights;
    int metal_count;
};
// Original 0x08057998 leaves these two words in its temporary segment
// unwritten. Preserve incoming stack history explicitly for direct calls.
struct WireConstructionHistory { int width_count,thickness_count; };
// Four via thickness fields (+38/+7c/+c0/+d4) are copied as raw unwritten
// words. Keep their payloads independent, including signaling NaNs.
struct ViaConstructionHistory {
    unsigned thickness[8];
    // Unwritten emitter +ec word, copied before a null-head dereference.
    // A successful prepend replaces it with the actual list link.
    unsigned next_address=0;
};
// Originals 0x08079e80 / 0x08079f10 / 0x08079fa0 / 0x0807a020. Coordinates
// are visited outer start/end, inner start/end, primary start/end; ordered
// strict comparisons retain the accumulator for ties and unordered values.
long double geometry_minimum_y(const ConductorSegment& segment,double accumulator);
long double geometry_minimum_x(const ConductorSegment& segment,double accumulator);
long double geometry_maximum_y(const ConductorSegment& segment,double accumulator);
long double geometry_maximum_x(const ConductorSegment& segment,double accumulator);
// Originals 0x0807a958 and 0x0807a9f4. Empty lists retain +/-1e15 sentinels.
void geometry_minimum_xy(const GeometryShape& shape,double* x,double* y);
void geometry_bounds_xy(const GeometryShape& shape,double* xmin,double* ymin,double* xmax,double* ymax);
// Originals 0x080b4688 / 0x0807a370 / 0x0807abb4. Angles are radians.
// Rotation preserves the asymmetric cosine/sine angle spill and rebases the
// records, moving their minimum coordinates into the shape origin.
void rotate_geometry_point(double* point,double angle,double center_x,double center_y);
void rotate_geometry_segment(ConductorSegment& segment,double angle,double center_x,double center_y);
void rotate_geometry_shape(GeometryShape& shape,double angle);
// Originals 0x0807a2d4 / 0x0807a8b8: reverse endpoints, retaining list order.
void reverse_geometry_endpoints(ConductorSegment& segment);
void reverse_geometry_directions(GeometryShape& shape);
// Original 0x0805b8d4: adds offsets to primary records only, in native order.
void offset_geometry_shape(GeometryShape& shape,double x,double y);
// Originals 0x0805b978 / 0x0805baec / 0x0807a25c. Height values populate
// endpoint Zs; the exported decompilation incorrectly describes them as colors.
void initialize_geometry_metal(ConductorSegment& segment,const GeometryShape& shape,const GeometryTechnology& technology);
void set_geometry_metal(ConductorSegment& segment,int metal,const GeometryTechnology& technology);
void refresh_geometry_metal(ConductorSegment& segment,int metal,const GeometryTechnology& technology);
// Originals 0x0805bab4 / 0x0805bb64: copies end XY to start, then optionally
// extends the end in one cardinal direction. Z and other fields remain intact.
void collapse_geometry_xy(ConductorSegment& segment);
void extend_geometry_xy(unsigned direction,ConductorSegment& segment,double length,double inset);
// Originals 0x08057998 / 0x08057b78. Both prepend a shape and one segment.
// Wire applies optional angle and both apply negative-attribute direction.
void build_wire_shape(GeometryShape*& head,const GeometryShape& arguments,const GeometryTechnology& technology,
    const GeometryRuntime& runtime,WireConstructionHistory history);
void build_via_shape(GeometryShape*& head,const GeometryShape& arguments,const GeometryTechnology& technology,
    const GeometryRuntime& runtime,const ViaConstructionHistory& history);
}
