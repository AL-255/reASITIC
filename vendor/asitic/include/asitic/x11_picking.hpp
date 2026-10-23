#pragma once
#include "asitic/geometry.hpp"
namespace asitic {
// Original 0x08081aa4: ordered half-open Y crossings and strict X-ray parity.
// The query coordinates are loaded even when count<=0. Interpolation retains
// x87 extended intermediates; coordinate arrays are separate binary64 values.
int point_in_polygon_parity(int count,const double* x,const double* y,double px,double py);
// Original 0x08080d64: quadrilateral order is outer start, outer end, inner
// end, inner start. Each coordinate plus shape origin spills to binary64.
bool geometry_segment_contains_point(const GeometryShape&,const ConductorSegment&,double px,double py);
// Original 0x08081b6c: main +ec chain only; secondary +e8 links are ignored.
bool geometry_shape_contains_point(const GeometryShape&,double px,double py);
// Original 0x08081bb4: first intersecting shape whose +b8 byte is zero wins.
// Hidden shapes are tested geometrically before their flag is inspected.
// A miss clears selected, including for an empty head.
bool select_geometry_shape_at_point(GeometryShape* head,GeometryShape*& selected,double px,double py);
}
