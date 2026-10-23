#include "asitic/console_session.hpp"
extern "C" int asitic_console_main(int argc,char** argv,char**) { return asitic::run_console_session(argc,argv); }
// The repository deliberately does not require i386 CRT development objects.
// libc still owns process initialization, TLS, stdio and ordinary termination.
asm(".text\n.global _start\n_start:\n"
    "xorl %ebp,%ebp\npopl %esi\nmovl %esp,%ecx\nandl $-16,%esp\n"
    "pushl %eax\npushl %esp\npushl %edx\npushl $0\npushl $0\npushl %ecx\npushl %esi\npushl $asitic_console_main\n"
    "call __libc_start_main\nhlt\n");
