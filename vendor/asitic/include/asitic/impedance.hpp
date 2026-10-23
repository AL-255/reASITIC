#pragma once
#include "asitic/filaments.hpp"

namespace asitic {
struct ComplexValue { double real, imag; };
struct ComplexMatrixView {
    ComplexValue* data;
    int stride;
    ComplexValue& at(int row,int column) const { return data[column*stride+row]; }
};
struct WarningSink {
    void (*write)(const char*,void*) = nullptr;
    void* user = nullptr;
    void emit(const char* s) const { if(write) write(s,user); }
};

#pragma pack(push,4)
struct ShapeFilamentList {
    unsigned char prefix[0xa8];
    const ConductorSegment* first;
};
struct InductanceContext {
    const ShapeFilamentList* shape;
    const ConductorSegment* const* segments;
    double frequency;
    int group_count;
    const int* extra_segments;
    const int* width_counts;
    const int* thickness_counts;
    int segment_count;
    int filament_count;
    int via_count;
    const int* via_indices;
    double origin[2];
    unsigned char unknown_40[8];
    unsigned char array_flags[5];
};
#pragma pack(pop)
static_assert(__builtin_offsetof(InductanceContext,frequency)==8);
static_assert(__builtin_offsetof(InductanceContext,filament_count)==0x24);
static_assert(__builtin_offsetof(InductanceContext,origin)==0x30);
static_assert(__builtin_offsetof(InductanceContext,array_flags)==0x48);

struct ArrayAllocator {
    void* (*allocate)(unsigned bytes,void* user);
    void* user = nullptr;
    void* get(unsigned bytes) const { return allocate?allocate(bytes,user):nullptr; }
};
// Original 0x08064e20, 0x08066500 and 0x08064f6c. Allocation and ownership
// flags preserve the original order, including partial allocation failures.
// The caller supplies/deallocates storage through the allocator boundary.
bool setup_filament_groups(InductanceContext& context,int metal_count,
                            ArrayAllocator allocator,WarningSink warnings = {});
bool index_conductor_segments(InductanceContext& context,int metal_count,
                               ArrayAllocator allocator,WarningSink warnings = {});
bool set_filament_counts(InductanceContext& context,double width_cell,double thickness_cell,
                          int max_width_cells,ArrayAllocator allocator,WarningSink warnings = {});

// Original 0x080650d4. Writes the upper triangle including the diagonal.
// Lower-triangle cells and stride padding are left untouched.
bool fill_filament_impedance(ComplexMatrixView out,const InductanceContext& context,
                             const FilamentSettings& settings,WarningSink warnings = {});
// Original 0x080655a4. Sum the symmetric full matrix into conductor groups.
void reduce_filament_impedance(ComplexMatrixView full,ComplexMatrixView reduced,
                               const InductanceContext& context);
// Original 0x080657b8. Scratch holds context.via_count pointers; nullptr
// reproduces the original allocation-failure path without a runtime allocator.
bool add_via_impedance(ComplexMatrixView out,const InductanceContext& context,
                       const ViaLayer* vias,int metal_count,const ConductorSegment** scratch,
                       WarningSink warnings = {});
}
