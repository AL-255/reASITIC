#include "asitic/argv_runtime.hpp"
extern "C" int getopt_long(int,char* const*,const char*,const asitic::ArgvLongOption*,int*);
extern "C" char* optarg;
extern "C" int optind;
extern "C" int printf(const char*,...);
extern "C" int fprintf(void*,const char*,...);
extern "C" void* stderr;
extern "C" [[noreturn]] void exit(int);
extern "C" void* realloc(void*,unsigned);
namespace asitic {
namespace {
void replace(char** destination,unsigned,unsigned,const char* text,unsigned length) {
    auto* buffer=static_cast<char*>(realloc(*destination,length+1));
    for(unsigned i=0;i<length;++i)buffer[i]=text[i];
    buffer[length]=0;*destination=buffer;
}
}
ArgvRuntime libc_startup_arguments(WarningSink warnings) {
    return {getopt_long,&optarg,&optind,replace,printf,fprintf,stderr,exit,warnings};
}
}
