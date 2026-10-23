#include "asitic/x11_runtime.hpp"
extern "C" void* dlopen(const char*,int);
extern "C" void* dlsym(void*,const char*);
extern "C" int dlclose(void*);
extern "C" char* dlerror();
extern "C" char* getenv(const char*);
extern "C" int scanf(const char*,...);
extern "C" int printf(const char*,...);
extern "C" int getc(void*);
extern "C" void* stdin;
namespace asitic {
namespace {
const char* environment(const char* name,void*) { return getenv(name); }
void error(X11Library& s,const char* text) { unsigned i=0;if(text)for(;i+1<sizeof(s.error) && text[i];++i)s.error[i]=text[i];s.error[i]=0; }
}
bool load_x11_library(X11Library& s,X11Runtime& r,const char* path) {
    s.error[0]=0;s.library=dlopen(path?path:"libX11.so.6",2);
    if(!s.library) { error(s,dlerror());return false; }
    bool complete=true;
#define X11_BIND(member,name) do { r.member=reinterpret_cast<decltype(r.member)>(dlsym(s.library,name));if(!r.member)complete=false; } while(false)
    X11_BIND(open_display,"XOpenDisplay");X11_BIND(create_window,"XCreateWindow");X11_BIND(create_bitmap,"XCreateBitmapFromData");
    X11_BIND(set_properties,"XSetStandardProperties");X11_BIND(create_cursor,"XCreateFontCursor");X11_BIND(select_input,"XSelectInput");X11_BIND(map_window,"XMapWindow");
    X11_BIND(create_gc,"XCreateGC");X11_BIND(set_fill_rule,"XSetFillRule");X11_BIND(set_fill_style,"XSetFillStyle");
    X11_BIND(set_foreground,"XSetForeground");X11_BIND(set_background,"XSetBackground");X11_BIND(set_stipple,"XSetStipple");
    X11_BIND(parse_color,"XParseColor");X11_BIND(allocate_color,"XAllocColor");X11_BIND(set_line_attributes,"XSetLineAttributes");
    X11_BIND(load_font,"XLoadQueryFont");X11_BIND(set_font,"XSetFont");X11_BIND(set_function,"XSetFunction");
    X11_BIND(free_gc,"XFreeGC");X11_BIND(destroy_window,"XDestroyWindow");X11_BIND(close_display,"XCloseDisplay");
    X11_BIND(undefine_cursor,"XUndefineCursor");X11_BIND(sync,"XSync");X11_BIND(bell,"XBell");
    X11_BIND(clear_window,"XClearWindow");X11_BIND(fill_polygon,"XFillPolygon");X11_BIND(draw_points,"XDrawPoints");
    X11_BIND(draw_segments,"XDrawSegments");X11_BIND(draw_string,"XDrawString");X11_BIND(draw_rectangle,"XDrawRectangle");X11_BIND(draw_line,"XDrawLine");
    X11_BIND(create_pixmap,"XCreatePixmap");X11_BIND(fill_rectangle,"XFillRectangle");X11_BIND(copy_area,"XCopyArea");
    X11_BIND(free_pixmap,"XFreePixmap");X11_BIND(define_cursor,"XDefineCursor");X11_BIND(check_mask_event,"XCheckMaskEvent");X11_BIND(lookup_string,"XLookupString");
#undef X11_BIND
    if(!complete) { error(s,"libX11 is missing a required standard ABI symbol.");dlclose(s.library);s.library=nullptr;return false; }
    r.environment=environment;r.scan=::scanf;r.print=::printf;r.get_character=::getc;r.standard_input=::stdin;return true;
}
void release_x11_library(X11Library& s) { if(s.library) { dlclose(s.library);s.library=nullptr; } }
}
