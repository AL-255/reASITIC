#pragma once
#include "asitic/composite_geometry.hpp"
namespace asitic {
struct GeometryEditRuntime {
    GeometryRuntime geometry;
    GeometryShape* selected;
    WarningSink warnings;
    char* message;
    int (*format)(char*,const char*,...);
};
// Original0x0807ad70. Formats the duplicate warning even on success, searches
// using native exact-name semantics (including selected name ".").
void rename_geometry_shape(GeometryShape* head,GeometryShape& shape,const char* name,const GeometryEditRuntime& runtime);
// Original0x08078a80. Index0 splits at count/2, negative indices use count+i-1.
// Empty input dereferences first+ec; integer division by sides may fault after
// the chain has already been split. No rollback occurs.
void unjoin_geometry_shape(GeometryShape*& head,GeometryShape& shape,int index,const char* name,const GeometryEditRuntime& runtime);
// Original0x0807acf8. Reverses both main chains and inserts one source record
// into the secondary list of each corresponding target record, then removes
// source. The target must have enough main records; the native cursor faults
// on an excess source record after its allocation and translation.
void join_shunt_geometry_shapes(GeometryShape*& head,GeometryShape& target,GeometryShape& source,const GeometryEditRuntime& runtime);
// Original0x08079974. Prepends a kind21 shape per main record and per record
// reached by the native secondary-start/+ec traversal, then removes source.
// Child chains are cloned in reverse order; selected is not updated.
int convert_geometry_to_wires(GeometryShape*& head,GeometryShape& source,const GeometryEditRuntime& runtime);
}
