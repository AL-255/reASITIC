#include "asitic/x11_zoom.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& n) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(n):"st");return out; }
long double add(long double a,long double b) { long double out;asm volatile("fadd %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double sub(long double a,long double b) { long double out;asm volatile("fsub %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double mul(long double a,long double b) { long double out;asm volatile("fmul %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double div(long double a,long double b) { long double out;asm volatile("fdiv %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double neg(long double a) { long double out;asm volatile("fchs":"=t"(out):"0"(a));return out; }
void store(double& out,long double value) { asm volatile("fstpl %0":"=m"(out):"t"(value):"memory","st"); }
int difference(int a,int b) { return static_cast<int>(static_cast<unsigned>(a)-static_cast<unsigned>(b)); }
}
void zoom_view_to_rectangle(int x0,int y0,int x1,int y1,NativeDispatchState& state,ViewCommandState& view) {
    const long double half=0.5L;
    const int middle_x=x87::truncate_int(add(mul(half,difference(x1,x0)),x0));
    const int half_width=view.canvas_width/2;
    const long double old_x=load(state.pan_x),zoom=load(view.zoom);
    const long double pan_x=add(div(difference(middle_x,half_width),zoom),neg(old_x));
    store(state.pan_x,neg(pan_x));
    const int middle_y=x87::truncate_int(add(mul(half,difference(y1,y0)),y0));
    const int half_height=view.canvas_height/2;
    const long double pan_y=add(div(difference(half_height,middle_y),zoom),neg(load(state.pan_y)));
    store(state.pan_y,neg(pan_y));
    const long double x_right=add(pan_x,div(difference(view.canvas_width,half_width),zoom));
    const long double x_left=add(div(difference(0,half_width),zoom),pan_x);
    const long double rectangle_right=add(div(difference(x1,half_width),zoom),pan_x);
    const long double rectangle_left=add(pan_x,div(difference(x0,half_width),zoom));
    const long double bottom=div(difference(half_height,view.canvas_height),zoom);
    const long double scale_x=div(sub(x_right,x_left),sub(rectangle_right,rectangle_left));
    const long double y_bottom=add(bottom,pan_y),y_top=add(div(half_height,zoom),pan_y);
    const long double rectangle_bottom_offset=div(difference(half_height,y1),zoom);
    const long double rectangle_top_offset=div(difference(half_height,y0),zoom);
    const long double y_span=sub(y_bottom,y_top);
    const long double rectangle_bottom=add(rectangle_bottom_offset,pan_y);
    const long double rectangle_top=add(rectangle_top_offset,pan_y);
    const long double scale_y=div(y_span,sub(rectangle_bottom,rectangle_top));
    const long double scale=scale_x>scale_y?scale_x:scale_y;
    store(view.zoom,mul(scale,load(view.zoom)));
}
}
