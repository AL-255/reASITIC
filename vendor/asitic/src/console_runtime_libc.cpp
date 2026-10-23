#include "asitic/console_runtime.hpp"
// The supported build is i386; use its ordinary libc ABI without requiring
// architecture-specific C/C++ development headers in the build container.
extern "C" int vfprintf(void*,const char*,__builtin_va_list);
extern "C" int sprintf(char*,const char*,...);
extern "C" int sscanf(const char*,const char*,...);
extern "C" char* strtok(char*,const char*);
extern "C" int fflush(void*);
extern "C" void* fopen(const char*,const char*);
extern "C" int fclose(void*);
extern "C" int time(int*);
extern "C" char* ctime(const int*);
extern "C" [[noreturn]] void exit(int);
extern "C" char* fgets(char*,int,void*);
extern "C" int feof(void*);
extern "C" int getc(void*);
extern "C" int select(int,void*,void*,void*,asitic::ReplTimeval*);
extern "C" unsigned sleep(unsigned);
extern "C" void* stdin;
extern "C" void* stdout;
extern "C" void* stderr;

namespace asitic {
namespace {
int print(void* file,const char* format,...) {
    __builtin_va_list args;__builtin_va_start(args,format);const int result=::vfprintf(file,format,args);__builtin_va_end(args);return result;
}
int flush(void* file,void*) { return ::fflush(file); }
void* open(const char* path,const char* mode,void*) { return ::fopen(path,mode); }
int close(void* file,void*) { return ::fclose(file); }
int now(int* out,void*) { return ::time(out); }
char* calendar(const int* time,void*) { return ::ctime(time); }
void terminate(int code,void*) { ::exit(code); }
char* read(char* out,int capacity,void* file,void*) { return ::fgets(out,capacity,file); }
int eof(void* file,void*) { return ::feof(file); }
int get(void* file,void*) { return ::getc(file); }
int poll(int count,unsigned* reads,void* writes,void* excepts,ReplTimeval* timeout,void*) {
    return ::select(count,reads,writes,excepts,timeout);
}
void sleep(unsigned seconds,void*) { ::sleep(seconds); }
void echo(const char* message,unsigned short flags,void* user) {
    auto& context=*static_cast<ConsoleInputContext*>(user);console_error(message,static_cast<unsigned char>(flags),*context.output,*context.runtime);
}
void history(const char* line,void* user) { auto& c=*static_cast<ConsoleInputContext*>(user);c.history(line,c.user); }
void cancel(void* user) { auto& c=*static_cast<ConsoleInputContext*>(user);c.cancel(c.user); }
}
ConsoleRuntime libc_console_runtime(ConsoleLifecycle lifecycle) {
    return {libc_technology_text().characters,::sprintf,print,flush,open,close,now,calendar,
        lifecycle.destroy_ui,lifecycle.release_green_cache,terminate,lifecycle.user};
}
void attach_libc_console_streams(ConsoleOutputState& state) { state.standard_output=stdout;state.standard_error=stderr; }
CommandRuntime libc_console_command_runtime(CommandRuntime r) {
    r.characters=libc_technology_text().characters;r.scan=::sscanf;r.token=::strtok;r.format=::sprintf;return r;
}
ReplInputRuntime libc_console_input_runtime(ConsoleInputContext& context) {
    const auto& s=*context.output;
    return {libc_technology_text().characters,stdin,s.standard_output,s.input_log,s.output_log,s.recording_input,
        read,eof,close,flush,get,nullptr,nullptr,history,::sprintf,print,echo,poll,nullptr,nullptr,nullptr,nullptr,nullptr,
        sleep,cancel,&context};
}
}
