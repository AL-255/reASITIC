#pragma once
#include "asitic/console_runtime.hpp"
namespace asitic {
// GNU readline is process-global. One initialized state may be bound at a
// time; its storage must outlive every returned ReplInputRuntime callback.
// This adapter loads modern libreadline.so.8 and never uses the legacy bundle.
struct ConsoleReadlineState {
    void* library;
    char* (*read_line)(const char*);
    void (*add_history)(const char*);
    void (*install)(const char*,void (*)(char*));
    void (*remove)();void (*read_character)();
    char** line_buffer;
    void** input_stream;void** output_stream;
    void (*completion)(void*);void* completion_user;
    unsigned char installed;
    char error[256];
};
// State must initially be zeroed. An optional explicit library path is useful
// for packaged ordinary runtimes and tests; null selects libreadline.so.8.
bool initialize_console_readline(ConsoleReadlineState&,const char* library=nullptr);
void release_console_readline(ConsoleReadlineState&);
// Binds actual GNU blocking/callback input and history, while preserving
// libc_console_input_runtime's streams, logging, polling and cancel services.
// Native EOL deliberately ignores GNU's allocated callback argument and reads
// rl_line_buffer before removing the handler. EOF follows that same boundary.
ReplInputRuntime libc_console_readline_runtime(ConsoleInputContext&,ConsoleReadlineState&);
// If context.history is supplied, acquisition forwards to it; otherwise GNU
// add_history is used directly. Hosts may forward every history callback here,
// including the outer dispatcher's separate native add_history call. No-op
// before initialization permits the same host callback in stdio mode.
void console_readline_add_history(const char*,ConsoleReadlineState&);
}
