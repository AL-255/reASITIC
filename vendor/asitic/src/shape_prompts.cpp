#include "asitic/shape_prompts.hpp"
namespace asitic {
namespace {
void copy(char* d,const char* s) { do { *d++=*s; }while(*s++); }
bool equal(const char* a,const char* b) { while(*a && *a==*b) { ++a;++b; }return *a==*b; }
unsigned length(const char* s) { unsigned n=0;while(s[n])++n;return n; }
long double load(const double& n) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(n):"st");return out; }
long double number(const CommandArgument& a) { long double out;asm volatile("flds %1; fstpt %0":"=m"(out):"m"(a.value):"st");return out; }
double& field(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
int& word(GeometryShape& s,unsigned offset) { return *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset); }
void warning(const ShapePromptRuntime& r,const char* s) { r.command.warnings.emit(s); }
void origin(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    const bool x=extract_shape_prompt_argument(line,h.temporary[0],"XORG",7,r.command,h);
    const bool y=extract_shape_prompt_argument(line,h.temporary[1],"YORG",7,r.command,h);
    field(out,0x54)=x?number(h.temporary[0]):0.L;
    if(y)field(out,0x5c)=number(h.temporary[1]);else field(out,0x5c)=load(field(out,0x88));
    if(load(field(out,0x54))<0.L || load(r.chip_x)<load(field(out,0x54)))warning(r,"Warning:  Origin x coordinate out of range.");
    if(load(field(out,0x5c))<0.L || load(r.chip_y)<load(field(out,0x5c)))warning(r,"Warning:  Origin y coordinate out of range.");
}
bool orientation(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    field(out,0xa0)=extract_shape_prompt_argument(line,h.temporary[0],"ORIENT",10,r.command,h)?number(h.temporary[0]):0.L;
    const long double angle=load(field(out,0xa0));
    if(angle< -360.L || angle>360.L) { warning(r,"Error:  Wire orientation angle not in range [-360,360].");return true; }return false;
}
void dimensions_warning(GeometryShape& out,const ShapePromptRuntime& r) {
    if(load(r.chip_x)<load(field(out,0x70)) || load(r.chip_y)<load(field(out,0x70)))warning(r,"Warning:  The spiral width exceeds 1/3 chip dimensions.");
    if(load(r.chip_x)<load(field(out,0x88)) || load(r.chip_y)<load(field(out,0x88)))warning(r,"Warning:  The spiral width exceeds 1/3 chip dimensions.");
}
bool name(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h,unsigned type) {
    const char* message=nullptr;
    if(!extract_shape_prompt_argument(line,h.temporary[0],"NAME",type,r.command,h))message="Error:  Missing Argument, must supply spiral name.";
    else { copy(reinterpret_cast<char*>(&out),h.temporary[0].text);
        if(find_geometry_shape(r.command.shapes,r.command.selected,reinterpret_cast<const char*>(&out)))message="Error:  The supplied sipral name is already in use.";
    }
    if(message) { warning(r,message);return true; }return false;
}
bool metal(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h,const char* key,unsigned offset,int default_value) {
    if(!extract_shape_prompt_argument(line,h.temporary[0],key,1,r.command,h)) { word(out,offset)=default_value;return false; }
    if(h.calls)h.layer=h.scan.integer;
    const int index=find_shape_prompt_metal(h.temporary[0].text,r,h.layer);word(out,offset)=index;
    if(index<0 || index>=r.technology.metal_count) { warning(r,"Error:  Illegal metal layer.");return true; }return false;
}
}
bool extract_shape_argument(const char* line,CommandArgument& out,const char* key,unsigned type,const CommandRuntime& r,CommandParseHistory& h) {
    if(!*line)return false;
    copy(r.message,line);char* token=r.token(r.message,":");
    char label[256];
    for(;;) {
        char* end=token;while(*end!='=' && *end)++end;
        const unsigned n=end-token;for(unsigned i=0;i<n;++i)label[i]=token[i];label[n]=0;
        if(equal(label,key))break;
        token=r.token(nullptr,":");if(!token)return false;
    }
    char* left=r.token(token,"=");const char* value=left+length(left)+1;
    if(!*value) { r.warnings.emit("Error:  Zero length right-hand ambiguous.");return false; }
    copy(out.text,value);return parse_command_argument(out,type,r,h);
}
bool extract_shape_prompt_argument(const char* line,CommandArgument& out,const char* key,unsigned type,const CommandRuntime& r,ShapePromptHistory& h) {
    if(h.calls)h.scan=h.calls[h.call_index];
    ++h.call_index;return extract_shape_argument(line,out,key,type,r,h.scan);
}
int find_shape_prompt_metal(char* name,const ShapePromptRuntime& r,int& history) {
    normalize_input_line(name,0xae,r.command.characters);
    if(r.command.characters.classification[static_cast<signed char>(*name)]&0x800) {
        r.command.scan(name,"%d",&history);return history>=0 && history<r.technology.metal_count?history:-1;
    }
    for(int i=0;i<r.technology.metal_count;++i)if(equal(name,reinterpret_cast<const char*>(r.technology.metals+i)))return i;
    return -1;
}
int find_shape_prompt_via(char* name,const ShapePromptRuntime& r,int& history) {
    normalize_input_line(name,0xae,r.command.characters);
    if(r.command.characters.classification[static_cast<signed char>(*name)]&0x800) {
        r.command.scan(name,"%d",&history);return history>=0 && history<r.via_count?history:-1;
    }
    for(int i=0;i<r.via_count;++i)if(equal(name,reinterpret_cast<const char*>(r.technology.vias+i)))return i;
    return -1;
}
void parse_wire_shape_arguments(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    normalize_input_line(line,0xae,r.command.characters);bool invalid=name(line,out,r,h,3);
    const bool len=extract_shape_prompt_argument(line,h.temporary[0],"LEN",7,r.command,h);
    const bool wid=extract_shape_prompt_argument(line,h.temporary[1],"WID",7,r.command,h);
    if(!len || !wid) { warning(r,"Error:  Missing Argument, must supply metal width and length.");invalid=true; }
    else {
        field(out,0x70)=number(h.temporary[0]);field(out,0x88)=number(h.temporary[1]);
        if(number(h.temporary[0])<load(1e-10) || number(h.temporary[1])<load(1e-10)) { warning(r,"Error:  Wire dimensions too small.");invalid=true; }
        dimensions_warning(out,r);
    }
    invalid=metal(line,out,r,h,"METAL",0x68,r.technology.metal_count-1)||invalid;word(out,0x6c)=-1;
    origin(line,out,r,h);invalid=orientation(line,out,r,h)||invalid;
    word(out,0xac)=extract_shape_prompt_argument(line,h.temporary[0],"PHASE",6,r.command,h)?static_cast<int>(h.temporary[0].value):1;
    if(!word(out,0xac))word(out,0xac)=1;
    word(out,0x64)=1;word(out,0x50)=2;
    const long double copied_length=load(field(out,0x70));field(out,0x80)=copied_length;field(out,0x78)=copied_length;
    field(out,0x90)=0.;field(out,0x98)=0.25;
    if(invalid)r.invalid(r.user);
}
void parse_capacitor_shape_arguments(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    normalize_input_line(line,0xae,r.command.characters);bool invalid=name(line,out,r,h,1);
    const bool len=extract_shape_prompt_argument(line,h.temporary[0],"LEN",7,r.command,h);
    const bool wid=extract_shape_prompt_argument(line,h.temporary[1],"WID",7,r.command,h);
    if(!len && !wid) { warning(r,"Error:  Missing Argument, must supply metal width and length.");invalid=true; }
    else {
        if(len)field(out,0x70)=number(h.temporary[0]);
        if(wid)field(out,0x88)=number(h.temporary[1]);
        if(!len)field(out,0x70)=load(field(out,0x88));
        if(!wid)field(out,0x88)=load(field(out,0x70));
        if(load(field(out,0x70))<load(1e-10) || load(field(out,0x88))<load(1e-10)) { warning(r,"Error:  Plate dimensions too small.");invalid=true; }
        dimensions_warning(out,r);
    }
    invalid=metal(line,out,r,h,"METAL1",0x68,r.technology.metal_count-1)||invalid;
    invalid=metal(line,out,r,h,"METAL2",0x6c,r.technology.metal_count-2)||invalid;
    origin(line,out,r,h);invalid=orientation(line,out,r,h)||invalid;
    word(out,0xac)=1;word(out,0x64)=1;word(out,0x50)=4;
    const long double copied_length=load(field(out,0x70));field(out,0x80)=copied_length;field(out,0x78)=copied_length;
    field(out,0x90)=0.;field(out,0x98)=0.;if(invalid)r.invalid(r.user);
}
namespace {
double binary64(long double value) { volatile double out=static_cast<double>(value);return out; }
double floor64(long double value) {
    unsigned short saved,down;double out;asm volatile("fnstcw %0":"=m"(saved));down=(saved&0xf3ffu)|0x400u;
    asm volatile("fldcw %1; fldt %2; frndint; fldcw %3; fstpl %0":"=m"(out):"m"(down),"m"(value),"m"(saved):"st");return out;
}
int integer(long double value) {
    unsigned short saved,truncate;int out;asm volatile("fnstcw %0":"=m"(saved));truncate=(saved&255u)|0xc00u;
    asm volatile("fldcw %1; fldt %2; fistpl %0; fldcw %3":"=m"(out):"m"(truncate),"m"(value),"m"(saved):"st");return out;
}
long double cosine(long double value) { long double out;asm volatile("fldt %1; fcos; fstpt %0":"=m"(out):"m"(value):"st");return out; }
long double quantize(long double n,int sides) {
    const double first=floor64(n),second=floor64(n);
    const double product=binary64((n-load(second))*static_cast<long double>(sides));
    const double last=floor64(load(product));
    return load(last)/static_cast<long double>(sides)+load(first);
}
bool winding_parameter(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h,const char* key,unsigned offset,const char* error) {
    if(!extract_shape_prompt_argument(line,h.temporary[0],key,7,r.command,h)) { warning(r,error);return true; }
    const long double value=number(h.temporary[0]);field(out,offset)=value;
    if(load(r.chip_x)/3.L<value || load(r.chip_y)/3.L<value)warning(r,"Warning:  The spiral width exceeds 1/3 chip dimensions.");
    return false;
}
void spiral_origin(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    const bool x=extract_shape_prompt_argument(line,h.temporary[0],"XORG",7,r.command,h);
    const bool y=extract_shape_prompt_argument(line,h.temporary[1],"YORG",7,r.command,h);
    field(out,0x54)=x?number(h.temporary[0]):0.L;field(out,0x5c)=y?number(h.temporary[1]):0.L;
    if(load(field(out,0x54))<0.L || load(r.chip_x)<load(field(out,0x54)))warning(r,"Warning:  Origin x coordinate out of range.");
    if(load(field(out,0x5c))<0.L || load(r.chip_y)<load(field(out,0x5c)))warning(r,"Warning:  Origin y coordinate out of range.");
}
bool spiral_orientation(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    field(out,0xa0)=extract_shape_prompt_argument(line,h.temporary[0],"ORIENT",10,r.command,h)?number(h.temporary[0]):0.L;
    const long double angle=load(field(out,0xa0));
    if(angle< -360.L || angle>360.L) { warning(r,"Error:  Spiral orientation angle not in range [-360,360].");return true; }return false;
}
void phase(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    word(out,0xac)=extract_shape_prompt_argument(line,h.temporary[0],"PHASE",6,r.command,h)?static_cast<int>(h.temporary[0].value):1;
    if(!word(out,0xac))word(out,0xac)=1;
}
bool maximum_turns(GeometryShape& out,const ShapePromptRuntime& r,long double maximum) {
    const long double limit=quantize(maximum,word(out,0x64));
    const bool invalid=limit<load(field(out,0x98));
    if(invalid) {
        r.command.format(r.command.message,"Error:  Spiral of given dimensions cannot support more than %5.2f turns.",binary64(limit));warning(r,r.command.message);
    }
    field(out,0x98)=quantize(load(field(out,0x98)),word(out,0x64));return invalid;
}
}
void parse_square_shape_arguments(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    word(out,0x64)=4;normalize_input_line(line,0xae,r.command.characters);
    bool invalid=false;
    if(!extract_shape_prompt_argument(line,h.temporary[0],"NAME",3,r.command,h)) { warning(r,"Error:  Missing Argument, must supply spiral name.");invalid=true; }
    else {
        copy(reinterpret_cast<char*>(&out),h.temporary[0].text);
        if(find_geometry_shape(r.command.shapes,r.command.selected,reinterpret_cast<const char*>(&out))) { warning(r,"Error:  The supplied spiral name is already in use.");invalid=true; }
    }
    if(!extract_shape_prompt_argument(line,h.temporary[0],"N",4,r.command,h)) { warning(r,"Error:  Missing Argument, must supply no. of turns N.");invalid=true; }
    else field(out,0x98)=number(h.temporary[0]);
    invalid=winding_parameter(line,out,r,h,"W",0x88,"Error:  Missing Argument, must supply metal width W.")||invalid;
    invalid=winding_parameter(line,out,r,h,"S",0x90,"Error:  Missing Argument, must supply metal spacing S.")||invalid;
    const bool len=extract_shape_prompt_argument(line,h.temporary[0],"LEN",7,r.command,h);
    const bool wid=extract_shape_prompt_argument(line,h.temporary[1],"WID",7,r.command,h);
    const bool ilen=extract_shape_prompt_argument(line,h.temporary[2],"ILEN",7,r.command,h);
    const bool iwid=extract_shape_prompt_argument(line,h.temporary[3],"IWID",7,r.command,h);
    if(len) { const long double v=number(h.temporary[0]);field(out,0x70)=v;field(out,0x78)=wid?number(h.temporary[1]):v; }
    else if(!ilen) { warning(r,"Error  Missing Argument, must supply spiral dimension.");invalid=true; }
    else {
        const long double v=number(h.temporary[2]);field(out,0x70)=v;field(out,0x78)=iwid?number(h.temporary[3]):v;
        const long double n=load(field(out,0x98));const double floor=floor64(n),fraction=binary64(n-load(floor));
        const double biased=binary64(load(1e-10)+load(fraction));const int part=integer(4.L*load(biased));
        const double floored=floor64(n);const int base=integer(load(floored)+load(floored));
        int nx=base,ny=base;
        if(part==1)ny=static_cast<int>(static_cast<unsigned>(ny)+1u);
        else if(part==2)nx=ny=static_cast<int>(static_cast<unsigned>(base)+1u);
        else if(part==3) { nx=static_cast<int>(static_cast<unsigned>(base)+1u);ny=static_cast<int>(static_cast<unsigned>(base)+2u); }
        const int sx=static_cast<int>(static_cast<unsigned>(nx)-2u),sy=static_cast<int>(static_cast<unsigned>(ny)-2u);
        const long double w=load(field(out,0x88)),s=load(field(out,0x90));
        field(out,0x70)=(w*static_cast<long double>(nx)+s*static_cast<long double>(sx))+load(field(out,0x70));
        field(out,0x78)=(w*static_cast<long double>(ny)+s*static_cast<long double>(sy))+load(field(out,0x78));
    }
    const long double x=load(field(out,0x70)),y=load(field(out,0x78));const long double largest=y<x?x:y;
    const long double cx=load(r.chip_x),cy=load(r.chip_y),smallest=cy>cx?cx:cy;
    if(smallest<largest)warning(r,"Warning:  Spiral dimensions exceed chip dimension.");
    if(!invalid) {
        const long double spacing=load(field(out,0x90)),den=spacing+load(field(out,0x88));
        const long double maximum=((spacing+load(field(out,0x70)))/den+(spacing+load(field(out,0x78)))/den)*0.25L;
        invalid=maximum_turns(out,r,maximum);
    }
    if(!extract_shape_prompt_argument(line,h.temporary[0],"METAL",1,r.command,h))word(out,0x68)=r.technology.metal_count-1;
    else { if(h.calls)h.layer=h.scan.integer;word(out,0x68)=find_shape_prompt_metal(h.temporary[0].text,r,h.layer); }
    if(word(out,0x68)<0 || word(out,0x68)>=r.technology.metal_count) { warning(r,"Error:  Illegal metal layer.");invalid=true; }
    if(!extract_shape_prompt_argument(line,h.temporary[0],"EXIT",1,r.command,h))word(out,0x6c)=r.technology.metal_count-2;
    else { if(h.calls)h.layer=h.scan.integer;word(out,0x6c)=find_shape_prompt_metal(h.temporary[0].text,r,h.layer); }
    if(word(out,0x6c)< -1 || word(out,0x6c)>=r.technology.metal_count) { warning(r,"Error:  Illegal exit metal layer.");invalid=true; }
    spiral_origin(line,out,r,h);invalid=spiral_orientation(line,out,r,h)||invalid;phase(line,out,r,h);
    word(out,0x50)=0;field(out,0x80)=load(field(out,0x70));
    if(invalid)r.invalid(r.user);
}
void parse_polygon_shape_arguments(char* line,GeometryShape& out,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    normalize_input_line(line,0xae,r.command.characters);bool invalid=name(line,out,r,h,3);
    const bool turns=extract_shape_prompt_argument(line,h.temporary[0],"N",4,r.command,h);
    if(!turns) { warning(r,"Error:  Missing Argument, must supply no. of turns N.");invalid=true; }else field(out,0x98)=number(h.temporary[0]);
    invalid=winding_parameter(line,out,r,h,"W",0x88,"Error:  Missing Argument, must supply metal width W.")||invalid;
    invalid=winding_parameter(line,out,r,h,"S",0x90,"Error:  Missing Argument, must supply metal spacing S.")||invalid;
    if(!extract_shape_prompt_argument(line,h.temporary[0],"SIDES",6,r.command,h))word(out,0x64)=16;
    else {
        word(out,0x64)=static_cast<int>(h.temporary[0].value);
        if(word(out,0x64)==4)warning(r,"Note:  The Square command also produces square spirals.");
        if(word(out,0x64)<3) { warning(r,"Error:  Bad Argument, must have at least 3 sides to make a spiral.");invalid=true; }
    }
    bool radius=extract_shape_prompt_argument(line,h.temporary[0],"RAD",7,r.command,h);
    if(!radius)radius=extract_shape_prompt_argument(line,h.temporary[0],"R",7,r.command,h);
    if(!radius)radius=extract_shape_prompt_argument(line,h.temporary[0],"RADIUS",7,r.command,h);
    if(!radius) { warning(r,"Error:  Missing Argument, must specify spiral radius.");invalid=true; }
    else {
        const long double v=number(h.temporary[0]);field(out,0x80)=v;
        const long double cx=load(r.chip_x),cy=load(r.chip_y),smallest=cx<cy?cx:cy;
        if(smallest<v+v) { warning(r,"Error:  Spiral dimensions exceed chip dimension.");invalid=true; }
    }
    const double pi=0x1.921fb54442d18p+1;
    if(turns && !invalid) {
        const long double maximum=cosine(load(pi)/static_cast<long double>(word(out,0x64)))*load(field(out,0x80))/(load(field(out,0x90))+load(field(out,0x88)));
        invalid=maximum_turns(out,r,maximum);
    }
    invalid=metal(line,out,r,h,"METAL",0x68,r.technology.metal_count-1)||invalid;word(out,0x6c)=-1;
    spiral_origin(line,out,r,h);invalid=spiral_orientation(line,out,r,h)||invalid;phase(line,out,r,h);
    const long double cosine_value=cosine(load(pi)/static_cast<long double>(word(out,0x64)));
    word(out,0x50)=1;const double length=binary64((cosine_value+cosine_value)*load(field(out,0x80)));
    __builtin_memcpy(&field(out,0x78),&length,8);__builtin_memcpy(&field(out,0x70),&length,8);
    if(invalid)r.invalid(r.user);
}
}
