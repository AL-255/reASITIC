#pragma once
#include "asitic/argv_runtime.hpp"
#include "asitic/command_runtime.hpp"
#include "asitic/repl_input.hpp"

namespace asitic {
// Stream identities remain explicit, including the two optional recording
// streams, which are distinct from the always-open session transcripts.
struct ConsoleOutputState {
    void* standard_output;
    void* standard_error;
    void* input_log;
    void* output_log;
    void* recording_input;
    void* recording_output;
    unsigned char recording;
    char* message; // shared native 1024-byte line/message workspace
};
struct ConsoleLogPaths { char* input;char* output; }; // native 96-byte buffers
struct ConsoleRuntime {
    CharacterTables characters;
    int (*format)(char*,const char*,...);
    int (*print)(void*,const char*,...);
    int (*flush)(void*,void*);
    void* (*open)(const char*,const char*,void*);
    int (*close)(void*,void*);
    int (*time)(int*,void*);
    char* (*calendar_time)(const int*,void*);
    void (*destroy_ui)(void*);
    void (*release_green_cache)(void*);
    // Native exit boundary. Must not return.
    void (*terminate)(int,void*);
    void* user;
};
// Originals 0x0804c070, 0x0804bf94 and 0x0804bf74. The error helper adds
// only newlines; callers supply any Error/Warning prefix themselves.
void console_error(const char*,unsigned char to_log,ConsoleOutputState&,const ConsoleRuntime&);
void console_output(const char*,ConsoleOutputState&,const ConsoleRuntime&);
void console_status(const char*,ConsoleOutputState&,const ConsoleRuntime&);
// Originals 0x0804be20, 0x0804c33c and 0x0804beb8. Closure is unconditional,
// retains stale FILE pointers, and preserves callback-visible teardown order.
void close_console_logs(ConsoleOutputState&,const ConsoleRuntime&);
void shutdown_console(ConsoleOutputState&,const ConsoleRuntime&);
void fatal_console(const char*,ConsoleOutputState&,const ConsoleRuntime&);
// Original 0x0804b8ec. time/ctime use the native signed 32-bit time_t boundary.
// Header writes are not explicitly flushed. Failed opens enter fatal teardown
// with the partially opened handles, including the original null-stream fault.
void open_console_logs(const ConsoleLogPaths&,ConsoleOutputState&,const ConsoleRuntime&);
// Original 0x0804b638. A case-sensitive .LOG substring anywhere in a supplied
// basename makes BOTH paths that basename (the original overwrites its i_).
void initialize_console_logs(const ArgvState&,ConsoleLogPaths&,ConsoleOutputState&,const ConsoleRuntime&);
// Original 0x0804c1dc. Prints the original version/banner; version flag exits0.
void initialize_console_banner(const ArgvState&,ConsoleOutputState&,const ConsoleRuntime&);
// Original 0x0804c198. One enum represents the selected prompted/unprompted
// native function pair; the announcement precedes the readline selection.
void select_console_acquisition(unsigned char readline,ReplAcquisition&,ConsoleOutputState&,const ConsoleRuntime&);
// Original 0x0804b85c. Keyboard redirect does not replace the stored script
// path, alter delay, or close a previous script stream.
void initialize_console_keyboard(const ArgvState&,ReplInputState&,ConsoleOutputState&,const ConsoleRuntime&);
// Storage/default initialization for an ordinary source-only console session.
// The two filenames and cleared script/mode/recording state are native ELF
// initial data. Call before attaching opened streams or parsed startup options.
struct ConsoleSession {
    ReplInputState input;
    ConsoleOutputState output;
    char input_log_path[96],output_log_path[96],script_path[80],line[1024];
};
void initialize_console_session(ConsoleSession&);

// Ordinary libc bindings are in console_runtime_libc.cpp. UI/cache ownership
// stays with the embedding host; both lifecycle callbacks are required.
struct ConsoleLifecycle {
    void (*destroy_ui)(void*);
    void (*release_green_cache)(void*);
    void* user;
};
ConsoleRuntime libc_console_runtime(ConsoleLifecycle);
void attach_libc_console_streams(ConsoleOutputState&);
// Replaces only character tables and sscanf/strtok/sprintf. All session fields
// (message, warning sink, shapes, scales, grid and snap) are retained.
CommandRuntime libc_console_command_runtime(CommandRuntime);
struct ConsoleInputContext {
    ConsoleOutputState* output;
    const ConsoleRuntime* runtime;
    void (*history)(const char*,void*);
    void (*cancel)(void*); // must not return
    void* user;
};
// Binds the actual stdin/stdout, fgets/feof/select/sleep and output helpers.
// Use PromptStdio acquisition and input.dragging=1 for a headless session.
// Readline/X event callbacks remain null; this adapter does not emulate them.
// Construct again after opening/replacing transcript streams.
ReplInputRuntime libc_console_input_runtime(ConsoleInputContext&);
}
