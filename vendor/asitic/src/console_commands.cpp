#include "asitic/console_commands.hpp"
namespace asitic {
namespace {
void copy(char* out,const char* in) { while((*out++=*in++)) {} }
void append(char* out,const char* in) { while(*out)++out;copy(out,in); }
}
bool execute_console_command(int id,const CommandArguments& arguments,ReplInputState& input,ConsoleOutputState& output,
    unsigned char& script_recording,const ConsoleCommandRuntime& r,bool* suppress_recording) {
    if(id!=203 && id!=209 && id!=213 && id!=215 && id!=216)return false;
    bool suppressed=false;const int count=static_cast<int>(arguments.command.value);
    const auto& io=*r.output;input.prompt_echo=output.recording;
    switch(id) {
    case 203:
        if(output.recording) {
            if(count==1)console_error("Warning:  Don't understand argument since log file is on",1,output,io);
            io.close(output.recording_output,io.user);io.close(output.recording_input,io.user);output.recording=0;
        } else {
            if(count==1)copy(output.message,arguments.arguments[0].text);
            else repl_read_command_line("Log Filename:  ",output.message,0,r.acquisition,input,*r.input);
            output.recording_output=io.open(output.message,"w",io.user);append(output.message,".out");
            output.recording_input=io.open(output.message,"w",io.user);
            if(output.recording_output && output.recording_input) {
                output.recording=1;io.print(output.recording_output,"# ASITIC version %s log file","10.20.99.0.0.0");
            } else console_error("Error:  Unable to open log file.",1,output,io);
        }
        break;
    case 209:execute_command_script(arguments.arguments[0].text,*r.script_workspace,r.script_histories,*r.script);break;
    case 213:
        if(!script_recording && !output.recording) {
            if(count==1)copy(output.message,arguments.arguments[0].text);
            else repl_read_command_line("Script Filename:  ",output.message,0,r.acquisition,input,*r.input);
            output.recording_output=io.open(output.message,"w",io.user);
            if(!output.recording_output)console_error("Error:  Unable to open script file.",1,output,io);
            else { script_recording=1;io.print(output.recording_output,"# ASITIC version %s script file","10.20.99.0.0.0"); }
        } else {
            if(count==1)console_error("Warning:  Do not understand argument since recording.",1,output,io);
            io.close(output.recording_output,io.user);script_recording=0;
        }
        if(output.recording) { suppressed=true;console_error("Error:  Cannot record and generate log file simultaneously.",1,output,io); }
        else suppressed=true; // Native returns directly, bypassing recording.
        break;
    case 215:
        if(count<1) {
            if(!input.script_path[0]) { console_error("Error:  Did not specify keyboard input filename.",1,output,io);suppressed=true;break; }
        } else copy(input.script_path,arguments.arguments[0].text);
        input.script_file=io.open(input.script_path,"r",io.user);
        if(input.script_file)input.scripted=1;
        else { suppressed=true;console_error("Error:  Unable to open keyboard input file.",1,output,io); }
        if(count==2)input.script_delay=static_cast<int>(arguments.arguments[1].value);
        if(input.script_delay>300)input.script_delay=300;
        break;
    case 216:r.input->get_character(r.input->standard_input,r.input->user);break;
    }
    input.prompt_echo=output.recording;if(suppress_recording)*suppress_recording=suppressed;return true;
}
}
