#include "asitic/shape_prompts.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
volatile unsigned& word(GeometryShape& s,unsigned o) { return *reinterpret_cast<volatile unsigned*>(reinterpret_cast<unsigned char*>(&s)+o); }
double& real(GeometryShape& s,unsigned o) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+o); }
char* name(GeometryShape& s) { return reinterpret_cast<char*>(&s); }
void copy(char* d,const char* s) { do { *d=*s;++d; }while(*s++); }
void append(char* d,const char* s) { while(*d)++d;copy(d,s); }
long double load(const double& x) { long double y;asm volatile("fldl %1; fstpt %0":"=m"(y):"m"(x):"st");return y; }
long double argument(const CommandArgument& a) { long double y;asm volatile("flds %1; fstpt %0":"=m"(y):"m"(a.value):"st");return y; }
void store(GeometryShape& s,unsigned o,long double x) { real(s,o)=x87::binary64(x); }
void floating_copy(GeometryShape& d,unsigned od,GeometryShape& s,unsigned os) { store(d,od,load(real(s,os))); }
double floor64(long double value) {
    unsigned short saved,down;long double result;
    asm volatile("fnstcw %0":"=m"(saved));down=(saved&0xf3ffu)|0x400u;
    asm volatile("fldcw %2; frndint; fldcw %3":"=t"(result):"0"(value),"m"(down),"m"(saved));
    return x87::binary64(result);
}
long double quantize(long double value,int sides) {
    const double first=floor64(value),second=floor64(value);
    const double product=x87::binary64((value-load(second))*sides);
    const double fraction=floor64(load(product));
    return load(fraction)/sides+load(first);
}
void warning(const ShapePromptRuntime& r,const char* message) { r.command.warnings.emit(message); }
bool exists(const ShapePromptRuntime& r,const GeometryShape& s) { return find_geometry_shape(r.command.shapes,r.command.selected,reinterpret_cast<const char*>(&s)); }
bool select(const char* line,ShapePromptHistory& h,unsigned temporary,const char* key,unsigned type,const ShapePromptRuntime& r) {
    return extract_shape_prompt_argument(line,h.temporary[temporary],key,type,r.command,h);
}
int select_metal(ShapePromptHistory& h,const ShapePromptRuntime& r) { if(h.calls)h.layer=h.scan.integer;return find_shape_prompt_metal(h.temporary[0].text,r,h.layer); }
bool exceeds_third(long double value,const ShapePromptRuntime& r) { return load(r.chip_x)/3.L<value || load(r.chip_y)/3.L<value; }
bool chip_exceeded(GeometryShape& s,const ShapePromptRuntime& r) {
    const long double length=load(real(s,0x70)),width=load(real(s,0x78));
    const long double largest=width<length?length:width;
    const long double x=load(r.chip_x),y=load(r.chip_y),smallest=y>x?x:y;
    return smallest<largest;
}
long double capacity(GeometryShape& s,bool balun=false) {
    const long double length=load(real(s,0x70)),width=load(real(s,0x78));
    const long double dimension=balun?length:(width>length?length:width);
    const long double sum=load(real(s,0x88))+load(real(s,0x90));
    return quantize(dimension/(sum+sum)-0.25L,static_cast<int>(word(s,0x64)));
}
void check_turns(GeometryShape& s,const ShapePromptRuntime& r,const char* message,bool& invalid,bool balun=false) {
    const long double maximum=capacity(s,balun);
    if(load(real(s,0x98))>maximum) {
        r.command.format(r.command.message,message,x87::binary64(maximum));warning(r,r.command.message);invalid=true;
    }
    store(s,0x98,quantize(load(real(s,0x98)),static_cast<int>(word(s,0x64))));
}
void origins(char* line,GeometryShape& s,const ShapePromptRuntime& r,ShapePromptHistory& h,bool& invalid) {
    const bool x=select(line,h,0,"XORG",7,r),y=select(line,h,1,"YORG",7,r);
    if(x)store(s,0x54,argument(h.temporary[0]));else real(s,0x54)=0.;
    if(y)store(s,0x5c,argument(h.temporary[1]));else real(s,0x5c)=0.;
    if(load(real(s,0x54))<0.L || load(real(s,0x54))>load(r.chip_x))warning(r,"Warning:  Origin x coordinate out of range.");
    if(load(real(s,0x5c))<0.L || load(real(s,0x5c))>load(r.chip_y))warning(r,"Warning:  Origin y coordinate out of range.");
    if(select(line,h,0,"ORIENT",10,r))store(s,0xa0,argument(h.temporary[0]));else real(s,0xa0)=0.;
    const long double angle=load(real(s,0xa0));
    if(angle< -360.L || angle>360.L) { warning(r,"Error:  Spiral orientation angle not in range [-360,360].");invalid=true; }
}
}
void parse_transformer_shape_arguments(char* line,GeometryShape& primary,GeometryShape& secondary,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    bool invalid=false;word(secondary,0x64)=4;word(primary,0x64)=4;word(secondary,0x50)=3;word(primary,0x50)=3;
    normalize_input_line(line,0xae,r.command.characters);
    if(!select(line,h,0,"PNAME",3,r) && !select(line,h,0,"NAME",3,r)) { warning(r,"Error:  Missing PNAME Argument, must supply primary spiral name.");invalid=true; }
    else copy(name(primary),h.temporary[0].text);
    if(exists(r,primary)) { warning(r,"Error:  The supplied primary spiral name is already in use.");invalid=true; }
    if(!select(line,h,0,"SNAME",3,r)) {
        copy(name(secondary),name(primary));append(name(primary),"-p");append(name(secondary),"-s");
        if(exists(r,secondary) || exists(r,primary)) { warning(r,"Error:  The supplied spiral name is already in use.");invalid=true; }
    } else {
        copy(name(secondary),h.temporary[0].text);
        if(exists(r,secondary)) { warning(r,"Error:  The supplied secondary spiral name is already in use.");invalid=true; }
    }
    if(!select(line,h,0,"NP",4,r) && !select(line,h,0,"N",4,r)) { warning(r,"Error:  Missing N Argument, must supply no. of turns.");invalid=true; }
    else store(primary,0x98,argument(h.temporary[0]));
    if(select(line,h,0,"NS",4,r))store(secondary,0x98,argument(h.temporary[0]));else floating_copy(secondary,0x98,primary,0x98);
    if(!select(line,h,0,"WP",7,r) && !select(line,h,0,"W",7,r)) { warning(r,"Error:  Missing W Argument, must supply primary metal width.");invalid=true; }
    else { const long double width=argument(h.temporary[0]);store(primary,0x88,width);if(exceeds_third(width,r))warning(r,"Warning:  The  metal width exceeds 1/3 chip dimensions."); }
    const bool supplied_secondary=select(line,h,0,"WS",7,r);floating_copy(secondary,0x88,primary,0x88);
    if(supplied_secondary)warning(r,"Warning:  WS!=WP not currently supported.  Taking WS=WP.");
    if(exceeds_third(load(real(secondary,0x88)),r))warning(r,"Warning:  The  metal width exceeds 1/3 chip dimensions.");
    if(!select(line,h,0,"S",7,r)) { warning(r,"Error:  Missing S Argument, must supply metal spacing.");invalid=true; }
    else {
        const long double space=argument(h.temporary[0]);
        const long double first=space+space+load(real(secondary,0x88)),second=space+space+load(real(primary,0x88));
        store(primary,0x90,first);store(secondary,0x90,second);
        if(exceeds_third(load(real(primary,0x90)),r))warning(r,"Warning:  The  spacing exceeds 1/3 chip dimensions.");
    }
    const bool length=select(line,h,0,"LEN",7,r),width=select(line,h,1,"WID",7,r);
    if(!length) { warning(r,"Error:  Missing LEN Argument, must supply outer dimension.");invalid=true; }
    else {
        const long double l=argument(h.temporary[0]),w=width?argument(h.temporary[1]):l;
        store(primary,0x70,l);store(secondary,0x70,l);store(primary,0x78,w);store(secondary,0x78,w);
    }
    if(chip_exceeded(primary,r)) { warning(r,"Error:  Spiral dimensions exceed chip dimension.");invalid=true; }
    if(!invalid) {
        check_turns(primary,r,"Error:  Primary Spiral of given dimensions cannot support more than %5.2f turns.",invalid);
        check_turns(secondary,r,"Error:  Secondary Spiral of given dimensions cannot support more than %5.2f turns.",invalid);
    }
    if(!select(line,h,0,"METAL",1,r))word(secondary,0x68)=word(primary,0x68)=r.technology.metal_count-1;
    else {
        const int metal=select_metal(h,r);word(primary,0x68)=word(secondary,0x68)=metal;
        if(metal<0 || metal>=r.technology.metal_count) { warning(r,"Error:  Illegal metal layer.");invalid=true; }
    }
    if(!select(line,h,0,"EXIT",1,r))word(secondary,0x6c)=word(primary,0x6c)=r.technology.metal_count-2;
    else word(secondary,0x6c)=word(primary,0x6c)=select_metal(h,r);
    const int exit=word(primary,0x6c);
    if(exit< -1 || exit>=r.technology.metal_count || word(primary,0x6c)==word(primary,0x68)) { warning(r,"Error:  Illegal exit metal layer.");invalid=true; }
    origins(line,primary,r,h,invalid);
    if(select(line,h,0,"PHASE",6,r))word(secondary,0xac)=word(primary,0xac)=h.temporary[0].value;
    else word(secondary,0xac)=word(primary,0xac)=1;
    if(!word(primary,0xac))word(secondary,0xac)=word(primary,0xac)=1;
    if(invalid) { r.invalid(r.user);__builtin_unreachable(); }
    const long double outer=load(real(primary,0x70));store(secondary,0x80,outer);store(primary,0x80,outer);
    floating_copy(secondary,0x54,primary,0x54);floating_copy(secondary,0x5c,primary,0x5c);floating_copy(secondary,0xa0,primary,0xa0);
}
void parse_balun_shape_arguments(char* line,GeometryShape& primary,GeometryShape& secondary,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    bool invalid=false;word(secondary,0x64)=4;word(primary,0x64)=4;
    normalize_input_line(line,0xae,r.command.characters);
    if(!select(line,h,0,"NAME",3,r)) { warning(r,"Error:  Missing Argument, must supply balun name.");invalid=true; }
    else {
        copy(name(primary),h.temporary[0].text);append(name(primary),"-P");copy(name(secondary),h.temporary[0].text);append(name(secondary),"-S");
        if(exists(r,primary)) { warning(r,"Error:  The supplied primary spiral name isalready in use.");invalid=true; }
        if(exists(r,secondary)) { warning(r,"Error:  The supplied secondary spiral name is already in use.");invalid=true; }
    }
    if(!select(line,h,0,"N",4,r)) { warning(r,"Error:  Missing Argument, must supply no. of turns N.");invalid=true; }
    else { const double n=floor64((argument(h.temporary[0])+1.L)*0.5L);store(primary,0x98,load(n));store(secondary,0x98,load(n)-1.L); }
    if(!select(line,h,0,"W1",7,r)) { warning(r,"Error:  Missing Argument, must supply primary metal width.");invalid=true; }
    else { const long double width=argument(h.temporary[0]);store(primary,0x88,width);if(exceeds_third(width,r))warning(r,"Warning:  The primary metal width exceeds 1/3 chip dimensions."); }
    if(!select(line,h,0,"W2",7,r))floating_copy(secondary,0x88,primary,0x88);
    else { const long double width=argument(h.temporary[0]);store(secondary,0x88,width);if(exceeds_third(width,r))warning(r,"Warning:  The secondary metal width exceeds 1/3 chip dimensions."); }
    if(!select(line,h,0,"S",7,r)) { warning(r,"Error:  Missing Argument, must supply metal-to-metal spacing S.");invalid=true; }
    else {
        const long double space=argument(h.temporary[0]),twice=space+space;h.spacing=x87::binary64(space);
        const long double second=twice+load(real(primary,0x88));store(primary,0x90,twice+load(real(secondary,0x88)));store(secondary,0x90,second);
        if(load(r.chip_x)/3.L<load(real(primary,0x90)) || load(r.chip_y)/3.L<second)warning(r,"Warning:  The  spacing exceeds 1/3 chip dimensions.");
    }
    if(!select(line,h,0,"LEN",7,r)) { warning(r,"Error:  Must specify outer dimension.");invalid=true; }
    else {
        const long double length=argument(h.temporary[0]),space=load(h.spacing);store(primary,0x70,length);
        store(secondary,0x70,length-(((space+load(real(primary,0x88)))+space)+load(real(primary,0x88))));
        const long double w=space+space+load(real(primary,0x88))+load(real(secondary,0x88));store(primary,0x78,w);store(secondary,0x78,w);
        if(chip_exceeded(primary,r)) { warning(r,"Error:  Spiral dimensions exceed chip dimension.");invalid=true; }
    }
    check_turns(primary,r,"Error:  Spirals of given dimensions cannot support more than %5.2f turns.",invalid,true);
    if(!select(line,h,0,"METAL",1,r))word(secondary,0x68)=word(primary,0x68)=r.technology.metal_count-1;
    else {
        const int metal=select_metal(h,r);word(primary,0x68)=word(secondary,0x68)=metal;
        if(metal<0 || metal>=r.technology.metal_count) { warning(r,"Error:  Illegal metal layer.");invalid=true; }
    }
    if(!select(line,h,0,"METAL2",1,r))word(secondary,0x6c)=word(primary,0x6c)=r.technology.metal_count-2;
    else {
        const int metal=select_metal(h,r);word(primary,0x6c)=word(secondary,0x6c)=metal;
        if(metal<0 || metal>=r.technology.metal_count || word(primary,0x6c)==word(primary,0x68)) { warning(r,"Error:  Illegal transition metal layer.");invalid=true; }
    }
    origins(line,primary,r,h,invalid);
    if(invalid) { r.invalid(r.user);__builtin_unreachable(); }
    const long double half=0.5L*load(real(primary,0x70));store(secondary,0x80,half);store(primary,0x80,half);
    word(secondary,0x50)=5;const long double x=load(real(primary,0x54))+load(real(primary,0x88));word(primary,0x50)=5;
    store(secondary,0x54,x+load(h.spacing));store(secondary,0x5c,load(real(primary,0x5c))+load(real(primary,0x88))+load(h.spacing));
    floating_copy(secondary,0xa0,primary,0xa0);word(secondary,0xac)=word(primary,0xac)=1;
}
}
