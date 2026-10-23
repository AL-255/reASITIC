#include "asitic/repl_input.hpp"

namespace asitic {
namespace {
void copy(char* out,const char* in) { while((*out++=*in++)) {} }
void append(char* out,const char* in) { while(*out)++out;copy(out,in); }
void finish(const char* prompt,char* out,ReplInputState& s,const ReplInputRuntime& r) {
    repl_log_input(out,prompt,s,r);normalize_input_line(out,0x8e,r.characters);
}
struct EolContext { ReplInputState* state;const ReplInputRuntime* runtime; };
void eol(void* context) {
    auto* c=static_cast<EolContext*>(context);repl_readline_eol(*c->state,*c->runtime);
}
}
bool repl_stdin_has_input(const ReplInputRuntime& r) {
    unsigned set[32];for(unsigned& word:set)word=0;set[0]|=1;
    ReplTimeval timeout{0,100};return r.select(1,set,nullptr,nullptr,&timeout,r.user)!=0;
}
void repl_log_input(const char* line,const char* prompt,const ReplInputState& s,const ReplInputRuntime& r) {
    if(!s.history_enabled) {
        r.print(r.input_log,"%s",line);r.flush(r.input_log,r.user);
        r.print(r.output_log,"%s%s",prompt,line);
    } else {
        r.print(r.input_log,"%s\n",line);r.flush(r.input_log,r.user);
        r.print(r.output_log,"%s%s\n\n",prompt,line);
    }
    r.flush(r.output_log,r.user);
}
void repl_print_prompt(const char* prompt,unsigned char output_log,const ReplInputState& s,const ReplInputRuntime& r) {
    r.print(r.standard_output,"\n%s",prompt);r.flush(r.standard_output,r.user);
    if(output_log) { r.print(r.output_log,"\n%s",prompt);r.flush(r.output_log,r.user); }
    if(s.prompt_echo) { r.print(r.prompt_log,"\n%s",prompt);r.flush(r.prompt_log,r.user); }
}
int repl_readline_eol(ReplInputState& s,const ReplInputRuntime& r) {
    s.line_pending=0;copy(s.line_buffer,r.readline_buffer(r.user));r.remove(r.user);return 0;
}
void repl_read_script_line(const char* prompt,char* out,unsigned char history,ReplInputState& s,const ReplInputRuntime& r) {
    out[0]=0;r.read_line(out,1024,s.script_file,r.user);
    if(!r.eof(s.script_file,r.user)) {
        char echo[1024];r.format(echo,"\n%s%s",prompt,out);r.echo(echo,1,r.user);
        if(s.history_enabled && echo[0] && history)r.history(echo,r.user);
    } else {
        s.scripted=0;s.script_delay=0;r.close(s.script_file,r.user);s.script_path[0]=0;
    }
}
void repl_read_callback(const char* prompt,char* out,unsigned char history,ReplInputState& s,const ReplInputRuntime& r) {
    if(s.scripted)repl_read_script_line(prompt,out,history,s,r);
    else {
        s.line_pending=1;EolContext context{&s,&r};
        if(prompt) { r.format(s.line_buffer,"\n%s",prompt);r.install(s.line_buffer,eol,&context,r.user); }
        else r.install("",eol,&context,r.user);
        bool interrupted=false;
        while(s.line_pending) {
            if(!s.dragging) {
                if(r.guarded_event_loop) { if(!r.guarded_event_loop(r.user)) { interrupted=true;break; } }
                else r.event_loop(r.user);
            }
            if(repl_stdin_has_input(r)) {
                if(!s.line_pending)break;
                if(r.guarded_read_character) { if(!r.guarded_read_character(r.user)) { interrupted=true;break; } }
                else r.read_character(r.user);
            }
        }
        if(interrupted) { r.remove(r.user);r.echo("Exiting current command due to exception.",1,r.user); }
        if(s.line_buffer[0] && history)r.history(s.line_buffer,r.user);
        copy(out,s.line_buffer);
    }
    finish(prompt,out,s,r);
}
void repl_read_unprompted(char* out,unsigned char history,ReplInputState& s,const ReplInputRuntime& r) {
    if(s.scripted)repl_read_script_line("",out,history,s,r);
    else {
        char* line=r.readline("",r.user);
        if(line && line[0] && history)r.history(line,r.user);
        copy(out,line);r.release(line,r.user);
    }
    finish("",out,s,r);
}
void repl_read_stdio(const char* prompt,char* out,unsigned char history,ReplInputState& s,const ReplInputRuntime& r) {
    if(s.scripted)repl_read_script_line(prompt,out,history,s,r);
    else {
        repl_print_prompt(prompt,0,s,r);
        while(!repl_stdin_has_input(r) && !s.dragging)r.event_loop(r.user);
        r.flush(r.standard_input,r.user);r.read_line(out,1024,r.standard_input,r.user);
    }
    append(out,"\n");finish(prompt,out,s,r);
}
void repl_read_stdio_unprompted(char* out,unsigned char history,ReplInputState& s,const ReplInputRuntime& r) {
    if(s.scripted)repl_read_script_line("",out,history,s,r);
    else { r.flush(r.standard_input,r.user);r.read_line(out,1024,r.standard_input,r.user); }
    finish("",out,s,r);
}
void repl_read_command_line(const char* prompt,char* out,unsigned char history,ReplAcquisition acquisition,ReplInputState& s,const ReplInputRuntime& r) {
    if(acquisition==ReplAcquisition::CallbackReadline)repl_read_callback(prompt,out,history,s,r);
    else repl_read_stdio(prompt,out,history,s,r);
    const char quit[]="QUIT";unsigned i=0;while(i<5 && out[i]==quit[i])++i;
    if(i==5 || s.abort_pending) { s.abort_pending=0;r.cancel(r.user);return; }
    if(s.script_delay<0 && s.scripted) { r.print(r.standard_output,"\nPress any key to continue:");r.get_character(r.standard_input,r.user); }
    else if(s.script_delay!=0 && s.scripted)r.sleep(static_cast<unsigned>(s.script_delay),r.user);
}
void repl_prompt_and_normalize(const char* prompt,char* out,ReplAcquisition acquisition,ReplInputState& s,const ReplInputRuntime& r) {
    repl_read_command_line(prompt,out,0,acquisition,s,r);normalize_input_line(out,0xae,r.characters);
}
}
