#pragma once
#include "asitic/impedance.hpp"

namespace asitic {
struct ResistanceTechnology { const MetalLayer* metals;const ViaLayer* vias;int metal_count; };
// Original 0x0804d1e4. The scalar helper uses GHz directly; nonpositive or
// unordered frequency returns its DC estimate. Via records ignore frequency.
long double segment_resistance(const ConductorSegment& segment,double frequency_ghz,const ResistanceTechnology& technology);
// Original RESIS at 0x0804eedc. Follows next_address, ignoring secondary chains.
// The norm/accumulator precision intentionally differs from the scalar helper.
long double shape_resistance_dc(const ShapeFilamentList& shape,const ResistanceTechnology& technology);
// Original RESISHF at 0x0804d70c. Frequency is in Hz. Each segment contribution
// is accumulated through a binary64 spill, preserving the command's formula.
long double shape_resistance_high_frequency(const ShapeFilamentList& shape,double frequency_hz,const ResistanceTechnology& technology);
}
