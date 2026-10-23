#pragma once

namespace asitic {
// Native XERBLA writes directly to stdout, outside the ASITIC log/warning sink.
void console_lapack_argument_error(const char* routine,int argument);

// ASITIC leaves the bundled XERROR print-enable/unit COMMON cells zero and
// exposes no command that changes them. Its callback is therefore silent;
// level 2 alone executes the original zero-status Fortran STOP.
void console_quadpack_diagnostic(void*,const char*,int length,int error,int level);
// libf2c sig_die(message,1): stderr message, restored default SIGABRT, abort.
[[noreturn]] void console_f2c_fatal(const char* message);
// Standalone compatibility with the actual legacy FILE ABI in ASITIC. Uses
// ordinary system libc imports; its old FILE object can fault inside fprintf.
[[noreturn]] void console_f2c_legacy_fatal(const char* message);
}
