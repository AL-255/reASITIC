#pragma once
#include "asitic/geometry_dispatch.hpp"
#include "asitic/geometry_reporting.hpp"
#include "asitic/shape_creation_commands.hpp"
#include "asitic/view_commands.hpp"
#include "asitic/symmetric_construction.hpp"
#include "asitic/transformer3d.hpp"
namespace asitic {
// Ordinary allocator-backed host state. Histories are explicit incoming host
// workspaces for the component APIs; native unwritten stack bytes have no
// process-independent default. The host retains zero incoming mesh counts, as observed in native SAVE output before analysis.
struct ConsoleGeometryState {
    GeometryShape* head;
    GeometryShape* selected;
    GeometryRuntime geometry;
    GeometryCommandRuntime commands;
    GeometryTechnology technology;
    ShapePromptRuntime prompt;
    ShapeCreationRuntime creation;
    ShapePromptHistory keyword_history;
    ShapeCreationHistory creation_history;
    GeometryShape arguments[2];
    SquareConstructionHistory square_history;
    TransformerConstructionHistory transformer_history;
    SymmetricConstructionHistory symmetric_history;
    Transformer3DHistory transformer3d_history;
    ViewCommandState view;
    ViewCommandHistory view_history;
    int layer_history;
    void (*invalid)(void*);
    void* user;
};
void initialize_console_geometry(ConsoleGeometryState&,const TechnologyInitialization&,CommandRuntime,
    void (*read)(const char*,char*,void*),void (*invalid)(void*),void* user);
// Refreshes the views after geometry/technology/settings mutations, retaining
// the shared malloc/free-backed GeometryRuntime exposed for numerical drivers.
void refresh_console_geometry(ConsoleGeometryState&,const TechnologyInitialization&,CommandRuntime&,const NativeDispatchState&);
// Composes the reconstructed shape acquisition/builders, geometry edits,
// reports and view handlers. Returns false for ids not bound here.
bool execute_console_geometry_command(int,CommandArguments&,ConsoleGeometryState&,
    const TechnologyInitialization&,CommandRuntime&,NativeDispatchState&,bool* suppress_recording=nullptr);
void release_console_geometry(ConsoleGeometryState&);
}
