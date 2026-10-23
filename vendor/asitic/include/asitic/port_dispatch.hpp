#pragma once
#include "asitic/command_runtime.hpp"
#include "asitic/port_commands.hpp"
namespace asitic {
struct PortDispatchRuntime {
    PortFormatRuntime formats;
    // Models the native basic_string::replace service. It must update the
    // supplied filename view, including its data pointer when storage moves.
    void (*replace_filename)(PortSweepFilename&,unsigned position,unsigned remove,
        const char*,unsigned length,void*);
    void (*sweep)(PortSweepKind,const GeometryShape* const*,double,const PortSweepSettings&,
        const PortSweepRuntime&)=run_port_frequency_sweep;
    PortSweepRuntime sweep_runtime;
    void* user;
};
// Actual native 0x0805bd40 cases 2PORTTRANS524, 2PORTGND529,
// 2PORTPAD530, 3PORT536 and 2PORTX539. Arguments are already parsed;
// text format/mode fields are normalized in place. The enclosing dispatcher
// owns the optional script-recording postlude. Returns false for other ids.
// Native defaults and backed reads are retained: TRANS clamps a reversed
// range while other commands swap it; 3PORT with exactly five arguments still
// reads the sixth value; omitted PAD parameter format and some polar settings
// retain their incoming state. No interactive acquisition occurs here.
bool execute_port_command(int,CommandArguments&,PortSweepSettings&,const PortDispatchRuntime&);
}
