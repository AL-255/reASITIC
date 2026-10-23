#include "asitic/console_binary.hpp"
extern "C" void* fopen(const char*,const char*);
extern "C" unsigned fread(void*,unsigned,unsigned,void*);
extern "C" unsigned fwrite(const void*,unsigned,unsigned,void*);
extern "C" int fclose(void*);
extern "C" int fseek(void*,long,int);
extern "C" long ftell(void*);
extern "C" void* realloc(void*,unsigned);
extern "C" void free(void*);
namespace asitic {
namespace {
void zero(void* pointer,unsigned size) {
    auto* bytes=static_cast<unsigned char*>(pointer);for(unsigned i=0;i<size;++i)bytes[i]=0;
}
[[noreturn]] void invalid(ConsoleBinaryState& state) {
    if(state.invalid)state.invalid(state.user);
    __builtin_trap();
}
[[noreturn]] void no_memory(ConsoleBinaryState& state) {
    state.runtime.geometry.errors.emit("Cannot allocate memory");
    if(state.runtime.geometry.terminate)state.runtime.geometry.terminate(-1,state.runtime.geometry.user);
    invalid(state);
}
void reserve(ConsoleBinaryState& state,unsigned count) {
    if(count<=state.capacity)return;
    if(count>~0u/sizeof(BinarySegmentHistory))no_memory(state);
    auto* replacement=static_cast<BinarySegmentHistory*>(::realloc(state.segments,count*sizeof(BinarySegmentHistory)));
    if(!replacement)no_memory(state);
    zero(replacement+state.capacity,(count-state.capacity)*sizeof(BinarySegmentHistory));
    state.segments=replacement;state.capacity=count;state.history.segments=replacement;
}
void* open(const char* path,const char* mode,void* user) {
    auto& state=*static_cast<ConsoleBinaryState*>(user);void* file=::fopen(path,mode);
    if(file && state.loading && mode[0]=='r') {
        const long position=::ftell(file);
        if(position>=0 && ::fseek(file,0,2)==0) {
            const long length=::ftell(file);::fseek(file,position,0);
            // Every complete raw shape consumes a four-byte marker plus its
            // 188-byte record. An extra incoming history covers partial EOF.
            if(length>=0)reserve(state,static_cast<unsigned>(length)/192u+1u);
        }
    }
    return file;
}
unsigned read(void* output,unsigned size,unsigned count,void* file,void* user) {
    auto& state=*static_cast<ConsoleBinaryState*>(user);
    if(state.loading && size==188 && count==1) {
        if(state.shape_reads==~0u)no_memory(state);
        // This also handles files that grow after opening and streams whose
        // length cannot be queried. No active segment-history reference exists
        // while the outer shape record is being read.
        reserve(state,++state.shape_reads);
    }
    return ::fread(output,size,count,file);
}
unsigned write(const void* input,unsigned size,unsigned count,void* file,void*) { return ::fwrite(input,size,count,file); }
int close(void* file,void*) { return ::fclose(file); }
int seek(void* file,long offset,int origin,void*) { return ::fseek(file,offset,origin); }
[[noreturn]] void raise(BinarySessionError,const char*,void* user) { invalid(*static_cast<ConsoleBinaryState*>(user)); }
void refresh(ConsoleBinaryState& state,const CommandRuntime& parser) {
    auto& runtime=state.runtime;runtime.selected=parser.selected;runtime.format=parser.format;runtime.line_buffer=parser.message;runtime.output=parser.warnings;
    runtime.files.warnings=parser.warnings;runtime.files.format=parser.format;runtime.files.line=parser.message;
}
}
void initialize_console_binary(ConsoleBinaryState& state,const GeometryRuntime& geometry,const CommandRuntime& parser,
    void (*on_invalid)(void*),void* user) {
    zero(&state,sizeof(state));state.runtime.geometry=geometry;state.invalid=on_invalid;state.user=user;
    state.runtime.files={open,read,write,close,seek,raise,&state,&state,parser.warnings,parser.format,parser.message};
    state.history.file.conflict_validation=&state.conflict_history;refresh(state,parser);
}
bool execute_console_binary_command(int id,const CommandArguments& arguments,GeometryShape*& head,
    ConsoleBinaryState& state,const CommandRuntime& parser) {
    if(id!=201 && id!=202 && id!=205)return false;
    refresh(state,parser);state.loading=id==202;state.shape_reads=0;state.history.segments=state.segments;
    const bool handled=execute_binary_session_command(id,arguments,head,state.runtime,state.history);
    state.loading=false;return handled;
}
void release_console_binary(ConsoleBinaryState& state) {
    ::free(state.segments);state.segments=nullptr;state.history.segments=nullptr;state.capacity=state.shape_reads=0;state.loading=false;
}
}
