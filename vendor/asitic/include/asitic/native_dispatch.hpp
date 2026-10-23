#pragma once
#include "asitic/command_runtime.hpp"
namespace asitic {
struct NativeDispatchState {
    double pan_x,pan_y,chip_width,chip_height;
    float origin3d[3],scale3d[3],angle3d[3];
    int maximum_width_cells,eddy;
    unsigned char show_dimensions,verbose,timing,dump_matrices,opengl;
};
// Concrete cases of native 0x0805bd40: HPAN100, VPAN101, PANOUT105,
// PAN106, ORIGIN107, SHOWDIR113, ORIGIN3D114, SCALE3D115, ROTATE3D116,
// CHIP217, MAXNW218, VERBOSE220, EDDY221, TIME228, SAVEMAT229, OPENGL230.
// Native no-ops -3/REFRESH103/SHOWLDIV800 and unknown/exit -2/-1 are included.
// Returns false for ids handled by other command modules; no state/output
// changes occur on that path. Optional suppress_recording reports the native
// local error flag. The outer optional script-recording postlude is excluded.
bool execute_native_command(int,const CommandArguments&,NativeDispatchState&,const CommandRuntime&,
    bool* suppress_recording=nullptr);
}
