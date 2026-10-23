#include "asitic/console_text.hpp"
extern "C" void* fopen(const char*,const char*);
extern "C" int fclose(void*);
extern "C" int fseek(void*,int,int);
extern "C" int feof(void*);
extern "C" char* fgets(char*,int,void*);
extern "C" int vfprintf(void*,const char*,__builtin_va_list);
extern "C" int sprintf(char*,const char*,...);
extern "C" int time(int*);
extern "C" char* ctime(const int*);
extern "C" void* realloc(void*,unsigned);
extern "C" void free(void*);
namespace asitic {
namespace {
void zero(void* address,unsigned bytes) { auto* p=static_cast<volatile unsigned char*>(address);for(unsigned i=0;i<bytes;++i)p[i]=0; }
void copy(char* destination,const char* source) { while((*destination++=*source++)) {} }
int print(void* file,const char* format,...) {
    __builtin_va_list args;__builtin_va_start(args,format);const int n=vfprintf(file,format,args);__builtin_va_end(args);return n;
}
void* open(const char* name,const char* mode) { return fopen(name,mode); }
int close(void* file) { return fclose(file); }
int seek(void* file,int offset,int origin) { return fseek(file,offset,origin); }
void* open_user(const char* name,const char* mode,void*) { return fopen(name,mode); }
int close_user(void* file,void*) { return fclose(file); }
int now(int* value,void*) { return time(value); }
char* calendar(const int* value,void*) { return ctime(value); }
void fail_parse(SessionParseFailure,void* user) { auto& s=*static_cast<ConsoleTextState*>(user);s.invalid(s.user);__builtin_trap(); }
void fail_file(TextSessionFailure,const char*,void* user) { auto& s=*static_cast<ConsoleTextState*>(user);s.invalid(s.user);__builtin_trap(); }
void prompt(const char* text,char* out,unsigned char,void* user) { auto& s=*static_cast<ConsoleTextState*>(user);s.read(text,out,s.user); }
bool prefix(const char* text,const char* key,unsigned n) { for(unsigned i=0;i<n;++i)if(text[i]!=key[i])return false;return true; }
double number(const unsigned& value) {
    double out;asm volatile("flds %1; fstpl %0":"=m"(out):"m"(value):"st");return out;
}
TextSessionCommandRuntime runtime(ConsoleTextState& s,GeometryShape* head) {
    auto commands=*s.parser;commands.shapes=head;commands.selected=s.geometry->selected;
    const auto& t=*s.technology;const auto& g=s.geometry->technology;
    SessionParseRuntime parsing{libc_technology_text(),commands.message,feof,close,commands.warnings,fail_parse,&s};
    TextSessionRuntime files{{open,close,seek},{print},parsing,fail_file,&s};
    SessionGeometryParseOptions record{commands,g,t.tables.via_count,s.geometry->prompt.chip_x,s.geometry->prompt.chip_y,parsing};
    return {files,{g.metals,g.vias,g.metal_count},s.geometry->geometry,s.geometry->selected,record,commands.format,commands.message,commands.warnings};
}
void reserve_input_histories(ConsoleTextState& s,const char* filename) {
    // Every loaded shape consumes a separate BLOCK=SPIRAL fgets chunk. Count
    // chunks using the same 1024-byte capacity as the parser, including long
    // physical lines, so valid inputs cannot exceed this backing allocation.
    char path[80];copy(path,filename);bool extension=false;
    for(const char* p=filename;*p;++p)if(p[0]=='.' && p[1]=='s' && p[2]=='p' && p[3]=='i') { extension=true;break; }
    if(!extension) { char* end=path;while(*end)++end;copy(end,".spi"); }
    unsigned frames=1;void* file=fopen(path,"r");
    if(file) { char line[1024];while(fgets(line,1024,file))++frames;fclose(file); }
    if(frames<=s.segment_history_capacity)return;
    const unsigned long long bytes=static_cast<unsigned long long>(frames)*sizeof(TextSessionSegmentHistory);
    if(bytes>0xffffffffu) { s.parser->warnings.emit("Cannot allocate memory");s.invalid(s.user);__builtin_trap(); }
    auto* bank=static_cast<TextSessionSegmentHistory*>(realloc(s.history.segments,static_cast<unsigned>(bytes)));
    if(!bank) { s.parser->warnings.emit("Cannot allocate memory");s.invalid(s.user);__builtin_trap(); }
    zero(bank+s.segment_history_capacity,(frames-s.segment_history_capacity)*sizeof(TextSessionSegmentHistory));
    s.history.segments=bank;s.segment_history_capacity=frames;
}
}
void initialize_console_text(ConsoleTextState& s,const TechnologyInitialization& technology,ConsoleGeometryState& geometry,
    CommandRuntime& parser,void (*read)(const char*,char*,void*),void (*invalid)(void*),void* user) {
    zero(&s,sizeof(s));s.technology=&technology;s.geometry=&geometry;s.parser=&parser;s.read=read;s.invalid=invalid;s.user=user;
}
bool execute_console_text_command(int id,CommandArguments& arguments,GeometryShape*& head,ConsoleTextState& s,bool* suppress_recording) {
    if(id>=225 && id<=227) {
        if(id==226)reserve_input_histories(s,arguments.arguments[0].text);
        const auto run=runtime(s,head);execute_text_session_command(id,arguments,head,run,s.history);
        s.geometry->head=head;s.parser->shapes=head;if(suppress_recording)*suppress_recording=false;
        return true;
    }
    if(id==300) {
        const int count=static_cast<int>(arguments.command.value);
        double scale=count>2?number(arguments.arguments[2].value):1.;double resolution=count>3?number(arguments.arguments[3].value):100.;
        if(scale<0.) { s.parser->warnings.emit("Warning:  Must specify non-negative <scale>.  Using default value of 1.0");scale=1.; }
        if(resolution<0.) { s.parser->warnings.emit("Warning:  Must specify non-negative <res>.  Using default value of 100.0");resolution=100.; }
        CifMode mode=CifMode::polygon;
        if(count>4) {
            char* text=normalize_input_line(arguments.arguments[4].text,0x20,s.parser->characters);
            if(prefix(text,"BOX",3))mode=CifMode::box;
            else {
                text=normalize_input_line(arguments.arguments[4].text,0x20,s.parser->characters);
                if(prefix(text,"WIRE",4))mode=CifMode::wire;
                else {
                    text=normalize_input_line(arguments.arguments[4].text,0x20,s.parser->characters);
                    if(!prefix(text,"POLY",4))s.parser->warnings.emit("Warning:  Did not understand 5th argument.  Using default of <poly> instead.");
                }
            }
        }
        const CifExportRuntime run{open_user,close_user,print,now,calendar,s.parser->characters,s.parser->warnings,&s};
        export_cif(*reinterpret_cast<const GeometryShape*>(arguments.arguments[0].value),arguments.arguments[1].text,scale,resolution,mode,s.technology->tables,run);
        if(suppress_recording)*suppress_recording=false;
        return true;
    }
    if(id==302) {
        const SonnetExportRuntime run{prompt,open_user,close_user,print,s.parser->scan,s.parser->warnings,&s};
        export_sonnet(head,s.technology->tables,s.geometry->prompt.chip_x,s.geometry->prompt.chip_y,s.sonnet_history,run);
        if(suppress_recording)*suppress_recording=false;
        return true;
    }
    return false;
}
void release_console_text(ConsoleTextState& s) { free(s.history.segments);s.history.segments=nullptr;s.segment_history_capacity=0; }
}
