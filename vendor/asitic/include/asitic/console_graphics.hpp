#pragma once
#include "asitic/x11_events.hpp"
namespace asitic {
// Ordinary source-host composition. All state/reference targets must outlive
// this object. Native incoming local-frame histories remain explicit storage.
struct ConsoleGraphicsState {
    X11State x11;
    X11Runtime runtime;
    X11Library library;
    X11ResourceHistory resource_history;
    X11WindowHistory window_history;
    X11EventHistory event_history;
    NativeDispatchState* native;
    ViewCommandState* view;
    TechnologyCommandSettings* settings;
    const TechnologyTables* tables;
    GeometryShape** shapes;GeometryShape** selected;
    X11EventBindings events;
    unsigned char bound;
};
// Use only for graphical sessions. Headless sessions need not load Xlib.
// Diagnostics/memory callbacks come from prototype; standard Xlib/libc
// services and the DISPLAY resolution are bound here. False means loader error.
bool prepare_console_graphics(ConsoleGraphicsState&,const X11Runtime& prototype);
void initialize_console_graphics(ConsoleGraphicsState&,int argc,char** argv,
    NativeDispatchState&,ViewCommandState&,TechnologyCommandSettings&,const TechnologyTables&,
    GeometryShape*& shapes,GeometryShape*& selected,const X11EventBindings&);
void pump_console_graphics(ConsoleGraphicsState&);
void redraw_console_graphics(ConsoleGraphicsState&);
void cleanup_console_graphics(ConsoleGraphicsState&);
void destroy_console_graphics(ConsoleGraphicsState&);
}
