#include "asitic/x11_keyboard.hpp"
namespace asitic {
namespace {
struct Viewport { long double y,width,x,height,zoom; };
unsigned viewport_key(const unsigned* pointer,const NativeDispatchState& s,const ViewCommandState& v,Viewport& out) {
    const int half_x=v.canvas_width/2,half_y=v.canvas_height/2;
    const int right=v.canvas_width-half_x,left=-half_x,top=half_y,bottom=half_y-v.canvas_height;
    unsigned key;
    // Preserve the two endpoint subtractions and their retained x87 values.
    // In particular, the native KeySym load precedes the final height subtract.
    asm volatile(
        "fldl %[zoom]; fld %%st(0); fidivrl %[right]; fldl %[x]; fld %%st(0); fchs; fld %%st(0); faddp %%st,%%st(3);"
        "fld %%st(3); fidivrl %[left]; faddp %%st,%%st(1); fld %%st(3); fidivrl %[top]; fxch %%st(3);"
        ".byte 0xde,0xe1;" // right - left
        "fldl %[y]; fld %%st(0); fchs; fld %%st(0); faddp %%st,%%st(5); fld %%st(5); fidivrl %[bottom]; faddp %%st,%%st(1);"
        "movl (%[pointer]),%[key]; .byte 0xde,0xec;" // top - bottom
        "fstpt %[oy]; fstpt %[width]; fstpt %[ox]; fstpt %[height]; fstpt %[oz]"
        : [key] "=&r"(key),[oy] "=m"(out.y),[width] "=m"(out.width),[ox] "=m"(out.x),[height] "=m"(out.height),[oz] "=m"(out.zoom)
        : [pointer] "r"(pointer),[zoom] "m"(v.zoom),[x] "m"(s.pan_x),[y] "m"(s.pan_y),[right] "m"(right),[left] "m"(left),[top] "m"(top),[bottom] "m"(bottom)
        : "memory","st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)");
    return key;
}
long double scale(long double value,long double factor) {
    long double out;asm volatile("fmulp %%st,%%st(1)":"=t"(out):"0"(factor),"u"(value):"st(1)");return out;
}
long double add_to(long double value,long double increment) {
    long double out;asm volatile("faddp %%st,%%st(1)":"=t"(out):"0"(increment),"u"(value):"st(1)");return out;
}
long double subtract_from(long double value,long double decrement) {
    long double out;asm volatile(".byte 0xde,0xe9":"=t"(out):"0"(decrement),"u"(value):"st(1)");return out;
}
void store(double& destination,long double value) { asm volatile("fstpl %0":"=m"(destination):"t"(value):"st"); }
}
void handle_x11_keyboard(const unsigned* pointer,GeometryShape* head,X11State& x11,NativeDispatchState& state,ViewCommandState& view,const X11Runtime& runtime) {
    Viewport extent;const unsigned key=viewport_key(pointer,state,view,extent);
    switch(key) {
    case 0xffe1:case 0xffe2:return;
    case 0xff1b:x11.interaction_mode=0;return;
    case 'Z':store(view.zoom,add_to(extent.zoom,extent.zoom));return;
    case 'z':store(view.zoom,scale(extent.zoom,0.5L));return;
    case 'f':fit_view_to_shapes(head,state,view);return;
    case 'k':x11.interaction_mode=2;return;
    case 0xff51:store(state.pan_x,add_to(extent.x,scale(extent.width,0x1.999999999999ap-4L)));return;
    case 0xff53:store(state.pan_x,subtract_from(extent.x,scale(extent.width,0x1.999999999999ap-4L)));return;
    case 0xff52:store(state.pan_y,subtract_from(extent.y,scale(extent.height,0x1.999999999999ap-4L)));return;
    case 0xff54:store(state.pan_y,add_to(extent.y,scale(extent.height,0x1.999999999999ap-4L)));return;
    default:runtime.bell(x11.display,100);return;
    }
}
}
