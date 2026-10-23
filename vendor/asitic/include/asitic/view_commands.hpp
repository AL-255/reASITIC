#pragma once
#include "asitic/native_dispatch.hpp"
#include "asitic/shape_prompts.hpp"
namespace asitic {
struct ViewCommandState {
    double zoom;
    int canvas_width,canvas_height;
    int* visible_metals;
};
struct ViewCommandHistory { int metal[15]; };
// Originals0x08081dc4,0x08081e74,0x0807f954. Fit one shape, the chip, or
// all main-list shapes. BB is a view fit; it produces no printed bounding box.
void fit_view_to_shape(const GeometryShape&,NativeDispatchState&,ViewCommandState&);
void fit_view_to_chip(NativeDispatchState&,ViewCommandState&);
void fit_view_to_shapes(GeometryShape*,NativeDispatchState&,ViewCommandState&);
// Native0x0805bd40 cases SCALE102,METAL104,BB109,VPC110,RULER111.
// SCALE and METAL mutate argument text using the original normalizers. The
// outer script-recording postlude is excluded; optional suppression reports
// its local failure flag. Histories supply resolver scan-frame contents.
bool execute_view_command(int,CommandArguments&,NativeDispatchState&,ViewCommandState&,
    const ShapePromptRuntime&,ViewCommandHistory&,bool* suppress_recording=nullptr);
}
