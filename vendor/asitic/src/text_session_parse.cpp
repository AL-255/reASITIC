#include "asitic/text_session_parse.hpp"
#include "asitic/shape_prompts.hpp"
namespace asitic {
namespace {
bool same(const char* a,const char* b) { while(*a==*b) { if(!*a)return true;++a;++b; }return false; }
void read_significant_line(void* file,const SessionParseRuntime& runtime) {
    do {
        runtime.text.read_line(runtime.line,1024,file);
        normalize_input_line(runtime.line,0xae,runtime.text.characters);
    } while(runtime.line[0]=='#' || !runtime.line[0]);
}
}
unsigned read_text_session_block(void* file,const SessionParseRuntime& runtime) {
    read_significant_line(file,runtime);
    const char* names[]={"BLOCK=SPIRAL","BLOCK=SEGMENT","BLOCK=SHUNT","BLOCK=END","BLOCK=ENDSPIRAL","BLOCK=ENDSEGMENT","BLOCK=ENDSHUNT"};
    for(unsigned i=0;i<7;++i)if(same(runtime.line,names[i]))return 0x88+i;
    return 0x8f;
}
void skip_text_session_shape(void* file,const SessionParseRuntime& runtime) {
    unsigned token;
    do { token=read_text_session_block(file,runtime); } while(token!=0x8c && !runtime.end(file));
    if(token!=0x8c)runtime.fail(SessionParseFailure::file_corrupted,runtime.user);
}
void validate_text_session_header(void* file,SessionHeaderHistory& history,const SessionParseRuntime& runtime) {
    read_significant_line(file,runtime);
    runtime.text.scan(runtime.line,"%s %s",history.name,history.version);
    if(!same(history.name,"ASITIC")) {
        runtime.warnings.emit("Error:  File does not have valid header.");
        runtime.close(file);
        runtime.fail(SessionParseFailure::invalid_header,runtime.user);
    }
    if(!same(history.version,"10.20.99.0.0.0"))runtime.warnings.emit("Warning:  Reading file produced by different version of ASITIC (Version %s)");
}
const char* session_shape_description(unsigned type) {
    switch(type) {
    case 0:return "Square Spiral Inductor";
    case 1:return "Polygon Spiral Inductor";
    case 2:return "Wire";
    case 3:return "Transformer";
    case 4:return "Capacitor";
    case 5:return "Balun";
    case 10:return "3D Mirror Transformer";
    case 14:return "Via";
    case 15:return "Pad";
    case 16:return "Symmetric Square Inductor";
    case 17:return "Symmetric Polygon Inductor";
    case 18:return "Multi-Metal Square Spiral Inductor";
    default:return "User-defined Structure";
    }
}
}
namespace asitic {
namespace {
void session_copy(char* d,const char* s) { while((*d++=*s++)) {} }
long double session_float(const CommandArgument& argument) {
    float value;__builtin_memcpy(&value,&argument.value,4);long double out;
    asm volatile("flds %1; fstpt %0":"=m"(out):"m"(value):"st");return out;
}
long double session_double(const void* record,unsigned offset) {
    double value;__builtin_memcpy(&value,static_cast<const unsigned char*>(record)+offset,8);long double out;
    asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(value):"st");return out;
}
void session_put(void* record,unsigned offset,long double value) {
    volatile double out=value;__builtin_memcpy(static_cast<unsigned char*>(record)+offset,const_cast<const double*>(&out),8);
}
int session_int(const void* record,unsigned offset) { int out;__builtin_memcpy(&out,static_cast<const unsigned char*>(record)+offset,4);return out; }
void session_word(void* record,unsigned offset,unsigned value) { __builtin_memcpy(static_cast<unsigned char*>(record)+offset,&value,4); }
struct SessionFieldReader {
    SessionRecordHistory& history;const SessionGeometryParseOptions& options;unsigned index=0;
    bool get(unsigned argument,const char* key,unsigned type) {
        return parse_named_session_argument(history.line,history.argument[argument],key,type,options.commands,history.scans[index++]);
    }
    void warning(const char* text) { options.commands.warnings.emit(text); }
    void fail(SessionParseFailure error) { options.file.fail(error,options.file.user); }
};
}
bool parse_named_session_argument(const char* line,CommandArgument& argument,const char* key,unsigned type,
    const CommandRuntime& runtime,CommandParseHistory& history) {
    return extract_shape_argument(line,argument,key,type,runtime,history);
}
int lookup_session_metal(char* text,const SessionGeometryParseOptions& options,int& history) {
    const ShapePromptRuntime runtime{options.commands,options.technology,options.via_count,0.,0.,nullptr,nullptr};
    return find_shape_prompt_metal(text,runtime,history);
}
int lookup_session_via(char* text,const SessionGeometryParseOptions& options,int& history) {
    const ShapePromptRuntime runtime{options.commands,options.technology,options.via_count,0.,0.,nullptr,nullptr};
    return find_shape_prompt_via(text,runtime,history);
}
void parse_text_session_shape(GeometryShape& shape,void* file,SessionRecordHistory& history,const SessionGeometryParseOptions& options) {
    options.file.text.read_line(history.line,1024,file);normalize_input_line(history.line,0x8e,options.commands.characters);
    SessionFieldReader field{history,options};bool invalid=false;
    if(field.get(0,"TYPEID",6)) { session_word(&shape,0x50,history.argument[0].value);if(history.argument[0].value>99) { field.warning("Error:  Invalid spiral ID specified.");invalid=true; } }
    else { field.warning("Warning:  Missing Argument, must supply spiral ID.  Assuming <square>.");session_word(&shape,0x50,0); }
    if(field.get(0,"NAME",3))session_copy(reinterpret_cast<char*>(&shape),history.argument[0].text);
    else { field.warning("Error:  Missing Argument, must supply spiral name.");invalid=true; }
    if(field.get(0,"SIDES",6))session_word(&shape,0x64,history.argument[0].value);
    else { field.warning("Warning:  Missing argument, must supply no. of sides.");session_word(&shape,0x64,4); }
    if(field.get(0,"N",4))session_put(&shape,0x98,session_float(history.argument[0]));
    else { field.warning("Error:  Missing Argument, must supply no. of turns N.");invalid=true; }
    if(field.get(0,"W",7)) { const long double value=session_float(history.argument[0]);session_put(&shape,0x88,value);if(value>session_double(&options.chip_width,0) || value>session_double(&options.chip_height,0))field.warning("Warning:  The spiral width exceeds chip dimensions."); }
    else { field.warning("Error:  Missing Argument, must supply metal width W.");invalid=true; }
    if(field.get(0,"S",7)) { const long double value=session_float(history.argument[0]);session_put(&shape,0x90,value);if(session_double(&options.chip_width,0)/3.L<value || session_double(&options.chip_height,0)/3.L<value)field.warning("Warning:  The spiral width exceeds 1/3 chip dimensions."); }
    else { field.warning("Error:  Missing Argument, must supply metal spacing S.");invalid=true; }
    const bool length=field.get(0,"LEN",7),width=field.get(1,"WID",7),radius=field.get(2,"RAD",7);
    if(length) {
        session_put(&shape,0x70,session_float(history.argument[0]));session_put(&shape,0x78,session_float(history.argument[width?1:0]));
        const long double a=session_double(&shape,0x70),b=session_double(&shape,0x78);session_put(&shape,0x80,a);
        const long double dimension=b<a?a:b;const long double chip=options.chip_width<options.chip_height?session_double(&options.chip_width,0):session_double(&options.chip_height,0);
        if(chip<dimension)field.warning("Warning:  Spiral dimensions exceed chip dimension.");
    } else if(radius) {
        const long double value=session_float(history.argument[2]);session_put(&shape,0x80,value);session_put(&shape,0x78,value);session_put(&shape,0x70,value);
        const long double chip=options.chip_height>options.chip_width?session_double(&options.chip_width,0):session_double(&options.chip_height,0);
        if(chip*0.5L<session_double(&shape,0x80))field.warning("Warning:  Spiral dimensions exceed chip dimension.");
    } else { field.warning("Error  Missing Argument, must supply spiral dimension.");invalid=true; }
    int metal=field.get(0,"METAL",1)?lookup_session_metal(history.argument[0].text,options,history.metal_index):options.technology.metal_count-1;session_word(&shape,0x68,metal);
    if(metal<0 || metal>=options.technology.metal_count) { field.warning("Error:  Illegal metal layer.");invalid=true; }
    metal=field.get(0,"EXIT",1)?lookup_session_metal(history.argument[0].text,options,history.metal_index):options.technology.metal_count-2;session_word(&shape,0x6c,metal);
    if(metal<-1 || metal>=options.technology.metal_count) { field.warning("Error:  Illegal exit metal layer.");invalid=true; }
    const bool x=field.get(0,"XORG",7),y=field.get(1,"YORG",7);session_put(&shape,0x54,x?session_float(history.argument[0]):0.L);session_put(&shape,0x5c,y?session_float(history.argument[1]):0.L);
    if(session_double(&shape,0x54)<0.L || session_double(&shape,0x54)>session_double(&options.chip_width,0))field.warning("Warning:  Origin x coordinate out of range.");
    if(session_double(&shape,0x5c)<0.L || session_double(&shape,0x5c)>session_double(&options.chip_height,0))field.warning("Warning:  Origin y coordinate out of range.");
    session_put(&shape,0xa0,field.get(0,"ORIENT",4)?session_float(history.argument[0]):0.L);
    if(session_double(&shape,0xa0)<-360.L || session_double(&shape,0xa0)>360.L) { field.warning("Error:  Spiral orientation angle not in range [-360,360].");invalid=true; }
    session_word(&shape,0xac,field.get(0,"PHASE",6)?history.argument[0].value:1);if(!shape.attributes)shape.attributes=1;
    shape.suffix[0]=field.get(0,"HIDDEN",9)?static_cast<unsigned char>(history.argument[0].value):0;
    if(invalid && session_int(&shape,0x50)!=21)field.fail(SessionParseFailure::unable_to_create);
}
void parse_text_session_segment(const GeometryShape& shape,ConductorSegment& segment,void* file,SessionRecordHistory& history,const SessionGeometryParseOptions& options) {
    options.file.text.read_line(history.line,1024,file);normalize_input_line(history.line,0xae,options.commands.characters);
    SessionFieldReader field{history,options};bool invalid=false;
    const char* keys[6][2]={{"AX","AY"},{"BX","BY"},{"CX","CY"},{"DX","DY"},{"PORT1X","PORT1Y"},{"PORT2X","PORT2Y"}};
    const unsigned offsets[]={0x44,0x5c,0x88,0xa0,0,0x18};
    const char* warnings[]={"Error:  Must specify vertex A (x,y) coordinates.","Error:  Must specify vertex B (x,y) coordinates.","Error:  Must specify vertex C (x,y) coordinates.","Error:  Must specify vertex D (x,y) coordinates.","Error:  Must specify port 1 (x,y) coordinates.","Error:  Must specify port 2 (x,y) coordinates."};
    bool previous=false;
    for(unsigned i=0;i<6;++i) {
        const bool a=field.get(0,keys[i][0],4),b=field.get(1,keys[i][1],4);const bool good=i==5?previous:a&&b;
        if(good) { session_put(&segment,offsets[i],session_float(history.argument[0]));session_put(&segment,offsets[i]+8,session_float(history.argument[1])); }
        else { field.warning(warnings[i]);invalid=true; }
        previous=a&&b;
    }
    int metal;
    if(!field.get(0,"METAL",1))metal=options.technology.metal_count-1;
    else { metal=lookup_session_metal(history.argument[0].text,options,history.metal_index);if(metal==-1)metal=lookup_session_via(history.argument[0].text,options,history.via_index)+options.technology.metal_count;if(metal==-1)invalid=true; }
    const int shape_metal=session_int(&shape,0x68);if(shape_metal<0 || shape_metal>=options.technology.metal_count)metal=shape_metal;
    session_word(&segment,0xdc,metal);session_word(&segment,0x40,metal);session_word(&segment,0xc8,metal);session_word(&segment,0x84,metal);
    const bool width=field.get(0,"W",7);const long double w=width?session_float(history.argument[0]):session_double(&shape,0x88);
    session_put(&segment,0xcc,w);session_put(&segment,0x30,w);session_put(&segment,0x74,w);session_put(&segment,0xb8,w);
    const bool thickness=field.get(0,"T",7);const long double t=thickness?session_float(history.argument[0]):session_double(options.technology.metals+metal,0xb0);
    session_put(&segment,0xd4,t);session_put(&segment,0x38,t);session_put(&segment,0xc0,t);session_put(&segment,0x7c,t);
    const bool nw=field.get(0,"NX",6),nt=field.get(1,"NY",6);if(nw && nt) { session_word(&segment,0xe0,history.argument[0].value);session_word(&segment,0xe4,history.argument[1].value); }
    if(invalid)field.fail(SessionParseFailure::unable_to_create);
    if(metal<options.technology.metal_count) {
        const long double z=session_double(options.technology.metal_heights+metal,0);session_put(&segment,0x28,z);session_put(&segment,0x10,z);session_put(&segment,0x54,z);session_put(&segment,0x98,z);
    } else {
        const auto* via=options.technology.vias+(metal-options.technology.metal_count);const int a=session_int(via,0xcc),b=session_int(via,0xd0);
        const long double za=session_double(options.technology.metal_heights+a,0),zb=session_double(options.technology.metal_heights+b,0);
        session_put(&segment,0x10,za);session_put(&segment,0x54,za);session_put(&segment,0x98,za);session_put(&segment,0x28,zb);session_put(&segment,0x6c,zb);session_put(&segment,0xb0,zb);
    }
    const unsigned end=read_text_session_block(file,options.file);if(end!=0x8d && end!=0x8e)field.fail(SessionParseFailure::file_corrupted);
}
}
