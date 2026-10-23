#include "asitic/x11_runtime.hpp"
namespace asitic {
namespace {
#include "x11_bitmap_data.inc"
unsigned field(void* pointer,unsigned offset) { return *reinterpret_cast<const unsigned*>(static_cast<unsigned char*>(pointer)+offset); }
void* screen(const X11State& s) {
    const unsigned base=field(s.display,0x8c);return reinterpret_cast<void*>(base+static_cast<unsigned>(s.screen)*80u);
}
void copy(char* out,const char* in) { do { *out=*in;++out; } while(*in++); }
int size(int n) {
    const double scale=0x1.4cccccccccccdp-1;long double value;int result;unsigned short saved,truncation;
    asm volatile("fldl %1; fimull %2; fstpt %0":"=m"(value):"m"(scale),"m"(n):"st");
    asm volatile("fnstcw %0":"=m"(saved));truncation=(saved&255u)|0xc00u;
    asm volatile("fldcw %1; fldt %2; fistpl %0; fldcw %3":"=m"(result):"m"(truncation),"m"(value),"m"(saved):"st");return result;
}
double negative_half(const double& n) {
    const double half=0.5;double result;asm volatile("fldl %1; fchs; fmull %2; fstpl %0":"=m"(result):"m"(n),"m"(half):"st");return result;
}
}
void resolve_x11_display(unsigned char no_graphics,X11State& s,const X11Runtime& r) {
    if(no_graphics) { s.disabled=1;r.print("\nRunning in console mode...");return; }
    const char* name=r.environment("DISPLAY",r.user);
    if(name && *name) { copy(s.display_name,name);return; }
    r.warnings.emit("Enter Display Name (X Server):  ");r.scan("%s",s.display_name);
}
void initialize_x11_window(int argc,char** argv,X11State& s,NativeDispatchState& native,ViewCommandState& view,
    const TechnologyTables& tables,X11ResourceHistory& resources,X11WindowHistory& history,const X11Runtime& r) {
    if(s.disabled)return;
    s.display=r.open_display(s.display_name);
    if(!s.display)r.fatal.emit("\nCould Not Open Display.");
    s.screen=static_cast<int>(field(s.display,0x84));
    void* info=screen(s);const int width=static_cast<int>(field(info,12)),height=static_cast<int>(field(info,16));
    s.canvas_width=width;s.canvas_height=height;
    const int canvas_height=size(height),canvas_width=size(width);
    auto* hints=history.size_hints;hints[0]=0x1c;hints[1]=0;hints[2]=0;hints[3]=canvas_width;hints[4]=canvas_height;hints[5]=canvas_width;hints[6]=height/2;
    auto* attributes=history.attributes;attributes[10]=0x28000;attributes[1]=field(screen(s),0x38);attributes[3]=field(screen(s),0x34);attributes[6]=2;
    s.canvas_width=canvas_width;s.canvas_height=canvas_height;view.canvas_width=canvas_width;view.canvas_height=canvas_height;
    s.window=r.create_window(s.display,field(screen(s),8),0,0,canvas_width,canvas_height,2,0,1,nullptr,0x80a,attributes);
    // Original80816d0 is an empty OpenGL compatibility entry.
    const unsigned icon=r.create_bitmap(s.display,field(screen(s),8),reinterpret_cast<const char*>(icon_bits),40,40);
    r.set_properties(s.display,s.window,"ASITIC","ASITIC",icon,argv,argc,hints);
    initialize_x11_resources(s,tables,resources,r);
    s.cursor_drag=r.create_cursor(s.display,0x34);s.cursor_idle=r.create_cursor(s.display,0x96);s.cursor_zoom=r.create_cursor(s.display,0x22);
    native.pan_x=negative_half(native.chip_width);native.pan_y=negative_half(native.chip_height);
    r.select_input(s.display,s.window,0x2804d);r.map_window(s.display,s.window);r.fit_all(r.user);r.render(r.user);
}
void destroy_x11_window(X11State& s,int metals,int vias,const X11Runtime& r) {
    if(s.disabled)return;
    for(int i=0;i<metals;++i)r.free_gc(s.display,s.metal_gcs[i]);
    for(int i=0;i<vias;++i)r.free_gc(s.display,s.via_gcs[i]);
    if(s.metal_gcs)r.memory.put(s.metal_gcs);
    if(s.via_gcs)r.memory.put(s.via_gcs);
    r.destroy_window(s.display,s.window);r.close_display(s.display);
}
void cleanup_x11_command(const X11State& s,const X11Runtime& r) {
    if(!s.disabled) { r.undefine_cursor(s.display,s.window);r.sync(s.display,0); }
}
}
