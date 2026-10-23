#pragma once
#include "asitic/command_runtime.hpp"
#include "asitic/shape_construction.hpp"
namespace asitic {
// Original 0x080870e4. Uses CommandRuntime.message as the native shared token
// buffer. Tokens are ':' separated; a matching key is split at its first '='.
// Empty delimiter-only input retains the native null-token dereference.
bool extract_shape_argument(const char*,CommandArgument&,const char*,unsigned,
    const CommandRuntime&,CommandParseHistory&);
struct ShapePromptRuntime {
    CommandRuntime command;
    GeometryTechnology technology;
    int via_count;
    double chip_x,chip_y;
    // Native UnableToCreate throw boundary; must not return.
    void (*invalid)(void*);
    void* user;
};
// Original per-parser uninitialized temporary record and scanner contents.
struct ShapePromptHistory {
    CommandArgument temporary[4];
    CommandParseHistory scan;
    int layer;
    double spacing;
    // Optional incoming scan-frame contents, one entry per keyword selector
    // invocation (including absent keys). Native libc calls can reuse that
    // frame between invocations. A null pointer uses the retained scan field.
    const CommandParseHistory* calls=nullptr;
    unsigned call_index=0;
};
bool extract_shape_prompt_argument(const char*,CommandArgument&,const char*,unsigned,
    const CommandRuntime&,ShapePromptHistory&);
// Original 0x08056494 / 0x080563fc.
int find_shape_prompt_metal(char*,const ShapePromptRuntime&,int&);
int find_shape_prompt_via(char*,const ShapePromptRuntime&,int&);
// Original keyword-argument adapters; input normalization mutates the line.
void parse_wire_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_capacitor_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_square_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_polygon_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_transformer_shape_arguments(char*,GeometryShape&,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_balun_shape_arguments(char*,GeometryShape&,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_symmetric_square_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_symmetric_polygon_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_ring_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
void parse_via_shape_arguments(char*,GeometryShape&,const ShapePromptRuntime&,ShapePromptHistory&);
}
