#include "asitic/console_diagnostics.hpp"
extern "C" int printf(const char*,...);
extern "C" int fprintf(void*,const char*,...);
extern "C" int fflush(void*);
extern "C" void* stderr;
extern "C" void (*signal(int,void(*)(int)))(int);
extern "C" [[noreturn]] void abort();
extern "C" [[noreturn]] void exit(int);
extern "C" void _IO_init(void*,int);
extern "C" unsigned char asitic_legacy_stderr[80];
asm(".symver asitic_legacy_stderr,_IO_stderr_@GLIBC_2.0");
// The original advertises the newer stdio ABI while also importing the old
// 80-byte stderr object. Keep the same startup marker for this mixed ABI.
extern "C" const unsigned _IO_stdin_used=0x20001u;

namespace asitic {
void console_lapack_argument_error(const char* routine,int argument) {
    ::printf("** On entry to %6s, parameter number %2i had an illegal value\n",routine,argument);
}
void console_quadpack_diagnostic(void*,const char*,int,int,int level) {
    if(level==2)::exit(0);
}
[[noreturn]] void console_f2c_fatal(const char* message) {
    ::fprintf(::stderr,"%s\n",message);::fflush(::stderr);
    // ASITIC has no open Fortran units at this boundary. Its ordinary files
    // must not run the application's graceful teardown on a libf2c abort.
    ::fflush(::stderr);::signal(6,nullptr);::abort();
}
[[noreturn]] void console_f2c_legacy_fatal(const char* message) {
    // The original C++ runtime supplies foreign FILE objects to libc. Recreate
    // its permissive vtable state through ordinary libc _IO_init, whose pinned
    // i386 implementation enables that compatibility as it initializes a FILE.
    alignas(16) unsigned char stream[152]{};::_IO_init(stream,0);
    ::fprintf(asitic_legacy_stderr,"%s\n",message);::fflush(asitic_legacy_stderr);
    ::fflush(asitic_legacy_stderr);::signal(6,nullptr);::abort();
}
}
