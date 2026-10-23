#include "asitic/console_runtime.hpp"

namespace asitic {
namespace {
void copy(char* out,const char* in) { while((*out++=*in++)) {} }
void append(char* out,const char* in) { while(*out)++out;copy(out,in); }
const char* value(const char* p) { return p?p:""; }
bool contains_log(const char* p) {
    for(;*p;++p)if(p[0]=='.' && p[1]=='L' && p[2]=='O' && p[3]=='G')return true;
    return false;
}
void terminate_session(ConsoleOutputState& s,const ConsoleRuntime& r) {
    r.destroy_ui(r.user);r.release_green_cache(r.user);close_console_logs(s,r);
    if(s.recording) { r.close(s.recording_output,r.user);r.close(s.recording_input,r.user); }
    r.terminate(0,r.user);
}
}
void console_error(const char* message,unsigned char to_log,ConsoleOutputState& s,const ConsoleRuntime& r) {
    r.print(s.standard_output,"\n%s",message);r.flush(s.standard_output,r.user);
    if(to_log) { r.print(s.output_log,"\n%s\n",message);r.flush(s.output_log,r.user); }
    if(s.recording) { r.print(s.recording_input,"\n%s\n",message);r.flush(s.recording_input,r.user); }
}
void console_output(const char* message,ConsoleOutputState& s,const ConsoleRuntime& r) {
    r.print(s.standard_output,"%s",message);r.flush(s.standard_output,r.user);
    r.print(s.output_log,"%s",message);r.flush(s.output_log,r.user);
    if(s.recording) { r.print(s.recording_input,"%s",message);r.flush(s.recording_input,r.user); }
}
void console_status(const char* message,ConsoleOutputState& s,const ConsoleRuntime& r) {
    r.print(s.standard_output,"\r%s",message);r.flush(s.standard_output,r.user);
}
void close_console_logs(ConsoleOutputState& s,const ConsoleRuntime& r) {
    r.close(s.input_log,r.user);r.close(s.output_log,r.user);
}
void shutdown_console(ConsoleOutputState& s,const ConsoleRuntime& r) { terminate_session(s,r); }
void fatal_console(const char* message,ConsoleOutputState& s,const ConsoleRuntime& r) {
    r.format(s.message,"\nFatal Error:  %s",message);
    console_error(s.message,1,s,r);terminate_session(s,r);
}
void open_console_logs(const ConsoleLogPaths& paths,ConsoleOutputState& s,const ConsoleRuntime& r) {
    int now;r.time(&now,r.user);
    s.input_log=r.open(paths.input,"a",r.user);s.output_log=r.open(paths.output,"a",r.user);
    if(s.input_log && s.output_log) {
        r.print(s.input_log,"\n#################################################");
        r.print(s.input_log,"\n# ASITIC ver %s INPUT LOG File","10.20.99.0.0.0");
        char* date=r.calendar_time(&now,r.user);normalize_input_line(date,0x80,r.characters);
        r.print(s.input_log,"\n# Generated on %s",date);
        r.print(s.input_log,"\n#################################################\n");
        r.print(s.output_log,"\n################################################");
        r.print(s.output_log,"\n# ASITIC ver %s INPUT/OUTPUT LOG File","10.20.99.0.0.0");
        date=r.calendar_time(&now,r.user);normalize_input_line(date,0x80,r.characters);
        r.print(s.output_log,"\n# Generated on %s",date);
        r.print(s.output_log,"\n################################################\n");
    } else fatal_console("Unable to open log files.",s,r);
}
void initialize_console_logs(const ArgvState& options,ConsoleLogPaths& paths,ConsoleOutputState& s,const ConsoleRuntime& r) {
    if(options.log) {
        const char* base=value(options.log_file);
        if(contains_log(base)) { copy(paths.input,"i_");copy(paths.input,base);copy(paths.output,base); }
        else { copy(paths.input,base);append(paths.input,"_i.LOG");copy(paths.output,base);append(paths.output,".LOG"); }
        r.print(s.standard_output,"\nOpening output log file <%s>...",base);
    }
    open_console_logs(paths,s,r);
}
void initialize_console_banner(const ArgvState& options,ConsoleOutputState& s,const ConsoleRuntime& r) {
    r.format(s.message,"ASITIC version %s","10.20.99.0.0.0");console_error(s.message,0,s,r);
    console_error("Analysis & Simulation of Inductors and Transformers for ICs",0,s,r);
    if(options.version)r.terminate(0,r.user);
}
void select_console_acquisition(unsigned char readline,ReplAcquisition& acquisition,ConsoleOutputState& s,const ConsoleRuntime& r) {
    if(readline) { r.print(s.standard_output,"\nUsing readline library for input...");acquisition=ReplAcquisition::CallbackReadline; }
    else acquisition=ReplAcquisition::PromptStdio;
}
void initialize_console_keyboard(const ArgvState& options,ReplInputState& input,ConsoleOutputState& s,const ConsoleRuntime& r) {
    if(options.keyboard) {
        const char* path=value(options.keyboard_file);
        r.print(s.standard_output,"\nRedirecting keyboard input to <%s>...",path);
        input.script_file=r.open(path,"r",r.user);
        if(!input.script_file)console_error("Error:  Unable to open keyboard input file.",1,s,r);
        else input.scripted=1;
    }
}
void initialize_console_session(ConsoleSession& s) {
    for(unsigned i=0;i<sizeof(s);++i)reinterpret_cast<volatile unsigned char*>(&s)[i]=0;
    s.input.script_path=s.script_path;s.input.line_buffer=s.line;s.output.message=s.line;
    copy(s.input_log_path,"asitic_i.LOG");copy(s.output_log_path,"asitic.LOG");
}
}
