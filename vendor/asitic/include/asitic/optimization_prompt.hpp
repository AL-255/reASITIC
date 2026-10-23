#pragma once
#include "asitic/optimization.hpp"
namespace asitic {
struct OptimizationPromptRuntime {
    ShapePromptRuntime shape;
    // Original read_command_line acquisition boundary, always history=false.
    // Cancellation propagates through this callback without committing globals.
    void (*read)(const char*,char*,void*);
    void* user;
};
// Native frame-local values are incoming history until a scan assigns them.
// Layout is the original contiguous frame region -0xa8..-1.
struct OptimizationPromptHistory {
    double frequency,width_min,width_increment,width_max;
    double spacing_min,spacing_increment,spacing_max;
    double length,length_multiplier,target,accuracy;
    char name[80];
    int layer;
};
// Original 0x08070034. Repeats individual prompts according to native scan
// counts and ordered comparisons, then commits all optimizer state at once.
// The shared minimum_quality/absolute_tolerance slots hold the two dimensions
// in OPTL mode; inductance_min/inductance_max hold target nH/accuracy percent.
void prompt_optimization_target(OptimizationSweepState&,const OptimizationPromptRuntime&,
    OptimizationPromptHistory&);
}
