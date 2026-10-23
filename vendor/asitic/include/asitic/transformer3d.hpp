#pragma once
#include "asitic/spiral_construction.hpp"
namespace asitic {
// The unwritten +ec word is observable in the raw copy before a null-target fault.
struct TransformerTerminalHistory { WireConstructionHistory mesh;unsigned next_address=0; };
// Original 0x08059214. Four conductor segments prepend in native emission order.
// Width/spacing and thickness come from arguments; Z uses the selected metal.
void emit_transformer_terminal(GeometryShape& target,const GeometryShape& arguments,double x,double y,double clearance,double direction,int metal,
    const GeometryTechnology& technology,const GeometryRuntime& runtime,const TransformerTerminalHistory& history);
struct Transformer3DHistory {
    WireConstructionHistory winding[2];
    // One mesh history per actual four-segment terminal-helper call. Null
    // selects zeros. A nonnull array must cover calls that fail allocation.
    const TransformerTerminalHistory* terminals;
};
// Original 0x08057d40. Two independently allocated winding shapes, four
// strict turn loops and asymmetric terminal insertion. No final reversal,
// rotation, rebasing or reciprocal partner links are added by this routine.
void build_3d_transformer(GeometryShape*& head,const GeometryShape& primary,const GeometryShape& secondary,
    const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const Transformer3DHistory& history);
}
