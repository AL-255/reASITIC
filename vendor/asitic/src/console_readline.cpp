#include "asitic/console_readline.hpp"
extern "C" void* dlopen(const char*,int);
extern "C" void* dlsym(void*,const char*);
extern "C" int dlclose(void*);
extern "C" char* dlerror();
extern "C" void free(void*);
namespace asitic {
namespace {
ConsoleReadlineState* active=nullptr;
void error(ConsoleReadlineState& s,const char* text) {
    unsigned i=0;if(text)for(;i+1<sizeof(s.error) && text[i];++i)s.error[i]=text[i];s.error[i]=0;
}
void completed(char*) { auto* s=active;if(s && s->completion)s->completion(s->completion_user); }
char* read(const char* prompt,void*) { return active->read_line(prompt); }
void history(const char* text,void* user) {
    auto& context=*static_cast<ConsoleInputContext*>(user);
    if(context.history)context.history(text,context.user);else active->add_history(text);
}
void install(const char* prompt,void (*callback)(void*),void* context,void*) {
    active->completion=callback;active->completion_user=context;active->installed=1;active->install(prompt,completed);
}
const char* buffer(void*) { return *active->line_buffer; }
void remove(void*) { active->installed=0;active->remove(); }
void character(void*) { active->read_character(); }
void release(void* pointer,void*) { free(pointer); }
}
bool initialize_console_readline(ConsoleReadlineState& s,const char* path) {
    s.error[0]=0;s.library=dlopen(path?path:"libreadline.so.8",2);
    if(!s.library) { error(s,dlerror());return false; }
    s.read_line=reinterpret_cast<char*(*)(const char*)>(dlsym(s.library,"readline"));
    s.add_history=reinterpret_cast<void(*)(const char*)>(dlsym(s.library,"add_history"));
    s.install=reinterpret_cast<void(*)(const char*,void(*)(char*))>(dlsym(s.library,"rl_callback_handler_install"));
    s.remove=reinterpret_cast<void(*)()>(dlsym(s.library,"rl_callback_handler_remove"));
    s.read_character=reinterpret_cast<void(*)()>(dlsym(s.library,"rl_callback_read_char"));
    s.line_buffer=static_cast<char**>(dlsym(s.library,"rl_line_buffer"));
    s.input_stream=static_cast<void**>(dlsym(s.library,"rl_instream"));s.output_stream=static_cast<void**>(dlsym(s.library,"rl_outstream"));
    if(!s.read_line || !s.add_history || !s.install || !s.remove || !s.read_character || !s.line_buffer || !s.input_stream || !s.output_stream) {
        error(s,"GNU readline is missing a required standard ABI symbol.");dlclose(s.library);s.library=nullptr;return false;
    }
    s.completion=nullptr;s.completion_user=nullptr;s.installed=0;return true;
}
void release_console_readline(ConsoleReadlineState& s) {
    if(!s.library)return;
    if(s.installed) { s.installed=0;s.remove(); }
    if(active==&s)active=nullptr;
    dlclose(s.library);s.library=nullptr;
}
ReplInputRuntime libc_console_readline_runtime(ConsoleInputContext& context,ConsoleReadlineState& s) {
    auto r=libc_console_input_runtime(context);active=&s;
    *s.input_stream=r.standard_input;*s.output_stream=r.standard_output;
    r.readline=read;r.release=release;r.history=history;r.install=install;r.readline_buffer=buffer;r.remove=remove;r.read_character=character;
    return r;
}
void console_readline_add_history(const char* text,ConsoleReadlineState& s) { if(s.library)s.add_history(text); }
}
