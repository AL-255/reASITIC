#include "asitic/console_session.hpp"
#include "asitic/console_diagnostics.hpp"
#include "asitic/console_runtime.hpp"
#include "asitic/console_commands.hpp"
#include "asitic/console_geometry.hpp"
#include "asitic/console_analysis.hpp"
#include "asitic/console_optimization.hpp"
#include "asitic/console_binary.hpp"
#include "asitic/console_text.hpp"
#include "asitic/console_readline.hpp"
#include "asitic/technology_commands.hpp"
#include "asitic/command_help.hpp"
#include "asitic/display_commands.hpp"
#include "asitic/two_port_input_command.hpp"
#include "asitic/console_graphics.hpp"
extern "C" void* malloc(unsigned);
extern "C" void free(void*);
extern "C" int sprintf(char*,const char*,...);
extern "C" int fprintf(void*,const char*,...);
extern "C" int feof(void*);
extern "C" int kill(int,int);
extern "C" [[noreturn]] void exit(int);
extern "C" void* stdin;
extern "C" void (*signal(int,void (*)(int)))(int);

namespace asitic {
namespace {
struct Allocation { Allocation* next;void* pointer; };
struct History { History* next;char text[1]; };
struct Host {
    ConsoleSession console;
    ConsoleRuntime output;
    ConsoleInputContext input_context;
    ReplInputRuntime input;
    ArgvState arguments;
    TechnologyInitialization technology;
    TechnologyRuntime technology_runtime;
    TechnologyCommandSettings settings;
    char technology_name[80],technology_directory[200];
    int eddy_indices[32];
    ConsoleGeometryState geometry;
    ConsoleAnalysisState analysis;
    ConsoleOptimizationState optimization;
    ConsoleBinaryState binary;
    ConsoleTextState text;
    ConsoleReadlineState readline;
    ConsoleGraphicsState graphics;
    ReplAcquisition acquisition;
    ConsoleSessionPolicy policy;
    NativeDispatchState native;
    CommandRuntime parser;
    CommandArguments command;
    CommandParseHistory scan[15];
    CommandScriptWorkspace script_workspace;
    CommandScriptRuntime script;
    unsigned char script_recording;
    Allocation* allocations;
    History* histories;
    void* jump[5];
    char line[1202];
    char* current_line;
    int escape,exit_status;
    bool geometry_ready,analysis_ready,optimization_ready,files_ready,readline_ready,released,jump_ready,signals_installed,startup_script,graphics_ready;
};
Host* active_host=nullptr;
unsigned length(const char* p) { const char* begin=p;while(*p)++p;return p-begin; }
void copy(char* out,const char* in) { while((*out++=*in++)) {} }
void warning(const char* text,void* user) {
    auto& h=*static_cast<Host*>(user);console_error(text,h.console.output.output_log?1:0,h.console.output,h.output);
}
void status(const char* text,void* user) { auto& h=*static_cast<Host*>(user);console_status(text,h.console.output,h.output); }
void progress_output(const char* text,void* user) { auto& h=*static_cast<Host*>(user);console_output(text,h.console.output,h.output); }
void fatal(const char* text,void* user) { auto& h=*static_cast<Host*>(user);fatal_console(text,h.console.output,h.output); }
void* allocate(unsigned bytes,void* user) {
    auto& h=*static_cast<Host*>(user);void* pointer=malloc(bytes?bytes:1);
    auto* block=static_cast<Allocation*>(malloc(sizeof(Allocation)));
    if(!pointer || !block) { free(pointer);free(block);fatal("Could not allocate sufficient memory.",user);return nullptr; }
    block->next=h.allocations;block->pointer=pointer;h.allocations=block;return pointer;
}
void release(void* pointer,void* user) {
    auto& h=*static_cast<Host*>(user);Allocation** link=&h.allocations;
    while(*link && (*link)->pointer!=pointer)link=&(*link)->next;
    if(*link) { Allocation* block=*link;*link=block->next;free(block); }free(pointer);
}
void history(const char* line,void* user) {
    auto& h=*static_cast<Host*>(user);auto* node=static_cast<History*>(malloc(sizeof(History)+length(line)));
    if(!node)fatal("Could not allocate history memory.",user);
    node->next=h.histories;copy(node->text,line);h.histories=node;console_readline_add_history(line,h.readline);
}
void cancel(void* user) {
    auto& h=*static_cast<Host*>(user);h.escape=1;
    if(h.jump_ready)__builtin_longjmp(h.jump,1);
    shutdown_console(h.console.output,h.output);
}
void invalid(void* user) {
    auto& h=*static_cast<Host*>(user);h.escape=3;if(h.jump_ready)__builtin_longjmp(h.jump,1);
    fatal("Unable to create requested geometry.",user);
}
void destroy_ui(void* user) { auto& h=*static_cast<Host*>(user);if(h.graphics_ready) { destroy_console_graphics(h.graphics);h.graphics_ready=false; } }
void release_session(void* user) {
    auto& h=*static_cast<Host*>(user);if(h.released)return;h.released=true;
    if(h.analysis_ready)release_console_analysis(h.analysis);
    if(h.files_ready) { release_console_binary(h.binary);release_console_text(h.text); }
    if(h.readline_ready)release_console_readline(h.readline);
    if(h.geometry_ready)release_console_geometry(h.geometry);
    while(h.allocations) { Allocation* block=h.allocations;h.allocations=block->next;free(block->pointer);free(block); }
    while(h.histories) { History* node=h.histories;h.histories=node->next;free(node); }
}
void terminate(int code,void* user) {
    auto& h=*static_cast<Host*>(user);release_session(user);
    free(h.arguments.keyboard_file);free(h.arguments.execute_file);free(h.arguments.technology_file);
    free(h.arguments.initialize_file);free(h.arguments.log_file);
    ::exit(code?code:h.exit_status);
}
void publish_parser_state(Host& h) {
    h.parser.shapes=h.geometry.head;h.parser.selected=h.geometry.selected;
    h.parser.grid_size=h.settings.snap_size;h.parser.snap=h.settings.snap_enabled!=0;
    h.script.parser=h.parser;
}
void pump_graphics(void* context);
void refresh(Host& h) {
    h.console.input.prompt_echo=h.console.output.recording;
    h.input=h.readline_ready?libc_console_readline_runtime(h.input_context,h.readline):libc_console_input_runtime(h.input_context);
    if(h.graphics_ready)h.input.event_loop=pump_graphics;
    publish_parser_state(h);
    h.technology.input.width=h.native.chip_width;h.technology.input.height=h.native.chip_height;
    refresh_console_geometry(h.geometry,h.technology,h.parser,h.native);
    auto& c=h.analysis.controls;
    c.limits={h.technology.half_width,h.technology.half_height,h.technology.thickness_cell,h.technology.length_step,h.technology.width_step};
    c.alpha=h.settings.auto_alpha;c.beta=h.settings.auto_beta;c.maximum_width_cells=h.native.maximum_width_cells;
    c.eddy_mode=h.native.eddy;c.automatic_cells=h.settings.automatic_cells;c.progress=1;
    c.timing=h.native.timing;c.dump_matrices=h.native.dump_matrices;c.verbose=h.native.verbose;
    c.technology_io=h.settings.technology_io;
    refresh_console_analysis(h.analysis);
    if(h.optimization_ready)refresh_console_optimization(h.optimization);
}
void pump_graphics(void* context) {
    auto& h=*static_cast<Host*>(static_cast<ConsoleInputContext*>(context)->user);pump_console_graphics(h.graphics);publish_parser_state(h);
}
void read(const char* prompt,char* out,void* user) {
    auto& h=*static_cast<Host*>(user);char* previous=h.current_line;h.current_line=out;
    repl_read_command_line(prompt,out,0,h.acquisition,h.console.input,h.input);h.current_line=previous;
}
void prompt_technology(char* out,unsigned char history_flag,void* user) {
    auto& h=*static_cast<Host*>(user);
    if(h.readline_ready)repl_read_unprompted(out,history_flag,h.console.input,h.input);
    else repl_read_stdio_unprompted(out,history_flag,h.console.input,h.input);
}
void prompt_output(const char* text,void* user) {
    auto& h=*static_cast<Host*>(user);repl_print_prompt(text,1,h.console.input,h.input);
}
void cleanup(void* user) { auto& h=*static_cast<Host*>(user);if(h.graphics_ready)cleanup_console_graphics(h.graphics); }
void redraw(void* user) { auto& h=*static_cast<Host*>(user);if(h.graphics_ready)redraw_console_graphics(h.graphics); }
void stop_signal(int group,int number,void* user) {
    auto& h=*static_cast<Host*>(user);
    if(h.policy.signal_process_group)::kill(group,number);
    else shutdown_console(h.console.output,h.output);
}
void dispatch(int id,CommandArguments& arguments,void* user) {
    auto& h=*static_cast<Host*>(user);refresh(h);bool suppressed=false;
    CommandScriptWorkspace nested_workspace{};CommandParseHistory nested_histories[15]{};
    ConsoleCommandRuntime console{&h.output,&h.input,h.acquisition,&nested_workspace,nested_histories,&h.script};
    const ViewOptionDisplay view{h.geometry.technology.metals,h.geometry.view.visible_metals,h.geometry.technology.metal_count,
        h.settings.snap_size,h.settings.grid_size,h.geometry.view.zoom,h.settings.snap_enabled,h.settings.grid_enabled,h.settings.technology_io};
    bool handled=execute_native_command(id,arguments,h.native,h.parser,&suppressed)
        || execute_technology_command(id,arguments,h.technology,h.settings,h.technology_runtime)
        || execute_display_command(id,arguments,view,h.parser)
        || execute_help_command(id,arguments,h.parser)
        || execute_console_command(id,arguments,h.console.input,h.console.output,h.script_recording,console,&suppressed)
        || execute_console_binary_command(id,arguments,h.geometry.head,h.binary,h.parser)
        || execute_console_text_command(id,arguments,h.geometry.head,h.text,&suppressed)
        || execute_console_geometry_command(id,arguments,h.geometry,h.technology,h.parser,h.native,&suppressed);
    bool numerical=false;
    if(!handled) { handled=execute_console_analysis(id,arguments,h.analysis,h.geometry.head,h.parser);numerical=handled; }
    if(!handled)handled=execute_two_port_input_command(id,arguments,h.analysis.admittance,h.analysis.capacitance.cache.frequency,h.analysis.report);
    if(!handled) { handled=execute_console_optimization(id,arguments,h.optimization,&suppressed);numerical=handled; }
    if(numerical)h.settings.dirty=h.analysis.capacitance.cache.dirty;
    else if(h.settings.dirty)h.analysis.capacitance.cache.dirty=true;
    if(!handled) {
        h.output.print(h.console.output.standard_error,"\nStandalone build: command %s (id %d) has no bound implementation.\n",arguments.command.text,id);
        h.exit_status=2;suppressed=true;
    }
    // The native optional recording postlude follows every ordinary handler.
    if(h.console.output.recording && !suppressed)record_command_transcript(arguments,{h.console.output.recording_output,h.console.output.recording_input,h.output.print});
    // Script execution parses the next line through its own runtime copy.
    // Publish only parser-visible state here; a full refresh would overwrite
    // numerical controls changed by an analysis or optimization command.
    publish_parser_state(h);
}
void script_output(const char* text,void* user) { warning(text,user); }
extern "C" void console_abort_signal(int) {
    if(!active_host)return;
    auto& h=*active_host;h.console.input.abort_pending=1;warning("Hit 'Enter' to quit current command.",&h);
}
extern "C" void console_quit_signal(int) { if(active_host)shutdown_console(active_host->console.output,active_host->output); }
void install_signals(Host& h) {
    const int numbers[]={2,6,3};const char* names[]={"SIGINT","SIGABRT","SIGQUIT"};
    for(unsigned i=0;i<3;++i)if(reinterpret_cast<unsigned>(::signal(numbers[i],i==2?console_quit_signal:console_abort_signal))==~0u) {
        h.output.print(h.console.output.standard_error,"Error:  Cannot set handler for %s\n",names[i]);terminate(1,&h);
    }
}
}

int run_console_session(int argc,char** argv,ConsoleSessionPolicy policy) {
    Host h{};h.policy=policy;active_host=&h;initialize_console_session(h.console);attach_libc_console_streams(h.console.output);
    h.output=libc_console_runtime({destroy_ui,release_session,&h});h.output.terminate=terminate;
    h.input_context={&h.console.output,&h.output,history,cancel,&h};h.input=libc_console_input_runtime(h.input_context);
    parse_startup_arguments(argc,argv,h.arguments,libc_startup_arguments({warning,&h}));
    initialize_console_banner(h.arguments,h.console.output,h.output);
    if(h.arguments.readline) {
        if(!initialize_console_readline(h.readline)) { h.output.print(h.console.output.standard_error,"Unable to initialize GNU readline: %s\n",h.readline.error);terminate(2,&h); }
        h.readline_ready=true;
    }
    h.console.input.history_enabled=h.arguments.readline;
    select_console_acquisition(h.arguments.readline,h.acquisition,h.console.output,h.output);
    ConsoleLogPaths paths{h.console.input_log_path,h.console.output_log_path};initialize_console_logs(h.arguments,paths,h.console.output,h.output);
    h.input=h.readline_ready?libc_console_readline_runtime(h.input_context,h.readline):libc_console_input_runtime(h.input_context);
    h.technology.input={h.technology_name,h.technology_directory,-1.,-1.,-1,-1,1.,0,h.eddy_indices};
    const char* colors[]={"red","blue","green","yellow","orange","grey","white","cyan","purple","brown","navy","lightblue","wheat","ivory","beige","magenta","sienna"};
    for(unsigned i=0;i<17;++i)h.technology.colors.palette[i]=colors[i];
    h.technology_runtime={{allocate,&h},{{allocate,&h},release},{warning,&h},{fatal,&h},h.console.line,sprintf};
    TechnologyFiles technology_files=libc_technology_files();technology_files.prompt=prompt_technology;technology_files.prompt_output={prompt_output,&h};technology_files.user=&h;
    if(h.arguments.technology)h.output.print(h.console.output.standard_output,"\nReading technology file <%s>...",h.arguments.technology_file?h.arguments.technology_file:"");
    TechnologyParseHistory technology_history{0};
    if(!load_technology(h.arguments.technology?h.arguments.technology_file:nullptr,h.technology,technology_history,h.technology_runtime,libc_technology_text(),technology_files)) {
        h.exit_status=255;shutdown_console(h.console.output,h.output);
    }
    h.settings={20.,.1,.05,.8,1,1,1,1,0};
    h.native.chip_width=h.technology.input.width;h.native.chip_height=h.technology.input.height;h.native.maximum_width_cells=500;h.native.verbose=1;
    for(float& scale:h.native.scale3d)scale=1;
    h.parser.message=h.console.line;h.parser.warnings={warning,&h};h.parser.frequency_scale=1e9;h.parser.length_scale=1e-6;
    h.parser=libc_console_command_runtime(h.parser);
    initialize_console_geometry(h.geometry,h.technology,h.parser,read,invalid,&h);h.geometry_ready=true;
    h.geometry.view.zoom=1.; // Native initial view scale; graphical fit may replace it.
    if(!h.arguments.nographics) {
        X11Runtime prototype{};prototype.memory=h.geometry.geometry.memory;prototype.warnings={warning,&h};prototype.fatal={fatal,&h};
        if(!prepare_console_graphics(h.graphics,prototype)) { h.output.print(h.console.output.standard_error,"Unable to initialize Xlib: %s\n",h.graphics.library.error);terminate(2,&h); }
        h.graphics_ready=true;h.console.input.dragging=h.graphics.x11.disabled;
    } else { h.console.input.dragging=1;h.output.print(h.console.output.standard_output,"\nRunning in console mode..."); }
    initialize_console_keyboard(h.arguments,h.console.input,h.console.output,h.output);warning("",&h);
    const AnalysisReportRuntime report{h.console.line,sprintf,{warning,&h},h.output.open,h.output.print,h.output.close,h.output.user};
    if(!initialize_console_analysis(h.analysis,h.technology,h.geometry.head,h.geometry.geometry,h.geometry.commands,report,h.parser.characters))fatal("Could not initialize numerical workspace.",&h);
    h.analysis.status={status,&h};h.analysis.progress_output={progress_output,&h};h.analysis_ready=true;refresh(h);
    initialize_console_binary(h.binary,h.geometry.geometry,h.parser,invalid,&h);
    initialize_console_text(h.text,h.technology,h.geometry,h.parser,read,invalid,&h);h.files_ready=true;
    initialize_console_optimization(h.optimization,h.geometry,h.analysis,h.technology,h.parser,read,invalid,&h);h.optimization_ready=true;
    if(h.graphics_ready)initialize_console_graphics(h.graphics,argc,argv,h.native,h.geometry.view,h.settings,h.technology.tables,h.geometry.head,h.geometry.selected,{h.console.line,dispatch,&h});
    h.script={h.parser,h.output.open,h.input.read_line,h.output.close,{script_output,&h},dispatch,&h};
    h.jump_ready=true;h.startup_script=h.arguments.execute!=0;
    for(;;) {
        h.escape=0;
        if(__builtin_setjmp(h.jump)) { if(h.escape==1)warning("Exiting current command due to exception.",&h);continue; }
        if(h.startup_script) {
            h.startup_script=false;h.output.print(h.console.output.standard_output,"\nExecuting script <%s>...",h.arguments.execute_file?h.arguments.execute_file:"");
            execute_command_script(h.arguments.execute_file?h.arguments.execute_file:"",h.script_workspace,h.scan,h.script);continue;
        }
        if(!h.signals_installed) { install_signals(h);h.signals_installed=true; }
        refresh(h);h.current_line=h.line+1;
        repl_read_command_line("ASITIC>  ",h.current_line,1,h.acquisition,h.console.input,h.input);
        // Native EOF retains the previous input and continues. A finite EOF
        // lifecycle is available only to an explicitly configured embedding.
        if(policy.terminate_at_stdin_eof && !h.console.input.scripted && ::feof(::stdin))break;
        publish_parser_state(h);
        CommandDispatchRuntime dispatcher{h.parser,history,dispatch,cleanup,redraw,stop_signal,&h.console.input.dragging,&h};
        dispatch_command_line(h.current_line,h.command,h.scan,dispatcher);
    }
    shutdown_console(h.console.output,h.output);return 0;
}
}
namespace asitic::lapack {
[[noreturn]] void fatal_error(const char* message) {
    asitic::console_f2c_legacy_fatal(message);
}
[[noreturn]] void application_fatal_error(const char* message) {
    if(asitic::active_host)asitic::fatal(message,asitic::active_host);
    ::exit(1);
}
void argument_error(const char* routine,int argument) {
    asitic::console_lapack_argument_error(routine,argument);
}
}
