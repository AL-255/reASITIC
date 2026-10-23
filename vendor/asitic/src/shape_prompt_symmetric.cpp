#include "asitic/shape_prompts.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
void copy_text(char* d,const char* s) { do { *d=*s; ++d; }while(*s++); }
template<class T> T read(const GeometryShape& shape,unsigned offset) {
    T out;__builtin_memcpy(&out,reinterpret_cast<const unsigned char*>(&shape)+offset,sizeof(T));return out;
}
template<class T> void write(GeometryShape& shape,unsigned offset,T value) {
    __builtin_memcpy(reinterpret_cast<unsigned char*>(&shape)+offset,&value,sizeof(T));
}
long double load(const double& value) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(value):"st");return out; }
long double load_float(unsigned value) { long double out;asm volatile("flds %1; fstpt %0":"=m"(out):"m"(value):"st");return out; }
unsigned comparison(long double a,long double b) {
    unsigned short status;asm volatile("fldt %2; fldt %1; fcompp; fnstsw %%ax":"=a"(status):"m"(a),"m"(b):"st","st(1)");return (status>>8)&0x45;
}
bool less(long double a,long double b) { return comparison(a,b)==1; }
bool greater(long double a,long double b) { return comparison(a,b)==0; }
double down(long double x) {
    // Both adapters temporarily select round toward negative infinity, then
    // restore the complete caller control word before the binary64 store.
    unsigned short saved,temporary;double out;asm volatile("fnstcw %0":"=m"(saved));temporary=(saved&0xf3ffu)|0x400u;
    asm volatile("fldt %1; fldcw %2; frndint; fldcw %3; fstpl %0":"=m"(out):"m"(x),"m"(temporary),"m"(saved):"st");return out;
}
void warn(const ShapePromptRuntime& r,const char* s) { r.command.warnings.emit(s); }
bool get(const char* line,CommandArgument& out,const char* key,unsigned type,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    return extract_shape_prompt_argument(line,out,key,type,r.command,h);
}
double value(const CommandArgument& a) { return x87::binary64(load_float(a.value)); }
void number(GeometryShape& shape,unsigned offset,const CommandArgument& a) { write(shape,offset,value(a)); }
bool transition(GeometryShape& shape,const CommandArgument& a) {
    // The native transition clearance test spills the hypotenuse, inverse
    // cosine and tangent to binary64; the first atan and half angle stay x87.
    const long double y=load_float(a.value),space=load(read<double>(shape,0x90));
    const long double radius=x87::sqrt(space*space+y*y);
    const double radius64=x87::binary64(radius);write(shape,0x78,x87::binary64(y));
    const long double angle=x87::atan2(y/space,1.L);
    const long double width=load(read<double>(shape,0x88));
    const long double ratio=width/load(radius64);
    const double acos64=x87::binary64(x87::atan2(x87::sqrt(1.L-ratio*ratio),ratio));
    const long double half=((load(0x1.921fb54442d18p+1)-angle)-load(acos64))*load(0.5);
    long double tangent;asm volatile("fptan; fstp %%st(0)":"=t"(tangent):"0"(half));
    const double tangent64=x87::binary64(tangent);
    const long double extent=width*load(tangent64);
    return greater(extent+extent,y-load(1e-10));
}
void common_start(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h,bool& bad) {
    auto& a=h.temporary[0];normalize_input_line(line,0xae,r.command.characters);
    if(!get(line,a,"NAME",3,r,h)) { warn(r,"Error:  Missing Argument, must supply spiral name.");bad=true; }
    else {
        copy_text(reinterpret_cast<char*>(&shape),a.text);
        if(find_geometry_shape(r.command.shapes,r.command.selected,reinterpret_cast<const char*>(&shape))) {
            warn(r,"Error:  The supplied spiral name is already in use.");bad=true;
        }
    }
    if(!get(line,a,"N",4,r,h)) { warn(r,"Error:  Missing Argument, must supply no. of turns N.");bad=true; }
    else number(shape,0x98,a);
    const char* keys[]={"W","S"};
    const char* errors[]={"Error:  Missing Argument, must supply metal width W.","Error:  Missing Argument, must supply metal spacing S."};
    for(unsigned i=0;i<2;++i) {
        if(!get(line,a,keys[i],7,r,h)) { warn(r,errors[i]);bad=true; }
        else {
            const long double n=load_float(a.value);write(shape,0x88+8*i,x87::binary64(n));
            if(less(load(r.chip_x)/load(3.),n) || less(load(r.chip_y)/load(3.),n))warn(r,"Warning:  The spiral width exceeds 1/3 chip dimensions.");
        }
    }
}
void common_end(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h,bool& bad,bool polygon) {
    auto& a=h.temporary[0];auto& b=h.temporary[1];
    if(!get(line,a,"METAL",1,r,h))write(shape,0x68,r.technology.metal_count-1);
    else {
        if(h.calls)h.layer=h.scan.integer;
        const int index=find_shape_prompt_metal(a.text,r,h.layer);write(shape,0x68,index);
        if(index<0 || index>=r.technology.metal_count) { warn(r,"Error:  Illegal metal layer.");bad=true; }
    }
    if(!get(line,a,"METAL2",1,r,h))write(shape,0x6c,r.technology.metal_count-2);
    else {
        if(h.calls)h.layer=h.scan.integer;
        const int index=find_shape_prompt_metal(a.text,r,h.layer);write(shape,0x6c,index);
        if(!polygon && (index<0 || index>=r.technology.metal_count)) { warn(r,"Error:  Illegal exit metal layer.");bad=true; }
    }
    if(polygon && !bad && (read<int>(shape,0x6c)<0 || read<int>(shape,0x6c)>=r.technology.metal_count)) {
        warn(r,"Error:  Illegal exit metal layer.");bad=true;
    }
    const bool has_x=get(line,a,"XORG",7,r,h),has_y=get(line,b,"YORG",7,r,h);
    if(has_x)number(shape,0x54,a);
    else write(shape,0x54,polygon?x87::binary64(load(read<double>(shape,0x78))*load(0.5)+load(read<double>(shape,0x80))):0.);
    if(has_y)number(shape,0x5c,b);
    else write(shape,0x5c,polygon?x87::binary64(load(read<double>(shape,0x78))*load(0.5)+load(read<double>(shape,0x80))):0.);
    const long double x=load(read<double>(shape,0x54)),y=load(read<double>(shape,0x5c));
    if(less(x,0.L) || less(load(r.chip_x),x))warn(r,"Warning:  Origin x coordinate out of range.");
    if(less(y,0.L) || less(load(r.chip_y),y))warn(r,"Warning:  Origin y coordinate out of range.");
    if(get(line,a,"ORIENT",10,r,h))number(shape,0xa0,a);else write(shape,0xa0,0.);
    const long double angle=load(read<double>(shape,0xa0));
    if(less(angle,-360.L) || greater(angle,360.L)) { warn(r,"Error:  Spiral orientation angle not in range [-360,360].");bad=true; }
    write(shape,0xac,get(line,a,"PHASE",6,r,h)?a.value:1u);
    if(read<unsigned>(shape,0xac)==0)write(shape,0xac,1u);
    write(shape,0x50,polygon?17:16);write(shape,0x80,x87::binary64(load(read<double>(shape,0x70))));
    if(bad)r.invalid(r.user);
}
void round_turns(GeometryShape& shape,double maximum,const ShapePromptRuntime& r,bool& bad) {
    long double turns=load(read<double>(shape,0x98));
    if(greater(turns,load(maximum))) {
        r.command.format(r.command.message,"Error:  Spiral of given dimensions cannot support more than %5.2f turns.",maximum);
        warn(r,r.command.message);bad=true;turns=load(read<double>(shape,0x98));
    }
    write(shape,0x98,down(turns));
}
}
void parse_symmetric_square_shape_arguments(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    // Original 0x08084f54; quarter turns are rounded down to whole turns.
    bool bad=false;write(shape,0x64,4);common_start(line,shape,r,h,bad);auto& a=h.temporary[0];
    if(get(line,a,"LEN",7,r,h))number(shape,0x70,a);
    else { warn(r,"Error:  Missing Argument, must supply spiral dimension.");bad=true; }
    if(get(line,a,"ILEN",7,r,h)) {
        if(transition(shape,a)) { warn(r,"Warning:  Need more space for transitions.");bad=true; }
    }else write(shape,0x78,x87::binary64(load(read<double>(shape,0x88))+load(read<double>(shape,0x90))));
    if(!bad) {
        const long double x=load(read<double>(shape,0x70)),y=load(read<double>(shape,0x78));
        const long double size=less(y,x)?x:y;
        const long double cx=load(r.chip_x),cy=load(r.chip_y),chip=greater(cy,cx)?cx:cy;
        if(less(chip,size))warn(r,"Warning:  Dimensions exceed chip dimensions.");
        const long double pitch=load(read<double>(shape,0x88))+load(read<double>(shape,0x90));
        round_turns(shape,down(x/(pitch+pitch)-load(0.25)),r,bad);
    }
    common_end(line,shape,r,h,bad,false);
}
void parse_symmetric_polygon_shape_arguments(char* line,GeometryShape& shape,const ShapePromptRuntime& r,ShapePromptHistory& h) {
    // Original 0x080856bc; odd sides are reduced by one before validation.
    bool bad=false;common_start(line,shape,r,h,bad);auto& a=h.temporary[0];
    if(get(line,a,"RAD",7,r,h)) { number(shape,0x80,a);number(shape,0x70,a); }
    else { warn(r,"Error:  Missing Argument, must supply spiral radius.");bad=true; }
    if(get(line,a,"ILEN",7,r,h)) {
        if(transition(shape,a)) { warn(r,"Warning:  Need more space for transitions.");bad=true; }
    }else write(shape,0x78,x87::binary64(load(read<double>(shape,0x88))+load(read<double>(shape,0x90))));
    if(get(line,a,"SIDES",6,r,h)) {
        unsigned sides=a.value;if(sides&1u)--sides;write(shape,0x64,sides);
        if(read<int>(shape,0x64)<3) { warn(r,"Error:  Need at least 3 sides to make a polygon.");bad=true; }
    }else write(shape,0x64,16);
    if(!bad) {
        long double radius=load(read<double>(shape,0x80));
        const long double cx=load(r.chip_x),cy=load(r.chip_y),chip=greater(cy,cx)?cx:cy;
        if(less(chip,radius+radius)) { warn(r,"Warning:  Spiral dimensions exceed chip dimension.");radius=load(read<double>(shape,0x80)); }
        const long double cosine=x87::cos(load(0x1.921fb54442d18p+1)/static_cast<long double>(read<int>(shape,0x64)));
        const long double pitch=load(read<double>(shape,0x88))+load(read<double>(shape,0x90));
        round_turns(shape,down(radius*cosine/pitch),r,bad);
    }
    common_end(line,shape,r,h,bad,true);
}
}
