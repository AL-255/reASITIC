#pragma once
#include "asitic/composite_geometry.hpp"
#include "asitic/technology_text.hpp"
namespace asitic {
// Native command storage: one command/count record, followed by fifteen
// text/value records. Tokenization does not initialize the value words.
struct CommandArgument { char text[256]; unsigned value; };
struct CommandArguments { CommandArgument command,arguments[15]; };
struct CommandDefinition {
    int id; const char* aliases[4]; int minimum,maximum;
    const char* usage; unsigned types[15];
};
struct CommandParseHistory { double number; int integer; unsigned char character; };
struct CommandRuntime {
    CharacterTables characters;
    int (*scan)(const char*,const char*,...);
    char* (*token)(char*,const char*);
    int (*format)(char*,const char*,...);
    char* message; WarningSink warnings;
    GeometryShape* shapes; GeometryShape* selected;
    double frequency_scale,length_scale,grid_size; bool snap;
};
// The 117 original rows, followed by the id=-2 sentinel. Lookup returns a
// table index, uses all four aliases (including empty aliases), and is case
// sensitive. Original 0x0805bcc0.
const CommandDefinition* native_command_catalog();
int lookup_command_index(const char*,const CommandDefinition* catalog=native_command_catalog());
// Original 0x0808769c. Uses the runtime strtok boundary and preserves the
// delimiter-only null-token fault and unbounded command-name copy.
bool tokenize_command(char*,CommandArguments&,int&,const CommandRuntime&);
// Original 0x080871f4. History supplies the native uninitialized scan
// destinations, including their retained value when sscanf fails.
bool parse_command_argument(CommandArgument&,unsigned,const CommandRuntime&,CommandParseHistory&);
// Original 0x08086fc0. -3 invalid/comment/empty, -2 unknown, otherwise the
// native command id (including -1 for exit). Histories has fifteen entries.
int parse_command_arguments(char*,CommandArguments&,const CommandRuntime&,CommandParseHistory* histories,
    const CommandDefinition* catalog=native_command_catalog());
struct CommandDispatchRuntime {
    CommandRuntime parser;
    void (*history)(const char*,void*);
    void (*dispatch)(int,CommandArguments&,void*);
    void (*cleanup)(void*); void (*redraw)(void*);
    void (*signal)(int,int,void*);
    const unsigned char* dragging; void* user;
};
// Original 0x0804c598. Caller supplies the original incoming scratch bytes.
// Dispatch includes negative parse errors; exit signals process group 0/SIGQUIT.
void dispatch_command_line(char*,CommandArguments&,CommandParseHistory*,const CommandDispatchRuntime&);
}
