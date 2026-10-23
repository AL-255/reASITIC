#pragma once
#include "asitic/x11_runtime.hpp"
#include "asitic/technology_commands.hpp"
namespace asitic {
struct X11RenderState {
    X11State& graphical;
    NativeDispatchState& native;
    ViewCommandState& view;
    TechnologyCommandSettings& settings;
    const TechnologyTables& tables;
    GeometryShape*& shapes;
    GeometryShape*& selected;
};
using X11RenderRuntime=X11Runtime;
}
