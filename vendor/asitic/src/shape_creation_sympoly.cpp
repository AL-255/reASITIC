#include "asitic/shape_creation_commands.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
double& field(GeometryShape& s,unsigned n) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+n); }
int& word(GeometryShape& s,unsigned n) { return *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+n); }
long double load(const double& value) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(value):"st");return out; }
unsigned compare(long double a,long double b) { unsigned short status;asm volatile("fldt %2; fldt %1; fcompp; fnstsw %%ax":"=a"(status):"m"(a),"m"(b):"st","st(1)");return (status>>8)&0x45; }
bool less(long double a,long double b) { return compare(a,b)==1; }
bool greater(long double a,long double b) { return compare(a,b)==0; }
void copy(char* d,const char* s) { do { *d++=*s; }while(*s++); }
void warn(const ShapeCreationRuntime& r,const char* text) { r.shape.command.warnings.emit(text); }
void formatted(const ShapeCreationRuntime& r,const char* format,double value) { r.shape.command.format(r.shape.command.message,format,value);warn(r,r.shape.command.message); }
void read_dimension(double& value,const char* prompt,const ShapeCreationRuntime& r) { read_shape_creation_line(prompt,r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%lf",&value); }
void origin(GeometryShape& s,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    double& x=field(s,0x54);double& y=field(s,0x5c);
    for(;;) {
        read_shape_creation_line("Origin of spiral center (x y)? ",h.origin,r);
        for(char* p=h.origin;*p;++p)if(*p=='(' || *p==')' || *p==',')*p=' ';
        const int count=r.shape.command.scan(h.origin,"%lf %lf",&x,&y);
        const bool invalid=less(load(x),0.L) || less(load(y),0.L) || greater(load(x),load(r.shape.chip_x)) || greater(load(y),load(r.shape.chip_y));
        if(invalid)warn(r,"Error:  Origin falls outside of chip boundary.");
        if(count>=1 && !invalid)return;
    }
}
void layer(GeometryShape& s,unsigned offset,const char* prompt,const ShapeCreationRuntime& r,ShapeCreationHistory& h,bool distinct=false) {
    do { read_shape_creation_line(prompt,r.shape.command.message,r);word(s,offset)=resolve_shape_creation_metal(r.shape.command.message,r,h); }
    while(word(s,offset)<0 || word(s,offset)>=r.shape.technology.metal_count || (distinct && word(s,offset)==word(s,0x68)));
}
// Native scanf -1 selects a default; scanf 0 preserves descriptor history.
void bound(double& value,const char* prompt,double minimum,double maximum,double fallback,const ShapeCreationRuntime& r) {
    for(;;) {
        read_shape_creation_line(prompt,r.shape.command.message,r);
        if(r.shape.command.scan(r.shape.command.message,"%lf",&value)==-1) {
            value=load(fallback)+load(1e-10);formatted(r,"Note:  Using default value of %5.2lf",fallback);
        }
        if(greater(load(value),load(maximum))) { formatted(r,"Error: Dimension exceeds maximum dimension of %5.2lf.",maximum);continue; }
        if(less(load(value),load(minimum)))formatted(r,"Warning:  Dimension less than %5.2lf",minimum);
        return;
    }
}
long double squared(long double x) {
    if(x==0.L)return 0.L;
    return load(x87::binary64((x*x)*1.L));
}
void transition_width_warning(GeometryShape& s,const ShapeCreationRuntime& r) {
    const long double spacing=load(field(s,0x90)),transition=load(field(s,0x78));
    const long double spacing_squared=squared(load(field(s,0x90))),transition_squared=squared(load(field(s,0x78)));
    const double first=x87::binary64(x87::sqrt(spacing_squared+transition_squared));
    const long double sum=spacing+load(field(s,0x88));
    const long double sum_squared=squared(sum),again_transition_squared=squared(load(field(s,0x78)));
    const long double second=x87::sqrt(sum_squared+again_transition_squared);
    const double width=x87::binary64(load(first)-((spacing+spacing)*transition)/second);
    const auto* record=reinterpret_cast<const unsigned char*>(r.shape.technology.metals+word(s,0x68));
    const long double minimum=load(*reinterpret_cast<const double*>(record+0xc8));
    if(minimum!=0.L && greater(minimum,load(width)))warn(r,"Warning:  Transition metal width less than WMIN specified in technology file");
}
}
void prompt_symmetric_polygon_creation(GeometryShape& s,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    word(s,0x50)=17;char* line=r.shape.command.message;
    for(;;) {
        normalize_shape_creation_prompt("Name? ",line,r);
        if(find_geometry_shape(r.shape.command.shapes,r.shape.command.selected,line)) { warn(r,"Another existing spiral has the same name.");*line=0; }
        else copy(reinterpret_cast<char*>(&s),line);
        if(*line)break;
    }
    normalize_input_line(reinterpret_cast<char*>(&s),0x8e,r.shape.command.characters);
    for(;;) {
        read_dimension(field(s,0x80),"Radius (center to edge)? ",r);long double n=load(field(s,0x80));
        if(greater(n,load(r.shape.chip_x)) || greater(n,load(r.shape.chip_y))) { warn(r,"Error:  Radius exceeds chip dimensions.");n=load(field(s,0x80)); }
        if(!(less(n,load(1e-10)) || greater(n,load(r.shape.chip_x)) || greater(n,load(r.shape.chip_y))))break;
    }
    const double radius=x87::binary64(load(field(s,0x80)));
    for(;;) {
        read_dimension(field(s,0x88),"Metal width? ",r);long double n=load(field(s,0x88));
        if(greater(n,load(r.shape.chip_x)) || greater(n,load(r.shape.chip_y))) { warn(r,"Error:  Metal width exceeds chip dimensions.");n=load(field(s,0x88)); }
        if(greater(n,load(radius))) { warn(r,"Error:  Metal width exceeds spiral dimensions.");n=load(field(s,0x88)); }
        if(!(less(n,load(1e-10)) || greater(n,load(r.shape.chip_x)) || greater(n,load(r.shape.chip_y)) || greater(n,load(radius))))break;
    }
    layer(s,0x68,"Metal layer? ",r,h);word(s,0x6c)=word(s,0x68);layer(s,0x6c,"Transition metal layer? ",r,h,true);
    const auto* record=reinterpret_cast<const unsigned char*>(r.shape.technology.metals+word(s,0x68));
    const double technology_spacing=x87::binary64(load(*reinterpret_cast<const double*>(record+0xc0)));
    if(load(technology_spacing)==0.L) {
        const double spacing_limit=x87::binary64(load(field(s,0x80))*load(0.5));
        for(;;) {
            read_dimension(field(s,0x90),"Spacing (metal edge to metal edge)? ",r);long double n=load(field(s,0x90));
            if(greater(n,load(r.shape.chip_x)) || greater(n,load(r.shape.chip_y))) { warn(r,"Error:  Spacing exceeds chip dimensions.");n=load(field(s,0x90)); }
            if(greater(n,load(spacing_limit))) { warn(r,"Error:  Spacing exceeds spiral dimensions.");n=load(field(s,0x90)); }
            if(!(less(n,load(1e-10)) || greater(n,load(r.shape.chip_x)) || greater(n,load(r.shape.chip_y)) || greater(n,load(spacing_limit))))break;
        }
        const double maximum=x87::binary64(load(field(s,0x80))*load(0.5));
        const double fallback=x87::binary64(load(field(s,0x88))+load(field(s,0x90)));
        bound(field(s,0x78),"Space for Transitions: ",0.,maximum,fallback,r);
    } else {
        // +70 is deliberately not initialized by the radius prompt. The native
        // technology-spacing branch uses this incoming descriptor dimension.
        const double maximum=x87::binary64(load(field(s,0x70))*load(0.5));
        bound(field(s,0x90),"Spacing (metal edge to metal edge): ",technology_spacing,maximum,technology_spacing,r);
        bool sufficient=false;
        do {
            double threshold;
            const unsigned relation=compare(load(field(s,0x90)),load(technology_spacing));
            if(static_cast<unsigned char>(relation-1)<0x40) {
                const double transition_maximum=x87::binary64(load(field(s,0x70))*load(0.5));
                bound(field(s,0x78),"Space for Transitions:  ",technology_spacing,transition_maximum,technology_spacing,r);
                threshold=x87::binary64(load(technology_spacing));
            } else {
                const long double spacing=load(field(s,0x90));
                const double square=x87::binary64(spacing*spacing);
                const long double root=x87::sqrt(load(square)-load(technology_spacing)*load(technology_spacing));
                threshold=x87::binary64(((spacing+load(field(s,0x88)))*load(technology_spacing))/root);
                r.shape.command.format(line,"Space for Transitions (%5.2lf):  ",threshold);
                const double warning_minimum=x87::binary64(load(threshold)-load(1e-10));
                const double transition_maximum=x87::binary64(load(field(s,0x70))*load(0.5));
                bound(field(s,0x78),line,warning_minimum,transition_maximum,threshold,r);
            }
            if(less(load(threshold),load(field(s,0x78))))sufficient=true;
            else warn(r,"Warning:  Need more space for transitions.");
            transition_width_warning(s,r);
        }while(!sufficient);
    }
    do {
        read_shape_creation_line("How many sides (>2)? ",line,r);r.shape.command.scan(line,"%d",&word(s,0x64));
        if(word(s,0x64)==4)warn(r,"You can also use the <Square> command for a square spiral.");
    }while(word(s,0x64)<3);
    if(static_cast<unsigned>(word(s,0x64))&1u)word(s,0x64)=static_cast<int>(static_cast<unsigned>(word(s,0x64))+1);
    prompt_shape_turns(field(s,0x98),field(s,0x80),field(s,0x88),field(s,0x90),word(s,0x64),17,r);
    origin(s,r,h);word(s,0xac)=0;field(s,0xa0)=0.;field(s,0x70)=load(field(s,0x70))+load(field(s,0x70));
}
}
