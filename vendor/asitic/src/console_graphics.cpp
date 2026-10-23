#include "asitic/console_graphics.hpp"
namespace asitic {
namespace {
X11RenderState render_state(ConsoleGraphicsState& g) { return {g.x11,*g.native,*g.view,*g.settings,*g.tables,*g.shapes,*g.selected}; }
void fit(void* user) { auto& g=*static_cast<ConsoleGraphicsState*>(user);fit_view_to_shapes(*g.shapes,*g.native,*g.view); }
void render(void* user) { auto& g=*static_cast<ConsoleGraphicsState*>(user);auto state=render_state(g);x11_render_scene(state,g.runtime); }
}
bool prepare_console_graphics(ConsoleGraphicsState& g,const X11Runtime& prototype) {
    g.runtime=prototype;
    if(!load_x11_library(g.library,g.runtime))return false;
    resolve_x11_display(0,g.x11,g.runtime);return true;
}
void initialize_console_graphics(ConsoleGraphicsState& g,int argc,char** argv,NativeDispatchState& native,
    ViewCommandState& view,TechnologyCommandSettings& settings,const TechnologyTables& tables,
    GeometryShape*& shapes,GeometryShape*& selected,const X11EventBindings& events) {
    g.native=&native;g.view=&view;g.settings=&settings;g.tables=&tables;g.shapes=&shapes;g.selected=&selected;g.events=events;g.bound=1;
    g.runtime.fit_all=fit;g.runtime.render=render;g.runtime.user=&g;
    initialize_x11_window(argc,argv,g.x11,native,view,tables,g.resource_history,g.window_history,g.runtime);
}
void pump_console_graphics(ConsoleGraphicsState& g) {
    auto state=render_state(g);process_x11_event(state,g.runtime,g.events,g.event_history);
    g.x11.canvas_width=g.view->canvas_width;g.x11.canvas_height=g.view->canvas_height;
}
void redraw_console_graphics(ConsoleGraphicsState& g) { auto state=render_state(g);x11_redraw_view(state,g.runtime); }
void cleanup_console_graphics(ConsoleGraphicsState& g) { cleanup_x11_command(g.x11,g.runtime); }
void destroy_console_graphics(ConsoleGraphicsState& g) {
    if(g.bound) { destroy_x11_window(g.x11,g.tables->metal_count,g.tables->via_count,g.runtime);g.bound=0; }
    release_x11_library(g.library);
}
}
