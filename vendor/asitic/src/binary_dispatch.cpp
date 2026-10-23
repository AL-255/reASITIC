#include "asitic/binary_dispatch.hpp"
namespace asitic {
bool execute_binary_session_command(int id,const CommandArguments& arguments,GeometryShape*& head,
    const BinarySessionRuntime& runtime,BinarySessionLoadHistory& history) {
    if(id==205) { list_binary_session(arguments.arguments[0].text,runtime,history);return true; }
    if(id!=201 && id!=202)return false;
    const auto count=arguments.command.value;const char* name=arguments.arguments[1].text;
    const bool all=count==1 || (count==2 && name[0]=='*' && name[1]==0);
    const char* filename=arguments.arguments[0].text;
    if(id==201) {
        if(all)save_binary_session(head,filename,runtime,history.file);
        else save_binary_session_shape(*reinterpret_cast<const GeometryShape*>(arguments.arguments[1].value),filename,runtime,history.file);
    } else {
        if(all)load_binary_session(head,filename,runtime,history);
        else load_binary_session_shape(head,name,filename,runtime,history);
    }
    return true;
}
}
