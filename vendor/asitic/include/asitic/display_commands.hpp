#pragma once
#include "asitic/command_runtime.hpp"
namespace asitic {
struct ViewOptionDisplay {
    const MetalLayer* metals;const int* visible;int metal_count;
    double snap_size,grid_size,view_scale;
    unsigned char snap_enabled,grid_enabled,technology_io;
};
// Original 0x08060704. Invisible-layer entries print the signed pointer value
// to the metal record with "%d ", exactly as the native formatter receives it.
void print_view_options(const ViewOptionDisplay&,const CommandRuntime&);
struct PortOptionDisplay {
    unsigned char verbose,automatic_cells,fast,polar;
    double maximum_length,maximum_width,maximum_thickness,cap_length,cap_width,alpha,beta;
    int maximum_width_cells,eddy,parameter_type;
    char* filename;unsigned filename_length;
};
// Original 0x0806088c. Native %s arguments for flag fields are raw byte values
// cast to pointers; the formatter boundary retains that behavior. Parameter
// descriptions overwrite the Output Style prefix. Filename[length] is cleared
// for nonempty strings although the final format omits a conversion specifier.
void print_port_options(const PortOptionDisplay&,const CommandRuntime&);
// Original 0x08060cc4: calls only print_view_options, not print_port_options.
void print_native_options(const ViewOptionDisplay&,const CommandRuntime&);
struct CommandRecordingRuntime {
    void* output_log;void* input_log;
    int (*print)(void*,const char*,...);
};
// Original 0x08060d6c. This is command recording, despite the old SHOWLDIV
// label. Writes output then input for the command and every text argument;
// no flush or stream-null checks occur at this boundary.
void record_command_transcript(const CommandArguments&,const CommandRecordingRuntime&);
// Native 0x0805bd40 VER208 and OPTIONS219, excluding optional recording.
bool execute_display_command(int,const CommandArguments&,const ViewOptionDisplay&,const CommandRuntime&);
}
