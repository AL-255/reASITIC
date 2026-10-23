#pragma once
#include "asitic/x11_render.hpp"
#include "asitic/x11_picking.hpp"
#include "asitic/x11_keyboard.hpp"
#include "asitic/x11_zoom.hpp"
namespace asitic {
struct X11EventHistory {
    CommandArguments arguments;
    X11Event event;
    unsigned keysym;
    unsigned compose[2];
};
struct X11EventBindings {
    char* line; // native shared 1024-byte command/output buffer
    // Bind the real command dispatcher. Native gestures submit MOVE405 and
    // RULER111 using float32 values and otherwise incoming argument bytes.
    void (*dispatch)(int,CommandArguments&,void*);
    void* user;
};
// Original0x0807e050 consumes at most one event with mask0x2804d. Selection,
// drawing, viewport and command effects follow the original event ordering.
// X11EventHistory supplies the native incoming local frame bytes, including
// command records whose count and text fields are deliberately not initialized.
void process_x11_event(X11RenderState&,const X11Runtime&,const X11EventBindings&,X11EventHistory&);
}
