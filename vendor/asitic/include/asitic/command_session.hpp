#pragma once
#include "asitic/command_runtime.hpp"
namespace asitic {
// Native 0x08060cd0 places the 300-byte fgets workspace immediately after
// its command records. Keep that layout, including incoming scratch bytes.
struct CommandScriptWorkspace { CommandArguments arguments; char line[300]; };
static_assert(__builtin_offsetof(CommandScriptWorkspace,line)==4160);
struct CommandScriptRuntime {
    CommandRuntime parser;
    void* (*open)(const char*,const char*,void*);
    char* (*read_line)(char*,int,void*,void*);
    int (*close)(void*,void*);
    WarningSink output;
    void (*dispatch)(int,CommandArguments&,void*);
    void* user;
};
// Original 0x08060cd0. Echoes every raw fgets chunk before normalization.
// Only leading-trimmed # lines bypass parsing. Direct dispatch includes blank,
// unknown and exit ids; it does not run outer history, cleanup, redraw or kill.
// The argument scan histories have the same contract as parse_command_arguments.
void execute_command_script(const char* path,CommandScriptWorkspace&,CommandParseHistory*,const CommandScriptRuntime&);
}
