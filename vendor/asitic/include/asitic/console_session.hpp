#pragma once
namespace asitic {
struct ConsoleSessionPolicy {
    // Explicit embedding policy only. The ordinary executable retains the
    // native failed-stdin-read loop by default; it does not repair that bug.
    bool terminate_at_stdin_eof=false;
    // Native EXIT sends SIGQUIT to the foreground process group. An
    // embedding may explicitly request private cleanup instead.
    bool signal_process_group=true;
};
// Ordinary libc application controller with optional X11 and GNU readline. The reconstructed component
// routines remain independent of the executable and its lifecycle boundaries.
int run_console_session(int argc,char** argv,ConsoleSessionPolicy policy={});
}
