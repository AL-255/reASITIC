#pragma once
#include "asitic/command_runtime.hpp"
namespace asitic {
// Remaining session globals consumed by the technology/grid command cases.
// Bytes are intentionally not bool: native toggling and assignment preserve
// arbitrary incoming low-byte values where specified by the original.
struct TechnologyCommandSettings {
    double grid_size,snap_size;
    double auto_alpha,auto_beta;
    unsigned char grid_enabled,snap_enabled,automatic_cells,technology_io,dirty;
};
// Original 0x08074fd0. Prints the dense tables in native metal/substrate/via
// order, including the original "Technolog" spelling. Numeric arguments are
// transferred as raw binary64 words to the runtime formatter.
void print_technology(const TechnologyInitialization&,const TechnologyRuntime&);
// Cases of original 0x0805bd40: GRID108, SNAP112, CELL207, CCELL211,
// AUTOCELL212, PRINTTEKFILE214, MODIFYTECHLAYER222 and TECHIO223.
// Returns true only for a handled id. Arguments have already passed the native
// parser, with float/int/byte values in CommandArgument::value. Geometry changes
// propagate cumulative substrate heights, metal Z, depth and metal-height banks.
// Failure retains preceding mutations; SNAP enables before validating its size.
// This boundary excludes the outer dispatcher's optional script recording.
bool execute_technology_command(int,const CommandArguments&,TechnologyInitialization&,
    TechnologyCommandSettings&,const TechnologyRuntime&);
}
