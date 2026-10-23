#include "asitic/text_sessions.hpp"
namespace asitic {
namespace {
unsigned word(const void* record,unsigned offset) {
    unsigned value;__builtin_memcpy(&value,static_cast<const unsigned char*>(record)+offset,4);return value;
}
int integer(const void* record,unsigned offset) {
    int value;__builtin_memcpy(&value,static_cast<const unsigned char*>(record)+offset,4);return value;
}
void number(void* file,const char* format,const void* record,unsigned offset,const TextSessionOutput& out) {
    out.print(file,format,word(record,offset),word(record,offset+4));
}
const char* layer(const TextSessionLayers& layers,int index) {
    const unsigned address=index<layers.metal_count?
        reinterpret_cast<unsigned>(layers.metals)+static_cast<unsigned>(index)*236u:
        reinterpret_cast<unsigned>(layers.vias)+(static_cast<unsigned>(index)-static_cast<unsigned>(layers.metal_count))*240u;
    return reinterpret_cast<const char*>(address);
}
void path(char* destination,const char* source) {
    unsigned length=0;for(;;) { destination[length]=source[length];if(!source[length])break;++length; }
    bool found=false;
    for(unsigned i=0;i<length;++i)if(source[i]=='.' && source[i+1]=='s' && source[i+2]=='p' && source[i+3]=='i') { found=true;break; }
    if(!found) { destination[length++]='.';destination[length++]='s';destination[length++]='p';destination[length++]='i';destination[length]=0; }
}
}
void write_text_session_shape_fields(const GeometryShape& shape,void* file,const TextSessionLayers& layers,const TextSessionOutput& out) {
    const void* record=&shape;
    out.print(file,"\nNAME=%s:",reinterpret_cast<const char*>(&shape));
    out.print(file,"TYPEID=%d:",integer(record,0x50));
    number(file,"XORG=%lg:",record,0x54,out);
    number(file,"YORG=%lg:",record,0x5c,out);
    out.print(file,"SIDES=%d:",integer(record,0x64));
    const int metal=integer(record,0x68);
    if(metal>=0 && metal<layers.metal_count)out.print(file,"METAL=%s:",layer(layers,metal));
    const int exit=integer(record,0x6c);
    if(exit>=0 && exit<layers.metal_count)out.print(file,"EXIT=%s:",layer(layers,exit));
    const char* formats[]={"LEN=%lg:","WID=%lg:","RAD=%lg:","W=%lg:","S=%lg:","N=%lg:","ORIENT=%lg:"};
    for(unsigned i=0;i<7;++i)number(file,formats[i],record,0x70+8*i,out);
    out.print(file,"PHASE=%d:",integer(record,0xac));
    out.print(file,shape.suffix[0]?"HIDDEN=True":"HIDDEN=False");
}
void write_text_session_segment(const ConductorSegment& segment,void* file,const TextSessionLayers& layers,const TextSessionOutput& out) {
    const unsigned offsets[]={0x44,0x4c,0x5c,0x64,0x88,0x90,0xa0,0xa8,0,8,0x18,0x20};
    const char* formats[]={"\nAX=%lf:","AY=%lf:","BX=%lf:","BY=%lf:","CX=%lf:","CY=%lf:","DX=%lf:","DY=%lf:","PORT1X=%lf:","PORT1Y=%lf:","PORT2X=%lf:","PORT2Y=%lf:"};
    for(unsigned i=0;i<12;++i)number(file,formats[i],&segment,offsets[i],out);
    out.print(file,"METAL=%s:",layer(layers,integer(&segment,0xdc)));
    number(file,"W=%lf:",&segment,0xcc,out);
    number(file,"T=%lf:",&segment,0xd4,out);
    out.print(file,"NX=%d:",integer(&segment,0xe0));
    out.print(file,"NY=%d:",integer(&segment,0xe4));
}
void write_text_session_shape(const GeometryShape& shape,void* file,const TextSessionLayers& layers,const TextSessionOutput& out) {
    out.print(file,"\nBLOCK=SPIRAL");
    write_text_session_shape_fields(shape,file,layers,out);
    for(const ConductorSegment* segment=shape.filaments.first;segment;segment=reinterpret_cast<const ConductorSegment*>(segment->next_address)) {
        out.print(file,"\nBLOCK=SEGMENT");write_text_session_segment(*segment,file,layers,out);out.print(file,"\nBLOCK=ENDSEGMENT");
        for(const ConductorSegment* shunt=reinterpret_cast<const ConductorSegment*>(segment->group_next_address);shunt;shunt=reinterpret_cast<const ConductorSegment*>(shunt->group_next_address)) {
            out.print(file,"\nBLOCK=SHUNT");write_text_session_segment(*shunt,file,layers,out);out.print(file,"\nBLOCK=ENDSHUNT");
        }
    }
    out.print(file,"\nBLOCK=ENDSPIRAL");
}
void write_text_session_version(void* file,const TextSessionOutput& out) {
    out.print(file,"\nASITIC %s","10.20.99.0.0.0");
}
void open_text_session_input(const char* filename,void*& file,SessionHeaderHistory& history,const TextSessionRuntime& runtime) {
    char filename_buffer[80];path(filename_buffer,filename);
    void* input=runtime.files.open(filename_buffer,"r");
    if(!input)runtime.fail(TextSessionFailure::unable_to_read,filename_buffer,runtime.user);
    validate_text_session_header(input,history,runtime.parsing);
    file=input;
}
}
