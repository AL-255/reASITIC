#pragma once
#include "asitic/network.hpp"
#include "asitic/capacitance.hpp"

namespace asitic {
struct AnalyticalModelOptions {
    const MetalLayer* metals;const ViaLayer* vias;const SubstrateLayer* substrate;
    const double* metal_heights;int metal_count,kind,sides;double spacing,turns,special_gap;
};
// Original frame slots -0x1ac, -0x1a4 and -0x19c. The original may read the
// incoming angular frequency or inductance before writing them; empty shapes
// retain all three. The embedding caller owns the starting bytes.
struct AnalyticalModelHistory { double angular_frequency,resistance,inductance; };
struct AnalyticalModelResult { TwoPortAdmittance admittance;PiEquivalent pi; };
// Numerical path of original 0x0804efb0, with technology/globals/history explicit.
// Every conductor reaches the original FLDCW read at 0xfffffffc and faults.
// Via-only and empty lists complete. The caller may isolate the routine in a
// process to observe the native fault. Verbose formatting belongs to the UI.
bool solve_analytical_model(const ShapeFilamentList& shape,double frequency_hz,const AnalyticalModelOptions& options,
    AnalyticalModelHistory& history,AnalyticalModelResult& result,WarningSink warnings = {});
}
