#pragma once
#include "asitic/view_commands.hpp"
namespace asitic {
using X11Drawable=unsigned;
using X11GC=void*;
struct X11Point { short x,y; };
struct X11Segment { short x1,y1,x2,y2; };
struct X11Color { unsigned pixel;unsigned short red,green,blue;unsigned char flags,pad; };
static_assert(sizeof(X11Color)==12);
// The first 0x144 bytes reproduce the native global bank at0x080d8b00.
// View adapters may reference the shared NativeDispatchState/ViewCommandState;
// the host synchronizes their canvas fields with this resource bank.
struct X11State {
    char display_name[200];
    int canvas_width,canvas_height;
    float origin3d[3];
    void* display;int screen;
    unsigned window,aux_window,back_pixmap,aux_pixmap;
    unsigned cursor_drag,cursor_idle,cursor_zoom;
    int depth,pixmap_width,pixmap_height;
    int grid_x,grid_y,rubber_x,rubber_y,origin_x_pixels,origin_y_pixels;
    int font_height,font_width,font_baseline;
    X11GC* metal_gcs;X11GC* via_gcs;
    X11GC grid_gc,default_gc,background_gc;
    // Additional native globals outside the contiguous resource bank.
    int blit_prev_x,blit_prev_y,interaction_mode;
    unsigned char disabled,terminal_present;
};
static_assert(__builtin_offsetof(X11State,display)==0xdc);
static_assert(__builtin_offsetof(X11State,metal_gcs)==0x130);
struct X11Event { unsigned words[24]; };
static_assert(sizeof(X11Event)==96);
struct X11ResourceHistory { X11Color color; };
struct X11WindowHistory { unsigned size_hints[18],attributes[15]; };
struct X11Runtime {
    SolverMemory memory;WarningSink warnings,fatal;
    const char* (*environment)(const char*,void*);
    int (*scan)(const char*,...);
    int (*print)(const char*,...);
    void* (*open_display)(const char*);
    unsigned (*create_window)(void*,unsigned,int,int,unsigned,unsigned,unsigned,int,unsigned,void*,unsigned,void*);
    unsigned (*create_bitmap)(void*,unsigned,const char*,unsigned,unsigned);
    int (*set_properties)(void*,unsigned,const char*,const char*,unsigned,char**,int,void*);
    unsigned (*create_cursor)(void*,unsigned);
    int (*select_input)(void*,unsigned,int);
    int (*map_window)(void*,unsigned);
    X11GC (*create_gc)(void*,unsigned,unsigned,void*);
    int (*set_fill_rule)(void*,X11GC,int);
    int (*set_fill_style)(void*,X11GC,int);
    int (*set_foreground)(void*,X11GC,unsigned);
    int (*set_background)(void*,X11GC,unsigned);
    int (*set_stipple)(void*,X11GC,unsigned);
    int (*parse_color)(void*,unsigned,const char*,X11Color*);
    int (*allocate_color)(void*,unsigned,X11Color*);
    int (*set_line_attributes)(void*,X11GC,unsigned,int,int,int);
    void* (*load_font)(void*,const char*);
    int (*set_font)(void*,X11GC,unsigned);
    int (*set_function)(void*,X11GC,int);
    int (*free_gc)(void*,X11GC);
    int (*destroy_window)(void*,unsigned);
    int (*close_display)(void*);
    int (*undefine_cursor)(void*,unsigned);
    int (*sync)(void*,int);
    int (*bell)(void*,int);
    int (*clear_window)(void*,unsigned);
    int (*fill_polygon)(void*,unsigned,X11GC,X11Point*,int,int,int);
    int (*draw_points)(void*,unsigned,X11GC,X11Point*,int,int);
    int (*draw_segments)(void*,unsigned,X11GC,X11Segment*,int);
    int (*draw_string)(void*,unsigned,X11GC,int,int,const char*,int);
    int (*draw_rectangle)(void*,unsigned,X11GC,int,int,unsigned,unsigned);
    int (*draw_line)(void*,unsigned,X11GC,int,int,int,int);
    unsigned (*create_pixmap)(void*,unsigned,unsigned,unsigned,unsigned);
    int (*fill_rectangle)(void*,unsigned,X11GC,int,int,unsigned,unsigned);
    int (*copy_area)(void*,unsigned,unsigned,X11GC,int,int,unsigned,unsigned,int,int);
    int (*free_pixmap)(void*,unsigned);
    int (*define_cursor)(void*,unsigned,unsigned);
    int (*check_mask_event)(void*,int,X11Event*);
    int (*lookup_string)(X11Event*,char*,int,unsigned*,void*);
    int (*get_character)(void*);
    void* standard_input;
    void (*fit_all)(void*);
    void (*render)(void*);
    void* user;
};
// Native804c118,807ff18,807fc9c,8081d10,8081830 respectively.
void resolve_x11_display(unsigned char no_graphics,X11State&,const X11Runtime&);
void initialize_x11_resources(X11State&,const TechnologyTables&,X11ResourceHistory&,const X11Runtime&);
void initialize_x11_window(int,char**,X11State&,NativeDispatchState&,ViewCommandState&,
    const TechnologyTables&,X11ResourceHistory&,X11WindowHistory&,const X11Runtime&);
void destroy_x11_window(X11State&,int metal_count,int via_count,const X11Runtime&);
void cleanup_x11_command(const X11State&,const X11Runtime&);
struct X11Library { void* library;char error[256]; };
// Loads ordinary libX11.so.6 and replaces only external Xlib/libc services.
// Memory, diagnostics, fit/render composition and user data are retained.
bool load_x11_library(X11Library&,X11Runtime&,const char* library=nullptr);
void release_x11_library(X11Library&);
}
