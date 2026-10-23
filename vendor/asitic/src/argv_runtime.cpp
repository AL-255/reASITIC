#include "asitic/argv_runtime.hpp"
namespace asitic {
namespace {
const ArgvLongOption options[]={
    {"exec",1,nullptr,'x'},{"keyboard",1,nullptr,'k'},{"tech",1,nullptr,'t'},
    {"tek",1,nullptr,'t'},{"technology",1,nullptr,'t'},{"log",1,nullptr,'l'},
    {"nographics",0,nullptr,'g'},{"ngr",0,nullptr,'g'},{"nw",0,nullptr,'g'},
    {"initfile",1,nullptr,'i'},{"readline",0,nullptr,'r'},{"rl",0,nullptr,'r'},
    {"help",0,nullptr,'h'},{"version",0,nullptr,'v'},{"ver",0,nullptr,'v'},
    {nullptr,0,nullptr,0}};
const char usage[]="Usage:  asitic [options]\nwhere [] indicates optional argument:\n\n"
    "[-x execfile | --exec execfile]  \tExecute filename upon startup.\n"
    "[-k keyfile | --keyboard keyfile]\tRedirect keyboard input to filename.\n"
    "[-t techfile | --tech techfile]  \tUse filename as the technology file.\n"
    "[-i initfile | --init initfile]  \tInitialize asitic using filename.\n"
    "[-l logfile | --log logfile]     \tUse logfile to log session.\n"
    "[-g | --ngr | -nw | --nographics]\tDo not open graphics window.\n"
    "[-r | --rl | --readline]         \tUse the readline library.\n"
    "[-v | --ver | --version]         \tPrint version information and quit.\n"
    "[-h | --help]                    \tDisplay help on command line options.";
void replace(char** destination,const ArgvRuntime& r) {
    const char* text=*r.option_argument;unsigned length=0;while(text[length])++length;
    r.replace(destination,0,~0u,text,length);
}
}
const ArgvLongOption* native_startup_options() { return options; }
const char* native_startup_usage() { return usage; }
void validate_startup_arguments(ArgvState& state,WarningSink warnings) {
    unsigned char execute=state.execute;
    if(execute) {
        if(state.keyboard) {
            warnings.emit("Error:  Execute script option and keyboard redirection cannot be specified simultaneously.");
            state.execute=state.keyboard=0;execute=0;
        }
        if(execute && !state.technology) {
            warnings.emit("Error:  Execute script option requires a --tech  technology file argument.");state.execute=0;
        }
    }
    if(state.keyboard && !state.technology) {
        warnings.emit("Error:  Keyboard redirection requires a --tech technology file argument.");state.keyboard=0;
    }
}
void parse_startup_arguments(int argc,char** argv,ArgvState& state,const ArgvRuntime& r) {
    for(;;) {
        int index=0;const int option=r.next_option(argc,argv,"x:k:t:l:i:grvh",options,&index);
        switch(option) {
        case -1:
            validate_startup_arguments(state,r.warnings);
            if(*r.option_index<argc) {
                const int count=argc-*r.option_index;
                for(int i=0;i<count;++i)argv[i]=argv[i+*r.option_index];
            }return;
        case 'x':replace(&state.execute_file,r);state.execute=1;break;
        case 'k':replace(&state.keyboard_file,r);state.keyboard=1;break;
        case 't':replace(&state.technology_file,r);state.technology=1;break;
        case 'i':replace(&state.initialize_file,r);state.initialize=1;break;
        case 'l':replace(&state.log_file,r);state.log=1;break;
        case 'g':state.nographics=1;break;
        case 'r':state.readline=1;break;
        case 'v':state.version=1;break;
        case 'h':r.error_print(r.error_file,usage);r.terminate(0);return;
        case 0:
            r.print("option %s",options[index].name);
            if(*r.option_argument)r.print(" with arg %s",*r.option_argument);
            r.print("\n");break;
        case '?':break;
        default:r.print("?? getopt returned character code 0%o ??\n",option);break;
        }
    }
}
}
