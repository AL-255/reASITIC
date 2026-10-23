#include "asitic/x11_events.hpp"
namespace asitic {
namespace {
long double load(const double& n) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(n):"st");return out; }
long double add(long double a,long double b) { long double out;asm volatile("fadd %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double subtract(long double a,long double b) { long double out;asm volatile("fsub %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double multiply(long double a,long double b) { long double out;asm volatile("fmul %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double divide(long double a,long double b) { long double out;asm volatile("fdiv %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double add_memory(long double a,const double& b) { long double out;asm volatile("fldt %1; faddl %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");return out; }
long double negate(long double n) { long double out;asm volatile("fldt %1; fchs; fstpt %0":"=m"(out):"m"(n):"st");return out; }
double binary64(long double n) { double out;asm volatile("fldt %1; fstpl %0":"=m"(out):"m"(n):"st");return out; }
unsigned binary32(long double n) { unsigned out;asm volatile("fldt %1; fstps %0":"=m"(out):"m"(n):"st");return out; }
double floor64(long double n) {
    unsigned short saved,down;double out;asm volatile("fnstcw %0":"=m"(saved));down=(saved&0xf3ffu)|0x400u;
    asm volatile("fldcw %1; fldt %2; frndint; fldcw %3; fstpl %0":"=m"(out):"m"(down),"m"(n),"m"(saved):"st");return out;
}
int integer(long double n) {
    unsigned short saved,zero;int out;asm volatile("fnstcw %0":"=m"(saved));zero=(saved&255u)|0xc00u;
    asm volatile("fldcw %1; fldt %2; fistpl %0; fldcw %3":"=m"(out):"m"(zero),"m"(n),"m"(saved):"st");return out;
}
int difference(int a,int b) { return static_cast<int>(static_cast<unsigned>(a)-static_cast<unsigned>(b)); }
int sum(int a,int b) { return static_cast<int>(static_cast<unsigned>(a)+static_cast<unsigned>(b)); }
int absolute(int n) { return n<0?static_cast<int>(0u-static_cast<unsigned>(n)):n; }
const double& origin(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const double*>(s.filaments.prefix+offset); }
void synchronize_canvas(X11RenderState& c) { c.view.canvas_width=c.graphical.canvas_width;c.view.canvas_height=c.graphical.canvas_height; }
void line(X11RenderState& c,const X11Runtime& r,int x0,int y0,int x1,int y1) { r.draw_line(c.graphical.display,c.graphical.window,c.graphical.grid_gc,x0,y0,x1,y1); }
void rectangle(X11RenderState& c,const X11Runtime& r,int width,int height) { auto& x=c.graphical;r.draw_rectangle(x.display,x.window,x.grid_gc,x.grid_x,x.grid_y,static_cast<unsigned>(width),static_cast<unsigned>(height)); }
void pixel_origin(X11RenderState& c,int& x,int& y) {
    const long double sx=add_memory(load(origin(*c.selected,0x54)),c.native.pan_x);
    const long double zoom=load(c.view.zoom),half=0.5L;
    x=integer(add(multiply(sx,zoom),multiply(half,c.graphical.canvas_width)));
    const long double sy=add_memory(load(origin(*c.selected,0x5c)),c.native.pan_y);
    y=integer(subtract(multiply(half,c.graphical.canvas_height),multiply(sy,zoom)));
}
unsigned snapped(long double value,long double grid,long double half) {
    const double rounded=floor64(divide(add(value,half),grid));return binary32(multiply(load(rounded),grid));
}
}
void process_x11_event(X11RenderState& c,const X11Runtime& r,const X11EventBindings& b,X11EventHistory& h) {
    auto& x=c.graphical;auto& e=h.event.words;
    if(!r.check_mask_event(x.display,0x2804d,&h.event))return;
    const unsigned type=e[0];const unsigned window=e[4];const int ex=static_cast<int>(e[8]),ey=static_cast<int>(e[9]);
    switch(type) {
    case 2: {
        const int count=r.lookup_string(&h.event,b.line,1024,&h.keysym,h.compose);b.line[count]=0;
        if(e[4]!=x.window) { x11_flush_to_screen();return; }
        synchronize_canvas(c);handle_x11_keyboard(&h.keysym,c.shapes,x,c.native,c.view,r);x11_render_scene(c,r);return;
    }
    case 4:
        if(window!=x.window)return;
        if(e[13]==3) {
            x.grid_x=ex;x.grid_y=ey;x.rubber_y=0;x.rubber_x=0;x.interaction_mode=1;r.define_cursor(x.display,window,x.cursor_zoom);return;
        }
        if(e[13]==1) {
            if(x.interaction_mode==2) { x.grid_x=x.rubber_x=ex;x.grid_y=x.rubber_y=ey;return; }
            const long double zoom=load(c.view.zoom);
            const double wy=binary64(add(divide(difference(x.canvas_height/2,ey),zoom),negate(load(c.native.pan_y))));
            const double wx=binary64(add(divide(difference(ex,x.canvas_width/2),zoom),negate(load(c.native.pan_x))));
            x.interaction_mode=select_geometry_shape_at_point(c.shapes,c.selected,wx,wy)?5:0;x11_render_scene(c,r);return;
        }
        if(e[13]!=2 || !c.selected)return;
        x.interaction_mode=4;x11_render_selected_pixmap(c,r);r.define_cursor(x.display,x.window,x.cursor_drag);
        {
            int px,py;pixel_origin(c,px,py);x.grid_x=ex;x.grid_y=ey;x11_blit_pixmap(px,difference(py,x.pixmap_height),0,c,r);
        }return;
    case 5:
        if(e[13]==3 && x.interaction_mode==1) {
            r.undefine_cursor(x.display,x.window);x.interaction_mode=0;
            if(absolute(difference(ex,x.grid_x))<6 || absolute(difference(ey,x.grid_y))<6)return;
            synchronize_canvas(c);zoom_view_to_rectangle(x.grid_x,x.grid_y,ex,ey,c.native,c.view);x11_render_scene(c,r);return;
        }
        if(e[13]==2 && c.selected) {
            x.interaction_mode=0;x11_free_pixmap(c,r);
            const int half_x=x.canvas_width/2,half_y=x.canvas_height/2;
            const long double zoom=load(c.view.zoom),pan_x=negate(load(c.native.pan_x)),pan_y=negate(load(c.native.pan_y));
            const long double old_x=add(divide(difference(x.grid_x,half_x),zoom),pan_x),new_x=add(pan_x,divide(difference(ex,half_x),zoom));
            const long double old_y=add(pan_y,divide(difference(half_y,x.grid_y),zoom)),new_y=add(pan_y,divide(difference(half_y,ey),zoom));
            const long double dx=subtract(new_x,old_x),dy=subtract(new_y,old_y);
            r.undefine_cursor(x.display,x.window);h.arguments.arguments[0].value=reinterpret_cast<unsigned>(c.selected);
            const long double grid=load(c.settings.snap_size),half=multiply(grid,0.5L);
            h.arguments.arguments[1].value=snapped(dx,grid,half);h.arguments.arguments[2].value=snapped(dy,grid,half);
            b.dispatch(405,h.arguments,b.user);x11_render_scene(c,r);return;
        }
        if(e[13]!=1 || x.interaction_mode!=2)return;
        {
            const int dx=absolute(difference(ex,x.grid_x)),dy=absolute(difference(ey,x.grid_y));
            if(dy<dx) {
                line(c,r,x.grid_x,x.grid_y,x.rubber_x,x.grid_y);line(c,r,x.grid_x,x.grid_y,ex,x.grid_y);
                line(c,r,x.grid_x,sum(x.grid_y,10),x.grid_x,difference(x.grid_y,10));line(c,r,ex,sum(x.grid_y,10),ex,difference(x.grid_y,10));
            } else if(dx<dy) {
                line(c,r,x.grid_x,x.grid_y,x.grid_x,x.rubber_y);line(c,r,x.grid_x,x.grid_y,x.grid_x,ey);
                line(c,r,difference(x.grid_x,10),x.grid_y,sum(x.grid_x,10),x.grid_y);line(c,r,difference(x.grid_x,10),ey,sum(x.grid_x,10),ey);
            }
            const long double zoom=load(c.view.zoom),pan_x=negate(load(c.native.pan_x)),pan_y=negate(load(c.native.pan_y));
            const long double grid=load(c.settings.snap_size),half=multiply(grid,0.5L);
            h.arguments.arguments[0].value=snapped(add(pan_x,divide(difference(x.grid_x,x.canvas_width/2),zoom)),grid,half);
            h.arguments.arguments[1].value=snapped(add(pan_y,divide(difference(x.canvas_height/2,x.grid_y),zoom)),grid,half);
            h.arguments.arguments[2].value=snapped(add(divide(difference(ex,x.canvas_width/2),zoom),pan_x),grid,half);
            h.arguments.arguments[3].value=snapped(add(divide(difference(x.canvas_height/2,ey),zoom),pan_y),grid,half);
            b.dispatch(111,h.arguments,b.user);return;
        }
    case 6:
        if(window!=x.window)return;
        if((e[12]&0x400) && x.interaction_mode==1) {
            const int width=absolute(difference(ex,x.grid_x)),height=absolute(difference(ey,x.grid_y));rectangle(c,r,x.rubber_x,x.rubber_y);rectangle(c,r,width,height);x.rubber_x=width;x.rubber_y=height;return;
        }
        if((e[12]&0x200) && c.selected) {
            int px,py;pixel_origin(c,px,py);x11_blit_pixmap(sum(px,difference(ex,x.grid_x)),sum(difference(py,x.pixmap_height),difference(ey,x.grid_y)),1,c,r);return;
        }
        if(!(e[12]&0x100) || x.interaction_mode!=2)return;
        {
            const int dx=absolute(difference(ex,x.grid_x)),dy=absolute(difference(ey,x.grid_y));
            if(dy<dx) { line(c,r,x.grid_x,x.grid_y,x.rubber_x,x.rubber_y);line(c,r,x.grid_x,x.grid_y,ex,x.grid_y);x.rubber_x=ex;x.rubber_y=x.grid_y; }
            else if(dx<dy) { line(c,r,x.grid_x,x.grid_y,x.rubber_x,x.rubber_y);line(c,r,x.grid_x,x.grid_y,x.grid_x,ey);x.rubber_x=x.grid_x;x.rubber_y=ey; }
        }return;
    case 12:
        if(!ey) { if(window==x.window)x11_render_scene(c,r);else if(x.terminal_present)x11_flush_to_screen(); }
        if(x.interaction_mode==1) { rectangle(c,r,x.rubber_x,x.rubber_y);return; }
        if(x.interaction_mode==2) {
            const int dx=absolute(difference(x.rubber_x,x.grid_x)),dy=absolute(difference(x.rubber_y,x.grid_y));
            if(dy<dx)line(c,r,x.grid_x,x.grid_y,x.rubber_x,x.grid_y);else if(dx<dy)line(c,r,x.grid_x,x.grid_y,x.grid_x,x.rubber_y);
        }return;
    case 22:
        if(e[5]!=x.window)return;
        x.canvas_width=ex;x.canvas_height=ey;synchronize_canvas(c);x11_render_scene(c,r);return;
    default:return;
    }
}
}
