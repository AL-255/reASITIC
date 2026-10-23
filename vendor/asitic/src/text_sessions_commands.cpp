#include "asitic/text_sessions_commands.hpp"
#include "asitic/binary_sessions.hpp"
namespace asitic {
namespace {
bool same(const char* a,const char* b) { while(*a==*b) { if(!*a)return true;++a;++b; }return false; }
[[noreturn]] void raise(const TextSessionRuntime& runtime,TextSessionFailure error,const char* detail=nullptr) {
    runtime.fail(error,detail,runtime.user);__builtin_trap();
}
struct CatchFrame { void* jump[5];TextSessionFailure error;char detail[256]; };
[[noreturn]] void catch_failure(TextSessionFailure error,const char* detail,void* context) {
    auto& frame=*static_cast<CatchFrame*>(context);frame.error=error;unsigned i=0;
    if(detail)while(detail[i] && i<255) { frame.detail[i]=detail[i];++i; }
    frame.detail[i]=0;
    __builtin_longjmp(frame.jump,1);
}
[[noreturn]] void catch_parse_failure(SessionParseFailure error,void* context) {
    catch_failure(error==SessionParseFailure::invalid_header?TextSessionFailure::invalid_header:
        error==SessionParseFailure::file_corrupted?TextSessionFailure::file_corrupted:TextSessionFailure::unable_to_create,nullptr,context);
}
TextSessionCommandRuntime catching_runtime(const TextSessionCommandRuntime& runtime,CatchFrame& frame) {
    auto out=runtime;out.files.fail=catch_failure;out.files.user=&frame;
    out.files.parsing.fail=catch_parse_failure;out.files.parsing.user=&frame;
    out.record.file.fail=catch_parse_failure;out.record.file.user=&frame;return out;
}
void report_load(GeometryShape*& head,void* file,bool remove,const CatchFrame& frame,const TextSessionCommandRuntime& runtime) {
    if(frame.error==TextSessionFailure::unable_to_read) {
        runtime.format(runtime.line,"Error:  Unable to open %s for reading.",frame.detail);runtime.output.emit(runtime.line);
    } else if(frame.error==TextSessionFailure::file_corrupted) {
        if(remove)remove_geometry_shape(head,head,runtime.geometry.memory);
        runtime.output.emit("Error:  File appears to be corrupted.");runtime.files.files.close(file);
    } else runtime.output.emit("Exiting due to error(s).");
}
void report_save(const CatchFrame& frame,const TextSessionCommandRuntime& runtime) {
    if(frame.error==TextSessionFailure::unable_to_write) {
        runtime.format(runtime.line,"Error:  Unable to open %s for writing.",frame.detail);runtime.output.emit(runtime.line);
    } else if(frame.error==TextSessionFailure::name_conflict) {
        runtime.format(runtime.line,"Warning:  Skipped writing spiral %s due to a name conflict.",frame.detail);runtime.output.emit(runtime.line);
    } else runtime.output.emit("Exiting due to error(s).");
}
void path(char* destination,const char* source) {
    unsigned length=0;for(;;) { destination[length]=source[length];if(!source[length])break;++length; }
    bool found=false;for(unsigned i=0;i<length;++i)if(source[i]=='.' && source[i+1]=='s' && source[i+2]=='p' && source[i+3]=='i') { found=true;break; }
    if(!found) { destination[length++]='.';destination[length++]='s';destination[length++]='p';destination[length++]='i';destination[length]=0; }
}
}
void check_text_session_conflict(void* file,const char* name,const TextSessionCommandRuntime& runtime,TextSessionFileHistory& history) {
    runtime.files.files.seek(file,0,0);validate_text_session_header(file,history.header[1],runtime.files.parsing);
    unsigned token=read_text_session_block(file,runtime.files.parsing);
    while(token!=0x8b) {
        if(token!=0x88)raise(runtime.files,TextSessionFailure::file_corrupted);
        parse_text_session_shape(history.shape,file,history.record,runtime.record);
        if(same(reinterpret_cast<const char*>(&history.shape),name))raise(runtime.files,TextSessionFailure::name_conflict,name);
        skip_text_session_shape(file,runtime.files.parsing);token=read_text_session_block(file,runtime.files.parsing);
    }
}
void* open_text_session_output(const char* filename,const char* shape_name,const TextSessionCommandRuntime& runtime,TextSessionFileHistory& history) {
    char filename_buffer[80];path(filename_buffer,filename);void* file=runtime.files.files.open(filename_buffer,"r");
    if(file) {
        validate_text_session_header(file,history.header[0],runtime.files.parsing);runtime.files.files.close(file);
        file=runtime.files.files.open(filename_buffer,"r+");if(!file)raise(runtime.files,TextSessionFailure::unable_to_write,filename_buffer);
        check_text_session_conflict(file,shape_name,runtime,history);runtime.files.files.seek(file,-10,2);
    } else {
        file=runtime.files.files.open(filename_buffer,"w");if(!file)raise(runtime.files,TextSessionFailure::unable_to_write,filename_buffer);
        write_text_session_version(file,runtime.files.output);
    }
    return file;
}
void load_text_session_segments(GeometryShape& shape,void* file,const TextSessionCommandRuntime& runtime,TextSessionSegmentHistory& history) {
    unsigned token=read_text_session_block(file,runtime.files.parsing);
    while(token!=0x8c) {
        if(token!=0x89)raise(runtime.files,TextSessionFailure::file_corrupted);
        parse_text_session_segment(shape,history.parent,file,history.parent_record,runtime.record);history.parent.group_next_address=0;
        prepend_geometry_segment(shape.filaments.first,history.parent,runtime.geometry);
        token=read_text_session_block(file,runtime.files.parsing);
        while(token==0x8a) {
            parse_text_session_segment(shape,history.child,file,history.child_record,runtime.record);
            prepend_binary_session_child(shape.filaments.first,history.child,runtime.geometry);
            token=read_text_session_block(file,runtime.files.parsing);
        }
    }
    reverse_geometry_segments(shape);
}
void load_text_session(GeometryShape*& head,const char* filename,const TextSessionCommandRuntime& runtime,TextSessionLoadHistory& history) {
    CatchFrame frame;void* volatile file=nullptr;const auto run=catching_runtime(runtime,frame);
    if(__builtin_setjmp(frame.jump)) { report_load(head,file,true,frame,runtime);return; }
    void* input=nullptr;open_text_session_input(filename,input,history.file.header[0],run.files);file=input;unsigned loaded=0;
    unsigned token=read_text_session_block(file,run.files.parsing);
    while(token!=0x8b) {
        if(token!=0x88)raise(run.files,TextSessionFailure::file_corrupted);
        parse_text_session_shape(history.file.shape,file,history.file.record,run.record);
        if(find_geometry_shape(head,runtime.selected,reinterpret_cast<const char*>(&history.file.shape))) {
            runtime.format(runtime.line,"Did not load spiral <%s> due to a name conflict with a spiral in memory.",reinterpret_cast<const char*>(&history.file.shape));runtime.output.emit(runtime.line);
            skip_text_session_shape(file,run.files.parsing);
        } else {
            prepend_geometry_shape(head,run.geometry);copy_geometry_shape_fields(*head,history.file.shape);
            load_text_session_segments(*head,file,run,history.segments[loaded++]);
        }
        token=read_text_session_block(file,run.files.parsing);
    }
    runtime.files.files.close(file);
}
void save_text_session(GeometryShape* head,const char* filename,const TextSessionCommandRuntime& runtime,TextSessionFileHistory& history) {
    CatchFrame frame;const auto run=catching_runtime(runtime,frame);
    if(__builtin_setjmp(frame.jump)) { report_save(frame,runtime);return; }
    for(auto* shape=head;shape;shape=shape->next) {
        void* file=open_text_session_output(filename,reinterpret_cast<const char*>(shape),run,history);
        write_text_session_shape(*shape,file,run.layers,run.files.output);run.files.output.print(file,"\nBLOCK=END");run.files.files.close(file);
    }
}
void save_text_session_shape(const GeometryShape& shape,const char* filename,const TextSessionCommandRuntime& runtime,TextSessionFileHistory& history) {
    CatchFrame frame;const auto run=catching_runtime(runtime,frame);
    if(__builtin_setjmp(frame.jump)) { report_save(frame,runtime);return; }
    void* file=open_text_session_output(filename,reinterpret_cast<const char*>(&shape),run,history);
    write_text_session_shape(shape,file,run.layers,run.files.output);run.files.output.print(file,"\nBLOCK=END");run.files.files.close(file);
}
void list_text_session(const char* filename,const TextSessionCommandRuntime& runtime,TextSessionFileHistory& history) {
    CatchFrame frame;void* volatile file=nullptr;const auto run=catching_runtime(runtime,frame);
    if(__builtin_setjmp(frame.jump)) {
        if(frame.error==TextSessionFailure::unable_to_read) { runtime.format(runtime.line,"Error:  Unable to open %s for reading.",frame.detail);runtime.output.emit(runtime.line); }
        else if(frame.error==TextSessionFailure::file_corrupted || frame.error==TextSessionFailure::unable_to_create) {
            runtime.output.emit(frame.error==TextSessionFailure::file_corrupted?"Error:  File appears to be corrupted.":"Warning:  Spiral Header appears to be corrupted.");
            runtime.files.files.close(file);
        }
        else runtime.output.emit("Exiting due to error(s).");
        return;
    }
    void* input=nullptr;open_text_session_input(filename,input,history.header[0],run.files);file=input;
    runtime.format(runtime.line,"%-15s%-30s","Name","Description");runtime.output.emit(runtime.line);
    unsigned token=read_text_session_block(file,run.files.parsing);
    while(token!=0x8b) {
        if(token!=0x88)raise(run.files,TextSessionFailure::file_corrupted);
        parse_text_session_shape(history.shape,file,history.record,run.record);
        unsigned type;__builtin_memcpy(&type,reinterpret_cast<const unsigned char*>(&history.shape)+0x50,4);
        runtime.format(runtime.line,"%-15s%-30s",reinterpret_cast<const char*>(&history.shape),session_shape_description(type));runtime.output.emit(runtime.line);
        skip_text_session_shape(file,run.files.parsing);token=read_text_session_block(file,run.files.parsing);
    }
}
void load_text_session_shape(GeometryShape*& head,const char* name,const char* filename,const TextSessionCommandRuntime& runtime,TextSessionLoadHistory& history) {
    CatchFrame frame;void* volatile file=nullptr;volatile bool found=false;const auto run=catching_runtime(runtime,frame);
    if(__builtin_setjmp(frame.jump)) { report_load(head,file,found,frame,runtime);return; }
    void* input=nullptr;open_text_session_input(filename,input,history.file.header[0],run.files);file=input;
    unsigned token=read_text_session_block(file,run.files.parsing);
    while(token!=0x8b && !found) {
        if(token!=0x88)raise(run.files,TextSessionFailure::file_corrupted);
        parse_text_session_shape(history.file.shape,file,history.file.record,run.record);
        if(same(name,reinterpret_cast<const char*>(&history.file.shape))) {
            found=true;prepend_geometry_shape(head,run.geometry);copy_geometry_shape_fields(*head,history.file.shape);
            load_text_session_segments(*head,file,run,*history.segments);reverse_geometry_segments(*head);
        } else skip_text_session_shape(file,run.files.parsing);
        token=read_text_session_block(file,run.files.parsing);
    }
    if(!found) { runtime.format(runtime.line,"Error:  Could not find <%s> in file <%s>",name,filename);runtime.output.emit(runtime.line); }
    runtime.files.files.close(file);
}
bool execute_text_session_command(int id,const CommandArguments& arguments,GeometryShape*& head,const TextSessionCommandRuntime& runtime,TextSessionLoadHistory& history) {
    if(id<225 || id>227)return false;
    const unsigned count=arguments.command.value;
    const bool all=count==1 || (count==2 && arguments.arguments[1].text[0]=='*' && !arguments.arguments[1].text[1]);
    const char* filename=arguments.arguments[0].text;
    if(id==225) {
        if(all)save_text_session(head,filename,runtime,history.file);
        else save_text_session_shape(*reinterpret_cast<const GeometryShape*>(arguments.arguments[1].value),filename,runtime,history.file);
    } else if(id==226) {
        if(all)load_text_session(head,filename,runtime,history);
        else load_text_session_shape(head,arguments.arguments[1].text,filename,runtime,history);
    } else list_text_session(filename,runtime,history.file);
    return true;
}
}
