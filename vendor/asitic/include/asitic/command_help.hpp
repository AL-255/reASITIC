#pragma once
#include "asitic/command_runtime.hpp"
namespace asitic {
// Original 0x08061088. Unlike lookup_command_index, returns the native id.
// Missing and QUIT both return -1; matching remains case sensitive.
int lookup_command_id(const char*,const CommandDefinition* catalog=native_command_catalog());
// Original 0x08060df0. Normalizes its copied topic with flags0xae, emits the
// original category lists/usages/details, and retains the native negative-id
// early return (QUIT prints usage only). Message workspace covers1024 bytes.
void print_command_help(const char*,const CommandRuntime&);
// Native 0x0805bd40 case206. Includes the native final empty output message;
// excludes the common optional recording postlude. Returns false for other ids.
bool execute_help_command(int,const CommandArguments&,const CommandRuntime&);
}
