#include "asitic/shape_creation_commands.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
volatile unsigned& word(GeometryShape& s,unsigned offset) { return *reinterpret_cast<volatile unsigned*>(reinterpret_cast<unsigned char*>(&s)+offset); }
double& number(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
char* name(GeometryShape& s) { return reinterpret_cast<char*>(&s); }
long double load(const double& x) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(x):"st");return out; }
void store(GeometryShape& s,unsigned offset,long double value) { number(s,offset)=x87::binary64(value); }
void copy_float(GeometryShape& d,unsigned od,GeometryShape& s,unsigned os) { store(d,od,load(number(s,os))); }
void copy(char* d,const char* s) { do { *d++=*s; }while(*s++); }
void append(char* d,const char* s) { while(*d)++d;copy(d,s); }
void warning(const ShapeCreationRuntime& r,const char* message) { r.shape.command.warnings.emit(message); }
char* buffer(const ShapeCreationRuntime& r) { return r.shape.command.message; }
void unique_name(GeometryShape& out,const ShapeCreationRuntime& r) {
    do {
        normalize_shape_creation_prompt("Name? ",buffer(r),r);
        if(!find_geometry_shape(r.shape.command.shapes,r.shape.command.selected,buffer(r)))copy(name(out),buffer(r));
        else { warning(r,"Another existing spiral has the same name.");*buffer(r)=0; }
    }while(!*buffer(r));
    normalize_input_line(name(out),0x8e,r.shape.command.characters);
}
long double minimum(GeometryShape& out) {
    const long double length=load(number(out,0x70)),width=load(number(out,0x78));return width>length?length:width;
}
void prompt_scalar(double& out,long double maximum,bool spacing,const ShapeCreationRuntime& r) {
    for(;;) {
        read_shape_creation_line(spacing?"Spacing (metal edge to metal edge)? ":"Metal width? ",buffer(r),r);
        r.shape.command.scan(buffer(r),"%lf",&out);
        if(load(r.shape.chip_x)<load(out) || load(r.shape.chip_y)<load(out))warning(r,spacing?"Error:  Spacing exceeds chip dimensions.":"Error:  Metal width exceeds chip dimensions.");
        if(maximum<load(out))warning(r,spacing?"Error:  Spacing exceeds spiral dimensions.":"Error:  Metal width exceeds spiral dimensions.");
        if(!(load(out)<load(1e-10) || load(r.shape.chip_x)<load(out) || load(r.shape.chip_y)<load(out) || maximum<load(out)))return;
    }
}
int prompt_metal(const char* prompt,GeometryShape& out,unsigned offset,const ShapeCreationRuntime& r,ShapeCreationHistory& history,int excluded=-0x7fffffff) {
    int index;
    do {
        read_shape_creation_line(prompt,buffer(r),r);index=resolve_shape_creation_metal(buffer(r),r,history);word(out,offset)=index;
    }while(index<0 || index>=r.shape.technology.metal_count || index==excluded);
    return index;
}
void prompt_origin(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& history) {
    for(;;) {
        read_shape_creation_line("Origin of spiral center (x y)? ",history.origin,r);
        for(char* p=history.origin;*p;++p)if(*p=='(' || *p==')' || *p==',')*p=' ';
        const int read=r.shape.command.scan(history.origin,"%lf %lf",&number(out,0x54),&number(out,0x5c));
        const bool invalid=load(number(out,0x54))<0.L || load(number(out,0x5c))<0.L || load(r.shape.chip_x)<load(number(out,0x54)) || load(r.shape.chip_y)<load(number(out,0x5c));
        if(invalid)warning(r,"Error:  Origin falls outside of chip boundary.");
        if(read>=1 && !invalid)return;
    }
}
double floor64(long double value) {
    unsigned short saved,down;long double result;asm volatile("fnstcw %0":"=m"(saved));down=(saved&0xf3ffu)|0x400u;
    asm volatile("fldcw %2; frndint; fldcw %3":"=t"(result):"0"(value),"m"(down),"m"(saved));return x87::binary64(result);
}
}
void prompt_transformer_creation(GeometryShape& primary,GeometryShape& secondary,const ShapeCreationRuntime& r,ShapeCreationHistory& history) {
    word(primary,0x50)=3;word(secondary,0x50)=3;
    warning(r,"Primary:");unique_name(primary,r);warning(r,"Secondary:");unique_name(secondary,r);
    int dimensions=0;prompt_shape_dimensions(number(primary,0x70),number(primary,0x78),dimensions,r);
    prompt_scalar(number(primary,0x88),minimum(primary),false,r);
    prompt_scalar(number(primary,0x90),minimum(primary),true,r);
    const double spacing=x87::binary64(load(number(primary,0x90))+load(number(primary,0x90))+load(number(primary,0x88)));
    prompt_shape_turns(number(primary,0x98),x87::binary64(minimum(primary)),x87::binary64(load(number(primary,0x88))),spacing,4,static_cast<int>(word(primary,0x50)),r);
    const int metal=prompt_metal("Metal layer? ",primary,0x68,r,history);
    read_shape_creation_line("Add exit segment (y/n)?",buffer(r),r);
    if(*buffer(r)=='Y' || *buffer(r)=='y')prompt_metal("Exit metal layer? ",primary,0x6c,r,history,metal);
    else word(primary,0x6c)=~0u;
    prompt_origin(primary,r,history);
    const long double half=0.5L;const long double outer=load(number(primary,0x70));word(primary,0xac)=1;store(primary,0x80,outer*half);number(primary,0xa0)=0.;
    copy_float(secondary,0x88,primary,0x88);
    const long double effective=load(number(primary,0x90))+load(number(primary,0x90))+load(number(secondary,0x88));word(primary,0x64)=4;
    const long double length=load(number(primary,0x70));store(primary,0x90,effective);store(secondary,0x70,length);
    copy_float(secondary,0x78,primary,0x78);copy_float(secondary,0x90,primary,0x90);copy_float(secondary,0x98,primary,0x98);
    word(secondary,0x68)=word(primary,0x68);word(secondary,0x6c)=word(primary,0x6c);word(secondary,0xac)=word(primary,0xac);word(secondary,0x64)=4;
    copy_float(secondary,0x54,primary,0x54);copy_float(secondary,0x5c,primary,0x5c);copy_float(secondary,0xa0,primary,0xa0);store(secondary,0x80,length*half);
}
void prompt_balun_creation(GeometryShape& primary,GeometryShape& secondary,const ShapeCreationRuntime& r,ShapeCreationHistory& history) {
    double spacing=5.;word(primary,0x50)=5;unique_name(primary,r);copy(name(secondary),name(primary));append(name(primary),"-P");append(name(secondary),"-S");
    const double lower=x87::binary64(static_cast<long double>(r.cells_x)),maximum=x87::binary64(load(r.shape.chip_x));
    const double default_value=x87::binary64(load(r.shape.chip_x)*5.L/load(lower));
    for(;;) {
        read_shape_creation_line("Outer Length Edge to Edge: ",buffer(r),r);const int read=r.shape.command.scan(buffer(r),"%lf",&number(primary,0x70));
        if(read==-1) { store(primary,0x70,load(default_value)+load(1e-10));r.shape.command.format(buffer(r),"Note:  Using default value of %5.2lf",default_value);warning(r,buffer(r)); }
        if(load(number(primary,0x70))>load(maximum)) {
            r.shape.command.format(buffer(r),"Error: Dimension exceeds maximum dimension of %5.2lf.",maximum);warning(r,buffer(r));
        }else {
            if(load(number(primary,0x70))<load(lower)) { r.shape.command.format(buffer(r),"Warning:  Dimension less than %5.2lf",lower);warning(r,buffer(r)); }
            break;
        }
    }
    warning(r,"Primary Metal Width:");prompt_scalar(number(primary,0x88),load(number(primary,0x70)),false,r);
    warning(r,"Metal Spacing between Primary and Secondary:");prompt_scalar(spacing,load(number(primary,0x70)),true,r);
    word(secondary,0x50)=5;const long double inset=load(number(primary,0x88))+load(spacing);store(secondary,0x70,load(number(primary,0x70))-(inset+inset));
    warning(r,"Secondary Metal Width:");prompt_scalar(number(secondary,0x88),load(number(secondary,0x70)),false,r);
    const long double gap=load(spacing),twice=gap+gap;
    const long double width=load(number(primary,0x88))+load(number(secondary,0x88))+twice;store(secondary,0x78,width);store(primary,0x78,width);
    store(primary,0x90,twice+load(number(secondary,0x88)));store(secondary,0x90,twice+load(number(primary,0x88)));
    prompt_shape_turns(number(primary,0x98),x87::binary64(load(number(primary,0x70))),x87::binary64(load(number(primary,0x88))),x87::binary64(gap),4,5,r);
    const double primary_turns=floor64((load(number(primary,0x98))+1.L)*0.5L);store(primary,0x98,load(primary_turns));store(secondary,0x98,load(primary_turns)-1.L);
    prompt_metal("Metal layer? ",primary,0x68,r,history);word(primary,0x6c)=word(primary,0x68);
    prompt_metal("Transition metal layer? ",primary,0x6c,r,history,static_cast<int>(word(primary,0x68)));
    word(secondary,0x68)=word(primary,0x68);word(secondary,0x6c)=word(primary,0x6c);
    prompt_origin(primary,r,history);
    store(secondary,0x54,load(number(primary,0x54))+load(number(primary,0x88))+load(spacing));
    const long double y=load(number(primary,0x5c)),primary_width=load(number(primary,0x88));number(secondary,0xa0)=0.;store(secondary,0x5c,y+primary_width+load(spacing));number(primary,0xa0)=0.;
    word(secondary,0xac)=0;word(primary,0xac)=0;const long double half=load(number(primary,0x70))*0.5L;store(secondary,0x80,half);store(primary,0x80,half);word(secondary,0x64)=4;word(primary,0x64)=4;
}
}
