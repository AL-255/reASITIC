#pragma once
#include "asitic/winding_construction.hpp"
namespace asitic {
// Native name search includes the special name ".", which returns selected
// even when selected is null. All other names scan the current main shape list.
// Original 0x0807ab04.
GeometryShape* find_geometry_shape(GeometryShape* head,GeometryShape* selected,const char* name);
struct GeometryCommandRuntime {
    SpiralConstructionRuntime construction;
    GeometryShape* selected;
    // Private naming workspace, separate from the geometry allocator. As for
    // native string allocation, storage must succeed or terminate externally.
    SolverMemory names;
    int (*random)(void*);
    void* user;
};
// Original 0x080782b0. A null name generates source + "_" plus raw random bytes
// on collision. A duplicate explicit name returns null but retains the new head.
// Metal retargeting updates primary records only; children retain their metals.
GeometryShape* copy_geometry_shape(GeometryShape*& head,const GeometryShape& source,const char* name,
    double x,double y,int metal,const GeometryTechnology& technology,const GeometryCommandRuntime& runtime);
// Original 0x08078890. Name-based self guard, via-layer validation, ordered
// translation/copy, conditional metadata carry and source removal. Empty lists
// and allocation failures retain the original faults/partial state.
void join_geometry_shapes(GeometryShape*& head,GeometryShape& target,GeometryShape& source,
    const GeometryTechnology& technology,const GeometryCommandRuntime& runtime);
// Original 0x0805af5c. Arguments are temporarily mutated while building the
// first winding. The native pointer workspace requests delta*4 bytes although
// it writes delta+1 pointers; keep that allocation/write sequence.
void build_multimetal_square(GeometryShape*& head,GeometryShape& arguments,const GeometryTechnology& technology,
    const GeometryCommandRuntime& runtime,const SquareConstructionHistory& history);
}
