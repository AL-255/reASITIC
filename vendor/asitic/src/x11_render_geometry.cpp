#include "asitic/x11_render_geometry.hpp"
#include "asitic/x11_render_context.hpp"
#include "asitic/numerics.hpp"
#include "asitic/x11_render.hpp"
#include "x11_render_math.hpp"
namespace asitic {
namespace {
using x11_render_math::load;using x11_render_math::number;
const ConductorSegment* main_next(const ConductorSegment* s) { return reinterpret_cast<const ConductorSegment*>(s->next_address); }
const ConductorSegment* child_next(const ConductorSegment* s) { return reinterpret_cast<const ConductorSegment*>(s->group_next_address); }
// Keep native arithmetic memory operands: an existing NaN can suppress a
// denormal-operand flag that a separate FLD would otherwise introduce.
long double subtract_memory(long double value,const double& operand) { long double result;asm volatile("fsubl %2":"=t"(result):"0"(value),"m"(operand));return result; }
long double add_memory(long double value,const double& operand) { long double result;asm volatile("faddl %2":"=t"(result):"0"(value),"m"(operand));return result; }
long double rounding(long double value,unsigned short mode) {
    unsigned short saved,changed;long double result;asm volatile("fnstcw %0":"=m"(saved));changed=(saved&0xf3ffu)|mode;
    asm volatile("fldcw %2; frndint; fldcw %3":"=t"(result):"0"(value),"m"(changed),"m"(saved));return result;
}
void polygon(const GeometryShape& shape,const ConductorSegment& s,X11GC* gc,X11RenderState& c,const X11RenderRuntime& r) {
    X11Point p[5];const auto ox=load(number(&shape,0x54)),px=load(c.native.pan_x),half_width=0.5L*c.graphical.canvas_width,zoom=load(c.view.zoom);
    p[0].x=static_cast<short>(x87::truncate_int(zoom*(load(s.outer_start[0])+ox+px)+half_width));
    p[1].x=static_cast<short>(x87::truncate_int(zoom*(load(s.outer_end[0])+ox+px)+half_width));
    p[3].x=static_cast<short>(x87::truncate_int(zoom*(load(s.inner_start[0])+ox+px)+half_width));
    p[2].x=static_cast<short>(x87::truncate_int(zoom*(ox+load(s.inner_end[0])+px)+half_width));
    const auto oy=load(number(&shape,0x5c)),py=load(c.native.pan_y),half_height=0.5L*c.graphical.canvas_height;
    p[0].y=static_cast<short>(x87::truncate_int(half_height-zoom*(load(s.outer_start[1])+oy+py)));
    p[1].y=static_cast<short>(x87::truncate_int(half_height-zoom*(load(s.outer_end[1])+oy+py)));
    p[3].y=static_cast<short>(x87::truncate_int(half_height-zoom*(load(s.inner_start[1])+oy+py)));
    p[2].y=static_cast<short>(x87::truncate_int(half_height-zoom*(oy+load(s.inner_end[1])+py)));
    p[4]=p[0];r.fill_polygon(c.graphical.display,c.graphical.window,*gc,p,5,2,0);
}
}
void x11_render_polygons(const GeometryShape& shape,int layer,X11RenderState& c,const X11RenderRuntime& r) {
    auto* gc=layer<c.tables.metal_count?c.graphical.metal_gcs+layer:c.graphical.via_gcs+(layer-c.tables.metal_count);
    for(auto* p=shape.filaments.first;p;p=main_next(p)) {
        if(p->metal==layer)polygon(shape,*p,gc,c,r);
        for(auto* q=child_next(p);q;q=child_next(q))if(q->metal==layer)polygon(shape,*q,gc,c,r);
    }
}
void x11_render_grid(X11RenderState& c,const X11RenderRuntime& r) {
    const int w=c.graphical.canvas_width,h=c.graphical.canvas_height;const auto z=load(c.view.zoom),nxpan=-load(c.native.pan_x),nypan=-load(c.native.pan_y);
    const auto left=nxpan+static_cast<long double>(-(w/2))/z,right=nxpan+static_cast<long double>(w-w/2)/z;
    long double span_x=right-left,span_y=(nypan+static_cast<long double>(h/2)/z)-(nypan+static_cast<long double>(h/2-h)/z);
    double start_x=x87::binary64(left),start_y;
    if(load(c.native.chip_width)<span_x) { start_x=0.;span_x=load(c.native.chip_width); }
    else { const auto snap=load(c.settings.snap_size);const auto quotient=(load(start_x)+snap*0.5L)/snap;const double rounded=x87::binary64(rounding(quotient,0x400));start_x=x87::binary64(snap*load(rounded)); }
    if(load(c.native.chip_height)<span_y) { start_y=0.;span_y=load(c.native.chip_height); }
    else { const auto snap=load(c.settings.snap_size),bottom=nypan+static_cast<long double>(h/2-h)/z;const auto quotient=(snap*0.5L+bottom)/snap;const double rounded=x87::binary64(rounding(quotient,0x400));start_y=x87::binary64(snap*load(rounded)); }
    const auto grid=load(c.settings.grid_size),panx=load(c.native.pan_x),zoom=load(c.view.zoom),hw=0.5L*c.graphical.canvas_width;
    int a=x87::truncate_int(zoom*(grid+grid+panx)+hw),b=x87::truncate_int((grid+panx)*zoom+hw);
    if(x87::abs(static_cast<long double>(static_cast<int>(static_cast<unsigned>(a)-static_cast<unsigned>(b))))<3.L)return;
    const auto pany=load(c.native.pan_y),hh=0.5L*c.graphical.canvas_height;
    a=x87::truncate_int(hh-zoom*(grid+grid+pany));b=x87::truncate_int(hh-(grid+pany)*zoom);
    if(x87::abs(static_cast<long double>(static_cast<int>(static_cast<unsigned>(a)-static_cast<unsigned>(b))))<3.L)return;
    const int count_x=x87::truncate_int(x87::binary64(rounding(span_x/grid,0x800))),count_y=x87::truncate_int(x87::binary64(rounding(span_y/grid,0x800)));
    auto* gc=c.graphical.metal_gcs+c.tables.metal_count;const unsigned total=static_cast<unsigned>(count_x)*static_cast<unsigned>(count_y);
    auto* points=static_cast<X11Point*>(r.memory.arrays.get(total*4u));if(!points)r.fatal.emit("Ran out of memory.");
    unsigned index=0;
    for(int x=0;x<count_x;++x) {
        if(count_y>0) {
            const auto spacing=load(c.settings.grid_size),py=load(c.native.pan_y),scale=load(c.view.zoom),pixel_x=scale*(load(start_x)+spacing*x+load(c.native.pan_x));
            const auto width=static_cast<long double>(c.graphical.canvas_width),height=static_cast<long double>(c.graphical.canvas_height);
            const short xi=static_cast<short>(x87::truncate_int(width*0.5L+pixel_x));
            for(int y=0;y<count_y;++y) { points[index].x=xi;points[index].y=static_cast<short>(x87::truncate_int(height*0.5L-scale*(load(start_y)+spacing*y+py)));++index; }
        }
    }
    r.draw_points(c.graphical.display,c.graphical.window,*gc,points,static_cast<int>(total),0);r.memory.put(points);
}
void x11_render_direction(const ConductorSegment& s,double origin_x,double origin_y,X11RenderState& c,const X11RenderRuntime& r) {
    const auto original_x=load(origin_x);
    double direction[3];for(unsigned i=0;i<3;++i)asm volatile("fldl %1; fsubl %2; fstpl %0":"=m"(direction[i]):"m"(s.endpoints[i+3]),"m"(s.endpoints[i]):"st","memory");
    const auto magnitude=norm(direction);for(unsigned i=0;i<3;++i)direction[i]=x87::binary64(load(direction[i])/magnitude);
    const double negative_dx=x87::binary64(-load(direction[0])),width=x87::binary64(load(s.width));
    const auto dx=load(s.endpoints[3])-load(s.endpoints[0]),dy=load(s.endpoints[4])-load(s.endpoints[1]),dz=subtract_memory(load(s.endpoints[5]),s.endpoints[2]);
    const double length=x87::binary64(x87::sqrt((dx*dx+dy*dy)+dz*dz));
    const double shortened=x87::binary64(load(length)-load(width)*0x1.999999999999ap-2L);
    const auto start_x=original_x+load(s.endpoints[0]),start_y=load(origin_y)+load(s.endpoints[1]);
    const auto hw=0.5L*c.graphical.canvas_width,hh=0.5L*c.graphical.canvas_height,z=load(c.view.zoom);
    const int tip_x=x87::truncate_int(z*(add_memory(original_x+load(s.endpoints[3]),c.native.pan_x))+hw),tip_y=x87::truncate_int(hh-z*(add_memory(load(origin_y)+load(s.endpoints[4]),c.native.pan_y)));
    const auto ax=load(shortened)*load(direction[0])+start_x,ay=load(shortened)*load(direction[1])+start_y;
    const auto transverse=load(width)*0x1.999999999999ap-3L;const double transverse_stored=x87::binary64(transverse);
    const auto xoff=transverse*load(direction[1]),yoff=load(transverse_stored)*load(negative_dx);
    X11Point p[4];p[0]={static_cast<short>(tip_x),static_cast<short>(tip_y)};
    p[1]={static_cast<short>(x87::truncate_int(z*(add_memory(ax+xoff,c.native.pan_x))+hw)),static_cast<short>(x87::truncate_int(hh-z*(add_memory(ay+yoff,c.native.pan_y))))};
    p[2]={static_cast<short>(x87::truncate_int(z*(add_memory(ax-xoff,c.native.pan_x))+hw)),static_cast<short>(x87::truncate_int(hh-(add_memory(ay-yoff,c.native.pan_y))*z))};p[3]=p[0];
    r.draw_line(c.graphical.display,c.graphical.window,c.graphical.grid_gc,x87::truncate_int(z*(add_memory(start_x,c.native.pan_x))+hw),x87::truncate_int(hh-z*(add_memory(start_y,c.native.pan_y))),tip_x,tip_y);
    r.fill_polygon(c.graphical.display,c.graphical.window,c.graphical.grid_gc,p,4,2,0);
}
void x11_render_directions(const GeometryShape& shape,X11RenderState& c,const X11RenderRuntime& r) {
    for(auto* p=shape.filaments.first;p;p=main_next(p)) {
        if(c.view.visible_metals[p->metal])x11_render_direction(*p,number(&shape,0x54),number(&shape,0x5c),c,r);
        for(auto* q=child_next(p);q;q=main_next(q))if(c.view.visible_metals[q->metal])x11_render_direction(*q,number(&shape,0x54),number(&shape,0x5c),c,r);
    }
}
void x11_render_filaments(const GeometryShape& shape,int nl,int nw,int nt,const FilamentSettings& incoming,X11RenderState& c,const X11RenderRuntime& r) {
    if(nw>c.native.maximum_width_cells)nw=c.native.maximum_width_cells;
    double origin[2];for(unsigned i=0;i<2;++i)origin[i]=x87::binary64(load(number(&shape,0x54+8*i)));
    auto settings=incoming;settings.max_width_cells=c.native.maximum_width_cells;
    if(shape.filaments.first) {
        const double denominator=x87::binary64(3.L*nw);
        for(auto* p=shape.filaments.first;p;p=main_next(p)) {
            const auto radius=load(p->width)/load(denominator);
            for(int x=0;x<nl;++x)for(int y=0;y<nw;++y)for(int z=0;z<nt;++z) {
                Filament f;build_filament(*p,f,origin,x,y,z,nl,nw,nt,settings);
                x11_render_world_line(f.values[0],f.values[1],f.values[3],f.values[4],c,r);
                const double x0=x87::binary64(load(f.values[3])-radius),y0=x87::binary64(load(f.values[4])-radius),
                    x1=x87::binary64(load(f.values[3])+radius),y1=x87::binary64(load(f.values[4])+radius);
                x11_render_layout_box(x0,y0,x1,y1,0,c,r);
            }
        }
    }
    r.get_character(r.user);
}

}
