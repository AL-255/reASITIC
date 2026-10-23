#include "asitic/binary_sessions.hpp"
#include "asitic/composite_geometry.hpp"
#include "asitic/text_session_parse.hpp"

namespace asitic {
namespace {
void raw_copy(void* destination,const void* source,unsigned bytes) {
    auto* d=static_cast<unsigned*>(destination);const auto* s=static_cast<const unsigned*>(source);
    for(unsigned i=0;i<bytes/4;++i)d[i]=s[i];
}
int compare(const char* a,const char* b) {
    while(*a && *a==*b) { ++a;++b; }
    return static_cast<unsigned char>(*a)-static_cast<unsigned char>(*b);
}
[[noreturn]] void raise(const BinarySessionFiles& files,BinarySessionError error,const char* payload=nullptr) {
    files.raise(error,payload,files.error_user);__builtin_trap();
}
struct CatchFrame {
    void* jump[5];
    BinarySessionError error;
    char payload[256];
};
[[noreturn]] void catch_error(BinarySessionError error,const char* payload,void* opaque) {
    auto& frame=*static_cast<CatchFrame*>(opaque);frame.error=error;
    unsigned i=0;if(payload)while(payload[i] && i<255) { frame.payload[i]=payload[i];++i; }frame.payload[i]=0;
    __builtin_longjmp(frame.jump,1);
}
BinarySessionFiles catching_files(const BinarySessionFiles& files,CatchFrame& frame) {
    auto out=files;out.raise=catch_error;out.error_user=&frame;return out;
}
void report_load_error(GeometryShape*& head,void* file,bool remove,const CatchFrame& frame,const BinarySessionRuntime& runtime) {
    if(frame.error==BinarySessionError::unable_to_open_read) {
        runtime.format(runtime.line_buffer,"Error:  Unable to open %s for reading.",frame.payload);runtime.output.emit(runtime.line_buffer);
    } else if(frame.error==BinarySessionError::file_corrupted) {
        if(remove)remove_geometry_shape(head,head,runtime.geometry.memory);
        runtime.output.emit("Error:  File appears to be corrupted.");runtime.files.close(file,runtime.files.user);
    } else runtime.output.emit("Exiting due to error(s).");
}
void report_save_error(const CatchFrame& frame,const BinarySessionRuntime& runtime) {
    if(frame.error==BinarySessionError::name_conflict) {
        runtime.format(runtime.line_buffer,"Warning:  Skipped writing spiral %s due to a name conflict.",frame.payload);runtime.output.emit(runtime.line_buffer);
    } else if(frame.error==BinarySessionError::unable_to_open_write) {
        runtime.format(runtime.line_buffer,"Error:  Unable to open %s for writing.",frame.payload);runtime.output.emit(runtime.line_buffer);
    } else runtime.output.emit("Exiting due to error(s).");
}
}
void prepend_binary_session_child(const ConductorSegment*& head,const ConductorSegment& source,const GeometryRuntime& runtime) {
    auto* node=static_cast<ConductorSegment*>(runtime.memory.arrays.get(0xf0));
    if(!node) {
        runtime.errors.emit("Cannot allocate memory");
        if(runtime.terminate)runtime.terminate(-1,runtime.user);
        __builtin_trap();
    }
    raw_copy(node,&source,0xf0);auto* parent=const_cast<ConductorSegment*>(head);
    node->next_address=parent->next_address;node->group_next_address=parent->group_next_address;
    parent->group_next_address=reinterpret_cast<unsigned>(node);
}
void skip_binary_session_records(void* file,unsigned& next,const BinarySessionFiles& files,BinarySkipHistory& history) {
    for(;;) {
        files.read(&history.marker,4,1,file,files.user);
        if(history.marker==0x88 || history.marker==0x8b) { next=history.marker;return; }
        if(history.marker==0x89)files.read(&history.parent,0xf0,1,file,files.user);
        else if(history.marker==0x8a)files.read(&history.child,0xf0,1,file,files.user);
        else raise(files,BinarySessionError::file_corrupted);
    }
}
void emit_binary_session_shape(const GeometryShape& shape,void* file,const BinarySessionFiles& files) {
    unsigned marker=0x88;files.write(&marker,4,1,file,files.user);files.write(&shape,0xbc,1,file,files.user);
    for(auto* segment=shape.filaments.first;segment;segment=reinterpret_cast<const ConductorSegment*>(segment->next_address)) {
        marker=0x89;files.write(&marker,4,1,file,files.user);files.write(segment,0xf0,1,file,files.user);
        for(auto* child=reinterpret_cast<const ConductorSegment*>(segment->group_next_address);child;child=reinterpret_cast<const ConductorSegment*>(child->group_next_address)) {
            marker=0x8a;files.write(&marker,4,1,file,files.user);files.write(child,0xf0,1,file,files.user);
        }
    }
}
void load_binary_session_segments(GeometryShape& shape,void* file,unsigned& next,const BinarySessionFiles& files,
    const GeometryRuntime& runtime,BinarySegmentHistory& history) {
    files.read(&history.marker,4,1,file,files.user);
    while(history.marker!=0x88 && history.marker!=0x8b) {
        if(history.marker!=0x89)raise(files,BinarySessionError::file_corrupted);
        files.read(&history.parent,0xf0,1,file,files.user);history.parent.group_next_address=0;
        prepend_geometry_segment(shape.filaments.first,history.parent,runtime);
        files.read(&history.marker,4,1,file,files.user);
        while(history.marker==0x8a) {
            files.read(&history.child,0xf0,1,file,files.user);
            prepend_binary_session_child(shape.filaments.first,history.child,runtime);
            files.read(&history.marker,4,1,file,files.user);
        }
    }
    reverse_geometry_segments(shape);next=history.marker;
}
void load_binary_session(GeometryShape*& head,const char* filename,const BinarySessionRuntime& runtime,BinarySessionLoadHistory& history) {
    CatchFrame frame;void* volatile file=nullptr;const auto files=catching_files(runtime.files,frame);
    if(__builtin_setjmp(frame.jump)) { report_load_error(head,file,true,frame,runtime);return; }
    file=open_binary_session_read(filename,files,history.file);files.read(&history.marker,4,1,file,files.user);unsigned loaded=0;
    while(history.marker!=0x8b) {
        if(history.marker!=0x88)raise(files,BinarySessionError::file_corrupted);
        files.read(&history.shape,0xbc,1,file,files.user);
        if(find_geometry_shape(head,runtime.selected,reinterpret_cast<const char*>(&history.shape))) {
            runtime.format(runtime.line_buffer,"Did not load spiral <%s> due to a name conflict with a spiral in memory.",reinterpret_cast<const char*>(&history.shape));runtime.output.emit(runtime.line_buffer);
            skip_binary_session_records(file,history.marker,files,history.skipped);
        } else {
            prepend_geometry_shape(head,runtime.geometry);copy_geometry_shape_fields(*head,history.shape);
            load_binary_session_segments(*head,file,history.marker,files,runtime.geometry,history.segments[loaded++]);
        }
    }
    files.close(file,files.user);
}
void load_binary_session_shape(GeometryShape*& head,const char* name,const char* filename,const BinarySessionRuntime& runtime,BinarySessionLoadHistory& history) {
    CatchFrame frame;void* volatile file=nullptr;volatile bool found=false;const auto files=catching_files(runtime.files,frame);
    if(__builtin_setjmp(frame.jump)) { report_load_error(head,file,found,frame,runtime);return; }
    file=open_binary_session_read(filename,files,history.file);files.read(&history.marker,4,1,file,files.user);
    while(history.marker!=0x8b && !found) {
        if(history.marker!=0x88)raise(files,BinarySessionError::file_corrupted);
        files.read(&history.shape,0xbc,1,file,files.user);
        if(compare(name,reinterpret_cast<const char*>(&history.shape))==0) {
            found=true;prepend_geometry_shape(head,runtime.geometry);copy_geometry_shape_fields(*head,history.shape);
            load_binary_session_segments(*head,file,history.marker,files,runtime.geometry,*history.segments);reverse_geometry_segments(*head);
        } else skip_binary_session_records(file,history.marker,files,history.skipped);
    }
    if(!found) { runtime.format(runtime.line_buffer,"Error:  Could not find <%s> in file <%s>",name,filename);runtime.output.emit(runtime.line_buffer); }
    files.close(file,files.user);
}
void save_binary_session(GeometryShape* head,const char* filename,const BinarySessionRuntime& runtime,BinarySessionFileHistory& history) {
    CatchFrame frame;const auto files=catching_files(runtime.files,frame);
    if(__builtin_setjmp(frame.jump)) { report_save_error(frame,runtime);return; }
    for(auto* shape=head;shape;shape=shape->next) {
        void* file=open_binary_session_write(filename,reinterpret_cast<const char*>(shape),files,history);
        emit_binary_session_shape(*shape,file,files);const unsigned end=0x8b;files.write(&end,4,1,file,files.user);files.close(file,files.user);
    }
}
void save_binary_session_shape(const GeometryShape& shape,const char* filename,const BinarySessionRuntime& runtime,BinarySessionFileHistory& history) {
    CatchFrame frame;const auto files=catching_files(runtime.files,frame);
    if(__builtin_setjmp(frame.jump)) { report_save_error(frame,runtime);return; }
    void* file=open_binary_session_write(filename,reinterpret_cast<const char*>(&shape),files,history);
    emit_binary_session_shape(shape,file,files);const unsigned end=0x8b;files.write(&end,4,1,file,files.user);files.close(file,files.user);
}
void list_binary_session(const char* filename,const BinarySessionRuntime& runtime,BinarySessionLoadHistory& history) {
    CatchFrame frame;void* volatile file=nullptr;const auto files=catching_files(runtime.files,frame);
    if(__builtin_setjmp(frame.jump)) {
        if(frame.error==BinarySessionError::unable_to_open_read) { runtime.format(runtime.line_buffer,"Error:  Unable to open %s for reading.",frame.payload);runtime.output.emit(runtime.line_buffer); }
        else if(frame.error==BinarySessionError::file_corrupted) { runtime.output.emit("Error:  File appears to be corrupted.");files.close(file,files.user); }
        else runtime.output.emit("Exiting due to error(s).");
        return;
    }
    file=open_binary_session_read(filename,files,history.file);
    runtime.format(runtime.line_buffer,"%-15s%-30s","Name","Description");runtime.output.emit(runtime.line_buffer);
    files.read(&history.marker,4,1,file,files.user);
    while(history.marker!=0x8b) {
        if(history.marker!=0x88)raise(files,BinarySessionError::file_corrupted);
        files.read(&history.shape,0xbc,1,file,files.user);
        runtime.format(runtime.line_buffer,"%-15s%-30s",reinterpret_cast<const char*>(&history.shape),session_shape_description(*reinterpret_cast<unsigned*>(history.shape.filaments.prefix+0x50)));runtime.output.emit(runtime.line_buffer);
        skip_binary_session_records(file,history.marker,files,history.skipped);
    }
}
}
