#pragma once
#include "asitic/potential.hpp"
#include "asitic/solver.hpp"

namespace asitic {
struct PotentialContactOptions {
    int nx,ny,metal_count;
    double width,height;
    double length_step,width_step;
};
// The builder copies frame -8 into contact.index before it is numbered by
// matrix assembly. In the full setup caller this slot holds its saved frame
// pointer from the preceding bounds call. Supply the same incoming word when
// observing complete temporary records independently of that runtime frame.
struct PotentialContactHistory { int index = 0; };
// Originals 0x08056614 and 0x080561e0, including scalar-allocation order,
// uninitialized group bytes and complete 60-byte source-record copying.
void prepend_potential_group(PotentialContactGroup*& groups,SolverMemory memory);
void prepend_potential_contact(PotentialContact*& head,const PotentialContact& value,SolverMemory memory);
// Original 0x08056524: contacts first, then their group, in linked order.
void release_potential_contacts(PotentialContactGroup*& groups,SolverMemory memory);
// Original 0x08056580 requires a nonnull head, including for an empty shape.
void reverse_potential_groups(PotentialContactGroup*& groups);
// Original 0x080536c8: translated bounds, mesh counts, group allocation/name.
// Shape name is a NUL-terminated string in its prefix and fits 79 bytes.
// reuse requires current != nullptr; false prepends a new group on success.
bool prepare_potential_contact_group(const ConductorSegment& segment,const ShapeFilamentList& shape,
    PotentialContactGroup*& current,bool reuse,PotentialContactGroup*& groups,
    const PotentialContactOptions& options,SolverMemory memory,WarningSink warnings = {});
// Original 0x08053a1c, including its front-segment width (+0x30) and metal
// (+0x40), separate from the polygon width/metal used for count selection.
void build_potential_contacts(PotentialContactGroup& group,const ConductorSegment& segment,
    const PotentialContactOptions& options,SolverMemory memory,PotentialContactHistory history = {});
// Original 0x08053580: linked and secondary segment walks, shape-name equality,
// partial failure state and final reversal. Does not clear an existing list.
bool prepare_potential_contacts(const ShapeFilamentList& first,const ShapeFilamentList& second,
    PotentialContactGroup*& groups,const PotentialContactOptions& options,SolverMemory memory,
    PotentialContactHistory history = {},WarningSink warnings = {});
}
