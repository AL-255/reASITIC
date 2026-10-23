#include "asitic/x11_render.hpp"
#include "asitic/x11_render_geometry.hpp"
#include "x11_render_math.hpp"
namespace asitic {
namespace {
using namespace x11_render_math;
long double x_coordinate(long double x,const X11RenderState& s) { return load(s.view.zoom)*(x+load(s.native.pan_x))+0.5L*s.graphical.canvas_width; }
long double y_coordinate(long double y,const X11RenderState& s) { return 0.5L*s.graphical.canvas_height-load(s.view.zoom)*(y+load(s.native.pan_y)); }
long double add_memory(long double value,const double& memory) { long double out;asm volatile("faddl %2":"=t"(out):"0"(value),"m"(memory));return out; }
long double x_world_argument(const double& x,const X11RenderState& s) { return load(s.view.zoom)*add_memory(load(x),s.native.pan_x)+0.5L*s.graphical.canvas_width; }
long double y_world_argument(const double& y,const X11RenderState& s) { return 0.5L*s.graphical.canvas_height-load(s.view.zoom)*add_memory(load(y),s.native.pan_y); }
X11GC annotation_gc(const X11RenderState& s) { return s.graphical.metal_gcs[s.tables.metal_count]; }
short pixel(long double x) { return static_cast<short>(x87::truncate_int(x)); }
void zoomed_segment(const ConductorSegment& p,X11RenderState& s,const X11RenderRuntime& r) {
    auto& g=s.graphical;X11Point points[5];
    const long double z=load(s.view.zoom),ox=g.origin_x_pixels,oy=g.origin_y_pixels,h=g.pixmap_height;
    points[0].x=pixel(z*load(p.outer_start[0])-ox);points[1].x=pixel(z*load(p.outer_end[0])-ox);
    points[3].x=pixel(z*load(p.inner_start[0])-ox);points[2].x=pixel(z*load(p.inner_end[0])-ox);
    points[0].y=pixel((h-z*load(p.outer_start[1]))-oy);points[1].y=pixel((h-z*load(p.outer_end[1]))-oy);
    points[3].y=pixel((h-z*load(p.inner_start[1]))-oy);points[2].y=pixel((h-z*load(p.inner_end[1]))-oy);points[4]=points[0];
    X11Segment edges[4]={{points[0].x,points[0].y,points[1].x,points[1].y},
        {points[3].x,points[3].y,points[2].x,points[2].y},{points[0].x,points[0].y,points[3].x,points[3].y},{points[1].x,points[1].y,points[2].x,points[2].y}};
    r.fill_polygon(g.display,g.back_pixmap,g.grid_gc,points,5,2,0);r.draw_segments(g.display,g.back_pixmap,g.grid_gc,edges,4);
}
}
void x11_draw_world_string(const char* text,double x,double y,X11RenderState& s,const X11RenderRuntime& r) {
    unsigned length=0;while(text[length])++length;
    const int py=x87::truncate_int(y_world_argument(y,s)),px=x87::truncate_int(x_world_argument(x,s));
    auto& g=s.graphical;r.draw_string(g.display,g.window,annotation_gc(s),px,py,text,static_cast<int>(length));
}
void x11_draw_world_string_forward(const char* text,double x,double y,X11RenderState& s,const X11RenderRuntime& r) { x11_draw_world_string(text,x,y,s,r); }
void x11_draw_zoom_box(double x0,double y0,double x1,double y1,X11RenderState& s,const X11RenderRuntime& r) {
    const auto z=load(s.view.zoom);const unsigned height=truncate_low64(z*(load(y1)-load(y0))),width=truncate_low64(z*(load(x1)-load(x0)));
    const int py=x87::truncate_int(y_world_argument(y1,s)),px=x87::truncate_int(x_world_argument(x0,s));
    auto& g=s.graphical;r.draw_rectangle(g.display,g.window,annotation_gc(s),px,py,width,height);
}
void x11_draw_selected_box(X11RenderState& s,const X11RenderRuntime& r) {
    double x0,y0,x1,y1;geometry_bounds_xy(*s.selected,&x0,&y0,&x1,&y1);
    x0=x87::binary64(load(x0)+load(number(s.selected,0x54)));x1=x87::binary64(load(x1)+load(number(s.selected,0x54)));
    y0=x87::binary64(load(y0)+load(number(s.selected,0x5c)));y1=x87::binary64(load(y1)+load(number(s.selected,0x5c)));
    x11_draw_zoom_box(x0,y0,x1,y1,s,r);
}
void x11_render_world_line(double x0,double y0,double x1,double y1,X11RenderState& s,const X11RenderRuntime& r) {
    const int px0=x87::truncate_int(x_coordinate(load(x0),s)),px1=x87::truncate_int(x_coordinate(load(x1),s)),
        py0=x87::truncate_int(y_coordinate(load(y0),s)),py1=x87::truncate_int(y_coordinate(load(y1),s));
    auto& g=s.graphical;r.draw_line(g.display,g.window,annotation_gc(s),px0,py0,px1,py1);r.sync(g.display,0);
}
void x11_draw_chip_outline(X11RenderState& s,const X11RenderRuntime& r) {
    const auto z=load(s.view.zoom);const unsigned height=truncate_low64(z*load(s.native.chip_height)),width=truncate_low64(z*load(s.native.chip_width));
    const int py=x87::truncate_int(y_coordinate(load(s.native.chip_height),s)),px=x87::truncate_int(x_coordinate(0.L,s));
    auto& g=s.graphical;r.draw_rectangle(g.display,g.window,annotation_gc(s),px,py,width,height);
}
void x11_render_layout_box(double x0,double y0,double x1,double y1,int metal,X11RenderState& s,const X11RenderRuntime& r) {
    auto& g=s.graphical;X11Point p[5];const short px0=pixel(x_coordinate(load(x0),s)),px1=pixel(x_coordinate(load(x1),s)),py0=pixel(y_coordinate(load(y0),s)),py1=pixel(y_coordinate(load(y1),s));
    p[0]={px0,py0};p[1]={px1,py0};p[2]={px1,py1};p[3]={px0,py1};p[4]=p[0];r.fill_polygon(g.display,g.window,g.metal_gcs[metal],p,5,2,0);
    x11_draw_zoom_box(x0,y0,x1,y1,s,r);r.sync(g.display,0);
}
void x11_set_idle_cursor(X11RenderState& s,const X11RenderRuntime& r) { auto& g=s.graphical;if(!g.disabled) { r.define_cursor(g.display,g.window,g.cursor_idle);r.sync(g.display,0); } }
void x11_blit_pixmap(int x,int y,unsigned char previous,X11RenderState& s,const X11RenderRuntime& r) {
    auto& g=s.graphical;
    if(previous)r.copy_area(g.display,g.back_pixmap,g.window,g.default_gc,0,0,g.pixmap_width,g.pixmap_height,g.blit_prev_x,g.blit_prev_y);
    r.copy_area(g.display,g.back_pixmap,g.window,g.default_gc,0,0,g.pixmap_width,g.pixmap_height,x,y);g.blit_prev_x=x;g.blit_prev_y=y;
}
void x11_free_pixmap(X11RenderState& s,const X11RenderRuntime& r) { auto& g=s.graphical;r.free_pixmap(g.display,g.back_pixmap); }
void x11_render_zoomed_view(X11RenderState& s,const X11RenderRuntime& r) {
    for(auto* p=s.selected->filaments.first;p;p=reinterpret_cast<const ConductorSegment*>(p->next_address)) {
        zoomed_segment(*p,s,r);
        for(auto* q=reinterpret_cast<const ConductorSegment*>(p->group_next_address);q;q=reinterpret_cast<const ConductorSegment*>(q->next_address))zoomed_segment(*q,s,r);
    }
}
void x11_render_selected_pixmap(X11RenderState& s,const X11RenderRuntime& r) {
    double x0,y0,x1,y1;geometry_bounds_xy(*s.selected,&x0,&y0,&x1,&y1);auto& g=s.graphical;const auto z=load(s.view.zoom);
    g.origin_x_pixels=x87::truncate_int(load(x0)*z);g.origin_y_pixels=x87::truncate_int(load(y0)*z);
    const int px0=x87::truncate_int(x_coordinate(load(x0),s)),px1=x87::truncate_int(x_coordinate(load(x1),s));
    g.pixmap_width=static_cast<int>(static_cast<unsigned>(wrap_abs_difference(px1,px0))+10);
    const int py0=x87::truncate_int(y_coordinate(load(y0),s)),py1=x87::truncate_int(y_coordinate(load(y1),s));
    g.pixmap_height=static_cast<int>(static_cast<unsigned>(wrap_abs_difference(py1,py0))+10);
    g.back_pixmap=r.create_pixmap(g.display,g.window,g.pixmap_width,g.pixmap_height,g.depth);
    if(g.back_pixmap==11)r.warnings.emit("Error:  Unable to allocate pixmap memory.");
    else { r.fill_rectangle(g.display,g.back_pixmap,g.background_gc,0,0,g.pixmap_width,g.pixmap_height);x11_render_zoomed_view(s,r); }
}
void x11_render_scene(X11RenderState& s,const X11RenderRuntime& r) {
    auto& g=s.graphical;r.clear_window(g.display,g.window);x11_draw_chip_outline(s,r);
    for(int layer=0;layer<s.tables.metal_count;++layer) {
        for(auto* shape=s.shapes;shape;shape=shape->next)for(int via=0;via<s.tables.via_count;++via)if(s.tables.vias[via].top==layer-1 && !shape->suffix[0])x11_render_polygons(*shape,s.tables.metal_count+via,s,r);
        if(s.view.visible_metals[layer])for(auto* shape=s.shapes;shape;shape=shape->next)if(!shape->suffix[0])x11_render_polygons(*shape,layer,s,r);
    }
    for(auto* shape=s.shapes;shape;shape=shape->next)for(int via=0;via<s.tables.via_count;++via)if(s.tables.vias[via].top==s.tables.metal_count-1 && !shape->suffix[0])x11_render_polygons(*shape,s.tables.metal_count+via,s,r);
    for(auto* shape=s.shapes;shape;shape=shape->next)if(!shape->suffix[0]) {
        const double y=x87::binary64(load(number(shape,0x5c))-20.L);double x;__builtin_memcpy(&x,&number(shape,0x54),8);x11_draw_world_string_forward(reinterpret_cast<const char*>(shape),x,y,s,r);
    }
    if(s.native.show_dimensions)for(auto* shape=s.shapes;shape;shape=shape->next)if(!shape->suffix[0])x11_render_directions(*shape,s,r);
    if(s.selected)x11_draw_selected_box(s,r);
    if(s.settings.grid_enabled)x11_render_grid(s,r);
    r.sync(g.display,0);
}
void x11_flush_to_screen() {}
void x11_redraw_view(X11RenderState& s,const X11RenderRuntime& r) { x11_render_scene(s,r);x11_flush_to_screen(); }
}
