#pragma once
#include "asitic/solver.hpp"

namespace asitic {
// Original 0xbc-byte shape. The numerical routines consume its first 0xac bytes.
struct GeometryShape {
    ShapeFilamentList filaments;
    unsigned attributes;
    GeometryShape* next;
    unsigned auxiliary;
    unsigned char suffix[4];
};
static_assert(sizeof(GeometryShape)==0xbc);
static_assert(__builtin_offsetof(GeometryShape,next)==0xb0);
struct GeometryRuntime {
    SolverMemory memory;
    WarningSink errors; // original perror boundary
    void (*terminate)(int,void*);
    void* user;
};
struct GeometryDiscretization { double half_width,length_step; };
// Originals 0x0807a0b8, 0x0807a0f0 and 0x0807a17c. Block pointers cover
// 0x44 bytes. Floating copies use FLD/FSTP, including signaling-NaN quieting.
void copy_geometry_block(void* destination,const void* source);
void copy_geometry_segment(ConductorSegment& destination,const ConductorSegment& source);
void subdivide_geometry_edge(double* destination,const double* source,const double* direction,int index,int count);
// Originals 0x0807a428, 0x0807a46c and 0x0807a520. Reversal requires a nonempty
// main list. The shape translator follows +ec after entering a secondary list.
void reverse_geometry_segments(GeometryShape& shape);
void translate_geometry_segment(ConductorSegment& segment,const GeometryShape& source,const GeometryShape& target);
void translate_geometry_shape(GeometryShape& source,const GeometryShape& target);
// Original 0x0807a5e8: raw whole-record prepend, reversing secondary +e8 links.
void prepend_geometry_segment(const ConductorSegment*& head,const ConductorSegment& source,const GeometryRuntime& runtime);
// Originals 0x0807a6d8, 0x0807a76c and 0x0807a7a0. Only selected fields copy;
// reset/count visit the main list, including negative metal indices in count.
void copy_geometry_shape_fields(GeometryShape& destination,const GeometryShape& source);
void reset_geometry_mesh_counts(GeometryShape& shape);
int count_geometry_metals(const GeometryShape& shape,int metal_count);
// Originals 0x0807a7d0 and 0x0807a81c. Both require nonnull input; releasing all
// shapes leaves the caller's head unchanged, matching the original global.
void release_geometry_segments(const ConductorSegment* head,SolverMemory memory);
void release_geometry_shapes(GeometryShape* head,SolverMemory memory);
// Original 0x0807a850: initializes a8/b4/b8 before checking for null.
void prepend_geometry_shape(GeometryShape*& head,const GeometryRuntime& runtime);
// Originals 0x08078e4c, 0x0807a908 and 0x0807ab68. Discretization tests maximum
// primary/secondary length against min(half_width,length_step), but its ceiling
// count divides by half_width. Clone prepends a shape, emits and reverses its
// main segments. Removing an absent shape follows the native invalid traversal.
void discretize_geometry_segment(GeometryShape& shape,const ConductorSegment& source,
    const GeometryDiscretization& options,const GeometryRuntime& runtime);
GeometryShape* clone_geometry_shape(GeometryShape*& head,const GeometryShape& source,
    const GeometryDiscretization& options,const GeometryRuntime& runtime);
void remove_geometry_shape(GeometryShape*& head,GeometryShape* shape,SolverMemory memory);
}
