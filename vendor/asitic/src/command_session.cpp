#include "asitic/command_session.hpp"
namespace asitic {
void execute_command_script(const char* path,CommandScriptWorkspace& workspace,CommandParseHistory* histories,const CommandScriptRuntime& r) {
    void* file=r.open(path,"r",r.user);
    if(!file) { r.output.emit("Error:  Unable to open file.");return; }
    while(r.read_line(workspace.line,300,file,r.user)) {
        r.output.emit(workspace.line);
        normalize_input_line(workspace.line,2,r.parser.characters);
        if(workspace.line[0]!='#') {
            const int id=parse_command_arguments(workspace.line,workspace.arguments,r.parser,histories);
            r.dispatch(id,workspace.arguments,r.user);
        }
    }
    r.close(file,r.user);
}
}
