#pragma once
#include "asitic/shape_prompts.hpp"
namespace asitic {
struct ShapeCreationRuntime {
    ShapePromptRuntime shape;
    // Bind to repl_read_command_line with history=false. Cancellation must
    // propagate through this callback, retaining partial argument changes.
    void (*read)(const char*,char*,void*);
    void* user;
    int cells_x=0,cells_y=0;
};
struct ShapeCreationHistory {
    char origin[160];
    int layer;
    int phase;
    char mirror_names[2][80];
    double mirror_values[7];
    int mirror_phase;
    double gap;
    const int* layer_calls=nullptr;
    unsigned layer_index=0;
};
// Shared source helpers for the native acquisition, normalization and layer
// lookup boundaries. The lookup history contract matches ShapePromptHistory.
void read_shape_creation_line(const char*,char*,const ShapeCreationRuntime&);
void normalize_shape_creation_prompt(const char*,char*,const ShapeCreationRuntime&);
int resolve_shape_creation_metal(char*,const ShapeCreationRuntime&,ShapeCreationHistory&);
int resolve_shape_creation_via(char*,const ShapeCreationRuntime&,ShapeCreationHistory&);
// Original 0x08066620 / 0x0806693c. These are interactive turn/dimension
// prompts despite the historical export labels describing filament arrays.
void prompt_shape_turns(double&,double,double,double,int,int,const ShapeCreationRuntime&);
void prompt_shape_dimensions(double&,double&,int&,const ShapeCreationRuntime&);
// Original interactive descriptor acquisition routines. Construction and
// allocation take place in the outer command dispatcher, after these return.
void prompt_square_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x08067084
void prompt_polygon_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x08066b78
void prompt_multimetal_square_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x080676f8
void prompt_wire_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x0806887c
void prompt_capacitor_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x08068cb4
void prompt_via_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x080690b4
void prompt_symmetric_square_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x08069474
void prompt_symmetric_polygon_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x0806a594
void prompt_ring_creation(GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x0806be74
void prompt_transformer_creation(GeometryShape&,GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x08067cf8
void prompt_3d_transformer_creation(GeometryShape&,GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x08068344
void prompt_balun_creation(GeometryShape&,GeometryShape&,const ShapeCreationRuntime&,ShapeCreationHistory&); // 0x0806b670
}
