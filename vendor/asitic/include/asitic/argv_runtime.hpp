#pragma once
#include "asitic/impedance.hpp"
namespace asitic {
// Original startup-option record. Strings are represented by the original
// four-byte string slots; their allocation/replacement is a runtime boundary.
struct ArgvState {
    unsigned char execute,keyboard,technology,nographics,initialize,readline,log,version;
    char* keyboard_file;char* execute_file;char* technology_file;char* initialize_file;char* log_file;
};
static_assert(sizeof(ArgvState)==28);
struct ArgvLongOption { const char* name;int argument;int* flag;int value; };
struct ArgvRuntime {
    int (*next_option)(int,char* const*,const char*,const ArgvLongOption*,int*);
    char** option_argument;int* option_index;
    void (*replace)(char**,unsigned,unsigned,const char*,unsigned);
    int (*print)(const char*,...);int (*error_print)(void*,const char*,...);
    void* error_file;void (*terminate)(int);
    WarningSink warnings;
};
// Original 0x08091d48: warns and clears invalid exec/keyboard flags. Strings
// and other bytes remain unchanged. Warning callbacks run before flag writes.
void validate_startup_arguments(ArgvState&,WarningSink warnings={});
// Original 0x08091aa0. Uses the native GNU getopt_long option table and short
// string. On completion, remaining argv entries move to the start of argv;
// argc and the old tail/terminator are retained. The runtime terminate callback
// must not return. GNU getopt state (including optind=0 resets) remains explicit.
void parse_startup_arguments(int argc,char** argv,ArgvState&,const ArgvRuntime&);
const ArgvLongOption* native_startup_options();
const char* native_startup_usage();
// libc adapter: uses GNU getopt_long and malloc-owned C strings. Initialize
// string slots to null or malloc-owned buffers; free them in the host session.
// The host supplies the warning sink used for the three validation messages.
ArgvRuntime libc_startup_arguments(WarningSink warnings={});
}
