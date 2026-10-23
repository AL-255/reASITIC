#pragma once
#include "asitic/combined_network.hpp"
#include "asitic/composite_geometry.hpp"
namespace asitic {
struct PadDeembeddingOptions {
    GeometryTechnology technology;
    GeometryCommandRuntime commands;
    CombinedNetworkOptions combined;
    bool print;
    // The second contact-builder invocation has its own incoming stack word.
    PotentialContactHistory pad_contact_history = {};
};
// PI4, original 0x08050d10. Solve the device with its two pads, then remove
// the pads' capacitive contribution and extract the resulting pi model.
// Both inner return bytes are ignored by the native caller. A failed pad
// capacitance stage therefore retains the original subsequent memory fault.
bool solve_pad_deembedded_network(const GeometryShape& device,const GeometryShape& first_pad,
    const GeometryShape& second_pad,const GeometryShape& reference,double frequency,
    CombinedNetworkState& state,const PadDeembeddingOptions& options);
}
