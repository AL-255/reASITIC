#pragma once
#include "asitic/x11_render_context.hpp"
namespace asitic {
// Native807eed4/80820f4. Via/metal stacking, hidden shapes, names, optional
// direction arrows, selected bounds and grid are drawn in the native order.
void x11_render_scene(X11RenderState&,const X11RenderRuntime&);
void x11_redraw_view(X11RenderState&,const X11RenderRuntime&);
// Native8080b48: fill a world rectangle, outline it and synchronize.
void x11_render_layout_box(double,double,double,double,int,X11RenderState&,const X11RenderRuntime&);
// Native8080df8/8080fb4. Pixmap creation uses native sentinel11 as failure;
// zero is accepted. Shape-local polygon origins and +e8/+ec traversal differ
// from the main scene and retain the original behavior.
void x11_render_selected_pixmap(X11RenderState&,const X11RenderRuntime&);
void x11_render_zoomed_view(X11RenderState&,const X11RenderRuntime&);
// Native8081764/8081814, the latter an ABI forwarding entry.
void x11_draw_world_string(const char*,double,double,X11RenderState&,const X11RenderRuntime&);
void x11_draw_world_string_forward(const char*,double,double,X11RenderState&,const X11RenderRuntime&);
// Native808185c/8081890/8081920. Blit state changes after both copy calls;
// freeing the pixmap retains its handle, as in the original.
void x11_set_idle_cursor(X11RenderState&,const X11RenderRuntime&);
void x11_blit_pixmap(int,int,unsigned char,X11RenderState&,const X11RenderRuntime&);
void x11_free_pixmap(X11RenderState&,const X11RenderRuntime&);
// Native8081938/8081a24/8081c0c/8082010. Rectangle extents use low32 bits of
// a signed64 x87 conversion; world coordinates use signed32 conversion.
void x11_draw_zoom_box(double,double,double,double,X11RenderState&,const X11RenderRuntime&);
void x11_draw_selected_box(X11RenderState&,const X11RenderRuntime&);
void x11_render_world_line(double,double,double,double,X11RenderState&,const X11RenderRuntime&);
void x11_draw_chip_outline(X11RenderState&,const X11RenderRuntime&);
// Native80816c8 is an empty function in this supplied executable.
void x11_flush_to_screen();
}
