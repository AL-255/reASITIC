#include "asitic/shape_creation_commands.hpp"
namespace asitic {
namespace {
long double load(const double& n) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(n):"st");return out; }
double floor64(long double n) {
    unsigned short saved,down;double out;asm volatile("fnstcw %0":"=m"(saved));down=(saved&0xf3ffu)|0x400u;
    asm volatile("fldcw %1; fldt %2; frndint; fldcw %3; fstpl %0":"=m"(out):"m"(down),"m"(n),"m"(saved):"st");return out;
}
long double cosine(long double n) { long double out;asm volatile("fldt %1; fcos; fstpt %0":"=m"(out):"m"(n):"st");return out; }
long double quantize(long double n,const double& sides) {
    const double first=floor64(n),second=floor64(n);const long double product=(n-load(second))*load(sides);
    double discarded;asm volatile("fldt %1; fstl %0; fstp %%st(0)":"=m"(discarded):"m"(product):"st");
    const double fraction=floor64(product);return load(fraction)/load(sides)+load(first);
}
void warning(const ShapeCreationRuntime& r,const char* text) { r.shape.command.warnings.emit(text); }
double& field(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
int& word(GeometryShape& s,unsigned offset) { return *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset); }
void copy(char* d,const char* s) { do { *d++=*s; }while(*s++); }
void prompt_name(GeometryShape& out,const ShapeCreationRuntime& r) {
    char* line=r.shape.command.message;
    do {
        normalize_shape_creation_prompt("Name? ",line,r);
        if(!find_geometry_shape(r.shape.command.shapes,r.shape.command.selected,line))copy(reinterpret_cast<char*>(&out),line);
        else { warning(r,"Another existing spiral has the same name.");line[0]=0; }
    }while(!*line);
    normalize_input_line(reinterpret_cast<char*>(&out),0x8e,r.shape.command.characters);
}
void prompt_origin(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    int scanned;bool invalid;
    do {
        read_shape_creation_line("Origin of spiral center (x y)? ",h.origin,r);
        for(char* p=h.origin;*p;++p)if(static_cast<unsigned char>(*p-'(')<2 || *p==',')*p=' ';
        scanned=r.shape.command.scan(h.origin,"%lf %lf",&field(out,0x54),&field(out,0x5c));
        invalid=load(field(out,0x54))<0.L || load(field(out,0x5c))<0.L || load(r.shape.chip_x)<load(field(out,0x54)) || load(r.shape.chip_y)<load(field(out,0x5c));
        if(invalid)warning(r,"Error:  Origin falls outside of chip boundary.");
    }while(scanned<1 || invalid);
}
void prompt_winding_parameter(GeometryShape& out,unsigned offset,double bound,const ShapeCreationRuntime& r,bool spacing) {
    char* line=r.shape.command.message;long double value;
    do {
        read_shape_creation_line(spacing?"Spacing (metal edge to metal edge)? ":"Metal width? ",line,r);
        r.shape.command.scan(line,"%lf",&field(out,offset));value=load(field(out,offset));
        if(load(r.shape.chip_x)<value || load(r.shape.chip_y)<value)warning(r,spacing?"Error:  Spacing exceeds chip dimensions.":"Error:  Metal width exceeds chip dimensions.");
        if(load(bound)<load(field(out,offset)))warning(r,spacing?"Error:  Spacing exceeds spiral dimensions.":"Error:  Metal width exceeds spiral dimensions.");
        value=load(field(out,offset));
    }while(value<load(1e-10) || load(r.shape.chip_x)<value || load(r.shape.chip_y)<value || load(bound)<value);
}
double smaller_dimension(GeometryShape& out) {
    const long double x=load(field(out,0x70)),y=load(field(out,0x78));return y>x?x:y;
}
int truncate_integer(long double n) {
    unsigned short saved,toward_zero;int out;asm volatile("fnstcw %0":"=m"(saved));toward_zero=(saved&255u)|0xc00u;
    asm volatile("fldcw %1; fldt %2; fistpl %0; fldcw %3":"=m"(out):"m"(toward_zero),"m"(n),"m"(saved):"st");return out;
}
double binary64(long double n) { volatile double out=static_cast<double>(n);return out; }
void expand_inner_square(GeometryShape& out,bool multimetal) {
    const long double turns=load(field(out,0x98));
    if(multimetal) {
        const double a=floor64(turns),b=floor64(turns);
        const long double dx=(load(a)-1.L)*load(field(out,0x90))+load(b)*load(field(out,0x88));
        field(out,0x70)=dx+dx+load(field(out,0x70));
        const double c=floor64(turns),d=floor64(turns);
        const long double dy=load(c)*load(field(out,0x88))+load(field(out,0x90))*(load(d)-1.L);
        field(out,0x78)=dy+dy+load(field(out,0x78));return;
    }
    const double whole=floor64(turns),fraction=binary64(turns-load(whole));
    const double biased=binary64(load(fraction)+load(1e-10));const int part=truncate_integer(load(biased)*4.L);
    const double whole_a=floor64(turns),whole_b=floor64(turns);const int base=truncate_integer(load(whole_a)+load(whole_b));
    int nx=base,ny=base;
    if(part==1)ny=static_cast<int>(static_cast<unsigned>(base)+1u);
    else if(part==2)nx=ny=static_cast<int>(static_cast<unsigned>(base)+1u);
    else if(part==3) { nx=static_cast<int>(static_cast<unsigned>(base)+1u);ny=static_cast<int>(static_cast<unsigned>(base)+2u); }
    field(out,0x70)=load(field(out,0x90))*static_cast<long double>(static_cast<int>(static_cast<unsigned>(nx)-2u))+load(field(out,0x88))*static_cast<long double>(nx)+load(field(out,0x70));
    field(out,0x78)=load(field(out,0x90))*static_cast<long double>(static_cast<int>(static_cast<unsigned>(ny)-2u))+load(field(out,0x88))*static_cast<long double>(ny)+load(field(out,0x78));
}
void prompt_rectangular(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h,bool multimetal) {
    word(out,0x64)=4;word(out,0x50)=multimetal?18:0;prompt_name(out,r);
    int inner=1;prompt_shape_dimensions(field(out,0x70),field(out,0x78),inner,r);
    prompt_winding_parameter(out,0x88,smaller_dimension(out),r,false);
    prompt_winding_parameter(out,0x90,smaller_dimension(out),r,true);
    prompt_shape_turns(field(out,0x98),inner?1e15:smaller_dimension(out),field(out,0x88),field(out,0x90),4,0,r);
    if(inner)expand_inner_square(out,multimetal);
    char* line=r.shape.command.message;
    do {
        read_shape_creation_line(multimetal?"Top Metal layer? ":"Metal layer? ",line,r);word(out,0x68)=resolve_shape_creation_metal(line,r,h);
    }while(word(out,0x68)<0 || word(out,0x68)>=r.shape.technology.metal_count);
    const int metal=word(out,0x68);
    if(multimetal) {
        do { read_shape_creation_line("Bottom Metal layer?",line,r);word(out,0x6c)=resolve_shape_creation_metal(line,r,h); }
        while(word(out,0x6c)<0 || word(out,0x6c)>=r.shape.technology.metal_count);
    } else {
        read_shape_creation_line("Add exit segment (y/n)?",line,r);
        if(*line=='Y' || *line=='y') {
            do { read_shape_creation_line("Exit metal layer? ",line,r);word(out,0x6c)=resolve_shape_creation_metal(line,r,h); }
            while(word(out,0x6c)==metal || word(out,0x6c)<0 || word(out,0x6c)>=r.shape.technology.metal_count);
        }else word(out,0x6c)=-1;
    }
    prompt_origin(out,r,h);word(out,0xac)=1;field(out,0xa0)=0.;field(out,0x80)=load(field(out,0x70))*0.5L;
}
}
void read_shape_creation_line(const char* prompt,char* out,const ShapeCreationRuntime& r) { r.read(prompt,out,r.user); }
void normalize_shape_creation_prompt(const char* prompt,char* out,const ShapeCreationRuntime& r) {
    read_shape_creation_line(prompt,out,r);normalize_input_line(out,0xae,r.shape.command.characters);
}
int resolve_shape_creation_metal(char* line,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    if(h.layer_calls)h.layer=h.layer_calls[h.layer_index];
    ++h.layer_index;
    return find_shape_prompt_metal(line,r.shape,h.layer);
}
int resolve_shape_creation_via(char* line,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    if(h.layer_calls)h.layer=h.layer_calls[h.layer_index];
    ++h.layer_index;
    return find_shape_prompt_via(line,r.shape,h.layer);
}
void prompt_shape_turns(double& turns,double dimension,double width,double spacing,int sides,int kind,const ShapeCreationRuntime& r) {
    const double count=sides;const bool integral=static_cast<unsigned>(kind)-16u<2u;const double minimum=kind==5?3.:1.;
    char* line=r.shape.command.message;long double maximum;
    for(;;) {
        read_shape_creation_line(kind==5?"Total Turns (pri+sec)? ":integral?"Turns? ":"Turns (fractions okay)? ",line,r);
        r.shape.command.scan(line,"%lf",&turns);
        if(integral || kind==5)turns=floor64(load(turns));
        long double n;
        if(kind==0 || kind==3 || kind==5 || kind==16 || kind==18)n=((load(dimension)+load(spacing))/(load(width)+load(spacing)))*0.5L;
        else if(kind==1 || kind==17 || kind==20) { const double pi=0x1.921fb54442d18p+1;n=(cosine(load(pi)/load(count))*load(dimension))/(load(spacing)+load(width)); }
        else n=1e15L;
        maximum=quantize(n,count);if(integral)maximum=load(floor64(maximum));
        if(maximum<1.L)r.shape.invalid(r.shape.user);
        if(maximum<load(turns)) { r.shape.command.format(line,"Cannot have more than %lf turns.",static_cast<double>(maximum));warning(r,line); }
        if(load(turns)<load(minimum)) { r.shape.command.format(line,"Need at least %lf truns.",minimum);warning(r,line); }
        if(!(load(turns)<load(minimum)) && !(maximum<load(turns)))break;
    }
    turns=quantize(load(turns),count);
}
void prompt_shape_dimensions(double& first,double& second,int& inner,const ShapeCreationRuntime& r) {
    char* line=r.shape.command.message;int scanned;
    if(!inner) {
        do {
            read_shape_creation_line("Outer dimension(s)  (edge to edge)? ",line,r);scanned=r.shape.command.scan(line,"%lf %lf",&first,&second);
            if(scanned==1)second=load(first);
        }while(load(first)<load(1e-10) || load(r.shape.chip_x)<load(first) || load(r.shape.chip_y)<load(first) || load(second)<load(1e-10) || load(r.shape.chip_x)<load(second) || load(r.shape.chip_y)<load(second));
    } else {
        do {
            warning(r,"Note:  To specify inner hole dimensions, enter -1.");
            read_shape_creation_line("Outer dimension(s) of spiral (edge to edge)? ",line,r);scanned=r.shape.command.scan(line,"%lf %lf",&first,&second);
            if(load(first)== -1.L) { inner=1;read_shape_creation_line("Inner hole dimension(s)? ",line,r);scanned=r.shape.command.scan(line,"%lf %lf",&first,&second); }else inner=0;
            if(scanned==1)second=load(first);
            if(load(r.shape.chip_x)<load(first) || load(r.shape.chip_y)<load(first) || load(r.shape.chip_x)<load(second) || load(r.shape.chip_y)<load(second))warning(r,"Error:  Length exceeds chip dimensions.");
        }while(load(first)<load(1e-10) || load(r.shape.chip_x)<load(first) || load(r.shape.chip_y)<load(first) || load(second)<load(1e-10) || load(r.shape.chip_x)<load(second) || load(r.shape.chip_y)<load(second));
    }
}
void prompt_square_creation(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h) { prompt_rectangular(out,r,h,false); }
void prompt_multimetal_square_creation(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h) { prompt_rectangular(out,r,h,true); }
void prompt_polygon_creation(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    word(out,0x50)=1;prompt_name(out,r);char* line=r.shape.command.message;
    do {
        read_shape_creation_line("How many sides (>2)? ",line,r);r.shape.command.scan(line,"%d",&word(out,0x64));
        if(word(out,0x64)==4)warning(r,"You can also use the <Square> command for a square spiral.");
    }while(word(out,0x64)<3);
    long double radius;
    do {
        read_shape_creation_line("Radius (center to edge)? ",line,r);r.shape.command.scan(line,"%lf",&field(out,0x80));radius=load(field(out,0x80));
        if(load(r.shape.chip_x)<radius || load(r.shape.chip_y)<radius)warning(r,"Error:  Radius exceeds chip dimensions.");
        radius=load(field(out,0x80));
    }while(radius<load(1e-10) || load(r.shape.chip_x)<radius || load(r.shape.chip_y)<radius);
    prompt_winding_parameter(out,0x88,field(out,0x80),r,false);prompt_winding_parameter(out,0x90,field(out,0x80),r,true);
    prompt_shape_turns(field(out,0x98),field(out,0x80),field(out,0x88),field(out,0x90),word(out,0x64),1,r);
    do { read_shape_creation_line("Metal layer? ",line,r);word(out,0x68)=resolve_shape_creation_metal(line,r,h); }
    while(word(out,0x68)<0 || word(out,0x68)>=r.shape.technology.metal_count);
    prompt_origin(out,r,h);field(out,0xa0)=0.;word(out,0xac)=1;
}
}
