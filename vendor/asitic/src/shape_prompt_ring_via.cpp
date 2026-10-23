#include "asitic/shape_prompts.hpp"
namespace asitic {
namespace {
double& rv_double(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
int& rv_word(GeometryShape& s,unsigned offset) { return *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset); }
void rv_copy(char* destination,const char* source) { do { *destination++=*source; }while(*source++); }
long double rv_load(const double& value) { long double result;asm volatile("fldl %1; fstpt %0":"=m"(result):"m"(value):"st");return result; }
long double rv_number(const CommandArgument& value) { long double result;asm volatile("flds %1; fstpt %0":"=m"(result):"m"(value.value):"st");return result; }
long double rv_height(const double& upper,const double& lower) { long double result;asm volatile("fldl %1; fsubl %2; fstpt %0":"=m"(result):"m"(upper),"m"(lower):"st");return result; }
void rv_warning(const ShapePromptRuntime& r,const char* message) { r.command.warnings.emit(message); }
bool rv_extract(char* line,ShapePromptHistory& h,unsigned record,const char* key,unsigned type,const ShapePromptRuntime& r) {
    return extract_shape_prompt_argument(line,h.temporary[record],key,type,r.command,h);
}
bool rv_name(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h,bool ring) {
    const char* message=nullptr;
    if(!rv_extract(line,h,0,"NAME",3,r))message=ring?"Error:  Missing Argument, must supply ring name.":"Error:  Missing Argument, must supply spiral name.";
    else {
        rv_copy(reinterpret_cast<char*>(&shape),h.temporary[0].text);
        if(find_geometry_shape(r.command.shapes,r.command.selected,reinterpret_cast<const char*>(&shape)))message=ring?"Error:  The supplied name is already in use.":"Error:  The supplied sipral name is already in use.";
    }
    if(message) { rv_warning(r,message);return true; }return false;
}
void rv_origin(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h,bool ring) {
    const bool x=rv_extract(line,h,0,"XORG",7,r);
    const bool y=rv_extract(line,h,1,"YORG",7,r);
    rv_double(shape,0x54)=x?rv_number(h.temporary[0]):0.L;
    rv_double(shape,0x5c)=y?rv_number(h.temporary[1]):ring?0.L:rv_load(rv_double(shape,0x88));
    if(rv_load(rv_double(shape,0x54))<0.L || rv_load(r.chip_x)<rv_load(rv_double(shape,0x54)))rv_warning(r,"Warning:  Origin x coordinate out of range.");
    if(rv_load(rv_double(shape,0x5c))<0.L || rv_load(r.chip_y)<rv_load(rv_double(shape,0x5c)))rv_warning(r,"Warning:  Origin y coordinate out of range.");
}
}
void parse_ring_shape_arguments(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    normalize_input_line(line,0xae,r.command.characters);bool invalid=rv_name(line,shape,r,h,true);
    if(rv_extract(line,h,0,"W",7,r)) {
        const long double width=rv_number(h.temporary[0]);rv_double(shape,0x88)=width;
        if(rv_load(r.chip_x)/3.L<width || rv_load(r.chip_y)/3.L<width)rv_warning(r,"Warning:  The ring width exceeds 1/3 chip dimensions.");
    } else { rv_warning(r,"Error:  Missing Argument, must supply metal width W.");invalid=true; }
    if(rv_extract(line,h,0,"GAP",10,r))rv_double(shape,0x90)=rv_number(h.temporary[0]);
    else { rv_warning(r,"Error:  Missing Argument, must supply gap angle.");invalid=true; }
    if(rv_extract(line,h,0,"RAD",7,r) || rv_extract(line,h,0,"R",7,r) || rv_extract(line,h,0,"RADIUS",7,r)) {
        const long double radius=rv_number(h.temporary[0]);rv_double(shape,0x80)=radius;
        const long double extent=radius+radius;
        const long double x=rv_load(r.chip_x),y=rv_load(r.chip_y);
        if((x<y?x:y)<extent) { rv_warning(r,"Error:  Ring dimensions exceed chip dimension.");invalid=true; }
    } else { rv_warning(r,"Error:  Missing Argument, must specify ring radius.");invalid=true; }
    if(rv_extract(line,h,0,"SIDES",6,r)) {
        rv_word(shape,0x64)=static_cast<int>(h.temporary[0].value);
        if(rv_word(shape,0x64)<3) { rv_warning(r,"Error:  Bad Argument, must have at least 3 sides to make a ring.");invalid=true; }
    } else rv_word(shape,0x64)=16;
    if(rv_extract(line,h,0,"METAL",1,r)) {
        if(h.calls)h.layer=h.scan.integer;
        const int index=find_shape_prompt_metal(h.temporary[0].text,r,h.layer);rv_word(shape,0x68)=index;
        if(index<0 || index>=r.technology.metal_count) { rv_warning(r,"Error:  Illegal metal layer.");invalid=true; }
    } else rv_word(shape,0x68)=r.technology.metal_count-1;
    rv_word(shape,0x6c)=-1;rv_origin(line,shape,r,h,true);
    rv_double(shape,0xa0)=rv_extract(line,h,0,"ORIENT",10,r)?rv_number(h.temporary[0]):0.L;
    const long double angle=rv_load(rv_double(shape,0xa0));
    if(angle< -360.L || 360.L<angle) { rv_warning(r,"Error:  Spiral orientation angle not in range [-360,360].");invalid=true; }
    rv_word(shape,0xac)=rv_extract(line,h,0,"PHASE",6,r)?static_cast<int>(h.temporary[0].value):1;
    if(!rv_word(shape,0xac))rv_word(shape,0xac)=1;
    const long double radius=rv_load(rv_double(shape,0x80));const long double diameter=radius+radius;
    rv_word(shape,0x50)=22;rv_double(shape,0x78)=diameter;rv_double(shape,0x70)=diameter;
    if(invalid)r.invalid(r.user);
}
void parse_via_shape_arguments(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    normalize_input_line(line,0xae,r.command.characters);bool invalid=rv_name(line,shape,r,h,false);
    const bool nx=rv_extract(line,h,0,"NX",6,r) || rv_extract(line,h,0,"N",6,r);
    const bool ny=rv_extract(line,h,1,"NY",6,r);
    if(nx) { rv_word(shape,0x64)=static_cast<int>(h.temporary[0].value);rv_word(shape,0x6c)=static_cast<int>(h.temporary[ny?1:0].value); }
    else { rv_warning(r,"Error:  Missing Argument, must supply no. of vias.");invalid=true; }
    int via=h.layer;
    if(rv_extract(line,h,0,"VIA",1,r)) {
        if(h.calls)h.layer=h.scan.integer;
        via=find_shape_prompt_via(h.temporary[0].text,r,h.layer);
        if(via<0 || via>=r.via_count) { rv_warning(r,"Error:  Illegal via layer.");invalid=true; }
    } else { rv_warning(r,"Error:  Must specify a via layer name.");invalid=true; }
    rv_origin(line,shape,r,h,false);
    rv_word(shape,0xac)=rv_extract(line,h,0,"PHASE",4,r)?static_cast<int>(h.temporary[0].value):1;
    if(invalid)r.invalid(r.user);
    rv_word(shape,0x50)=14;
    const auto* record=reinterpret_cast<const unsigned char*>(r.technology.vias+via);
    const int first=*reinterpret_cast<const int*>(record+0xcc),second=*reinterpret_cast<const int*>(record+0xd0);
    const long double height=rv_height(r.technology.metal_heights[second],r.technology.metal_heights[first]);
    const long double width=rv_load(*reinterpret_cast<const double*>(record+0xa0));
    rv_double(shape,0x78)=width;rv_double(shape,0x90)=0.;rv_double(shape,0x88)=width;
    rv_double(shape,0x98)=0.25;rv_double(shape,0xa0)=0.;rv_double(shape,0x70)=height;rv_double(shape,0x80)=height*0.5L;
    rv_word(shape,0x68)=via+r.technology.metal_count;
}
}
