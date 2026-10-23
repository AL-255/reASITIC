#pragma once
#include "asitic/x11_render_context.hpp"
namespace asitic {
// Original0x0807f0c4. Match the native +dc layer, choose its metal/via GC,
// and emit five signed-short XPoints for each closed polygon. Primary links
// follow +ec; secondary links follow +e8, including mismatched parent layers.
void x11_render_polygons(const GeometryShape&,int layer,X11RenderState&,const X11RenderRuntime&);
// Original0x0807f4dc. Floor to the snap grid, ceil the viewing-grid counts,
// suppress sub-three-pixel dots, allocate/draw/release an XPoint array.
// Counts and byte products wrap in the native32-bit domain. A returning fatal
// callback continues into the native failed-allocation write sequence.
void x11_render_grid(X11RenderState&,const X11RenderRuntime&);
// Original0x08081400. Draws current direction with a line and filled arrow;
// despite its old label, this function does not print dimension strings.
void x11_render_direction(const ConductorSegment&,double origin_x,double origin_y,X11RenderState&,const X11RenderRuntime&);
// Original0x080816d8. Visibility-selected primary and secondary arrows;
// this walk follows secondary +ec links (different from polygon rendering).
void x11_render_directions(const GeometryShape&,X11RenderState&,const X11RenderRuntime&);
// Original0x08066078. Draw each numerical filament and a small end marker,
// clamp width subdivisions to native.maximum_width_cells, then consume one
// input character through the runtime. The edge cell size is explicit because
// it comes from the numerical technology state outside the X resource bank.
void x11_render_filaments(const GeometryShape&,int length_count,int width_count,int thickness_count,
    const FilamentSettings&,X11RenderState&,const X11RenderRuntime&);
}
