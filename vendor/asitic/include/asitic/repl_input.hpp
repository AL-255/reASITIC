#pragma once
#include "asitic/technology_text.hpp"

namespace asitic {
// g_use_readline is actually the script-active byte in the original program.
struct ReplInputState {
    unsigned char scripted,history_enabled,dragging,abort_pending,line_pending,prompt_echo;
    int script_delay;
    void* script_file;
    char* script_path; // native fixed character buffer; EOF clears only byte zero
    char* line_buffer; // native shared 1024-byte buffer
};
struct ReplTimeval { int seconds,microseconds; };
struct ReplInputRuntime {
    CharacterTables characters;
    void* standard_input;
    void* standard_output;
    void* input_log;
    void* output_log;
    void* prompt_log;
    char* (*read_line)(char*,int,void*,void*);
    int (*eof)(void*,void*);
    int (*close)(void*,void*);
    int (*flush)(void*,void*);
    int (*get_character)(void*,void*);
    char* (*readline)(const char*,void*);
    void (*release)(void*,void*);
    void (*history)(const char*,void*);
    int (*format)(char*,const char*,...);
    int (*print)(void*,const char*,...);
    void (*echo)(const char*,unsigned short,void*);
    int (*select)(int,unsigned*,void*,void*,ReplTimeval*,void*);
    void (*event_loop)(void*);
    void (*install)(const char*,void (*)(void*),void*,void*);
    const char* (*readline_buffer)(void*);
    void (*remove)(void*);
    void (*read_character)(void*);
    void (*sleep)(unsigned,void*);
    // Native CancelCreate throw boundary. This callback must not return.
    void (*cancel)(void*);
    void* user;
    // Optional guarded UI boundaries: true means normal return, false means
    // a caught callback exception. The native pump removes the handler,
    // reports the exception and resumes with the existing shared line.
    bool (*guarded_event_loop)(void*)=nullptr;
    bool (*guarded_read_character)(void*)=nullptr;
};
enum class ReplAcquisition { CallbackReadline,PromptStdio };
// Original 0x0804bdc4: select(1,stdin-set,null,null,{0,100}); even -1 is true.
bool repl_stdin_has_input(const ReplInputRuntime& runtime);
// Original 0x0804be3c: logging occurs before normalization, with precisely the
// native newlines and separate flushes for the two log streams.
void repl_log_input(const char* line,const char* prompt,const ReplInputState& state,const ReplInputRuntime& runtime);
// Original 0x0804bffc: leading-newline console prompt and optional log echoes.
void repl_print_prompt(const char* prompt,unsigned char output_log,const ReplInputState& state,const ReplInputRuntime& runtime);
// Original 0x0804c480. Ignores readline's callback argument, copies its shared
// line buffer, clears pending and removes the callback handler in that order.
int repl_readline_eol(ReplInputState& state,const ReplInputRuntime& runtime);
// Original 0x0804c4a4: script fgets/feof, echo/history and EOF teardown only.
void repl_read_script_line(const char* prompt,char* out,unsigned char history,ReplInputState& state,const ReplInputRuntime& runtime);
// Original 0x0804b1c4, 0x0804b3dc, 0x0804b4fc, 0x0804c378 respectively.
// Buffers must have native capacity (1024 plus prompt/newline concatenations).
// As in the binary, failed reads retain existing bytes except script reads
// clear out[0] first. A null blocking-readline result is not special-cased.
void repl_read_callback(const char* prompt,char* out,unsigned char history,ReplInputState& state,const ReplInputRuntime& runtime);
void repl_read_unprompted(char* out,unsigned char history,ReplInputState& state,const ReplInputRuntime& runtime);
void repl_read_stdio(const char* prompt,char* out,unsigned char history,ReplInputState& state,const ReplInputRuntime& runtime);
void repl_read_stdio_unprompted(char* out,unsigned char history,ReplInputState& state,const ReplInputRuntime& runtime);
// Original 0x0804b0d8. Acquires a line, throws through cancel on exact uppercase
// QUIT or pending abort, then applies scripted pause/positive-second delay.
void repl_read_command_line(const char* prompt,char* out,unsigned char history,ReplAcquisition acquisition,ReplInputState& state,const ReplInputRuntime& runtime);
// Original 0x0804c570: command-line wrapper with history=0, then flags0xae.
void repl_prompt_and_normalize(const char* prompt,char* out,ReplAcquisition acquisition,ReplInputState& state,const ReplInputRuntime& runtime);
}
