#pragma once
#include "asitic/shape_prompts.hpp"
namespace asitic {
// Original contiguous optimizer globals, 0x080d86e0..0x080d880f. Fields not
// written by an argument remain byte-for-byte incoming state, even on failure.
struct OptimizationSweepState {
    char shape_name[80],output_filename[80];
    double spacing_min,spacing_increment,spacing_max;
    double width_min,width_increment,width_max;
    double area_min,area_increment,area_max;
    double minimum_quality,absolute_tolerance;
    double inductance_min,inductance_max;
    unsigned metric_kind,metric_terminal;
    int metal,exit_metal,sides,simulation_type;
    unsigned frequency_prefix;
    double frequency;
    GeometryShape* ground;
};
static_assert(sizeof(OptimizationSweepState)==304);
static_assert(__builtin_offsetof(OptimizationSweepState,frequency)==292);
struct OptimizationSweepHistory {
    CommandArgument temporary;
    // One native scanner-frame history per keyword query, in source order.
    // Intervening runtime calls may change unwritten stack bytes between them.
    CommandParseHistory scan[20];
    // METAL and EXIT each invoke the separate native layer-lookup frame.
    int layer[2];
};
// Original 0x080847ec: parses SWEEP/SWEEPMM keyword arguments, validates all
// supplied ranges and assigns optimizer globals. The historical export calls
// this a sweep runner, but this entry performs no search or simulation.
// Invalid input retains earlier assignments and invokes runtime.invalid only
// after all fields have been visited. That callback must not return.
void parse_optimization_sweep(char*,OptimizationSweepState&,
    const ShapePromptRuntime&,OptimizationSweepHistory&);
}
