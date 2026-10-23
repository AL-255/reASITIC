#include "asitic/shape_creation_commands.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
double& field(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
int& word(GeometryShape& s,unsigned offset) { return *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset); }
long double load(const double& d) { long double r;asm volatile("fldl %1; fstpt %0":"=m"(r):"m"(d):"st");return r; }
void copy(char* d,const char* s) { do { *d++=*s; }while(*s++); }
void warning(const ShapeCreationRuntime& r,const char* text) { r.shape.command.warnings.emit(text); }
void formatted(const ShapeCreationRuntime& r,const char* format,double value) { r.shape.command.format(r.shape.command.message,format,value);warning(r,r.shape.command.message); }
void name(GeometryShape& out,const ShapeCreationRuntime& r) {
    char* line=r.shape.command.message;
    do {
        normalize_shape_creation_prompt("Name? ",line,r);
        if(find_geometry_shape(r.shape.command.shapes,r.shape.command.selected,line)) { warning(r,"Another existing spiral has the same name.");*line=0; }
        else copy(reinterpret_cast<char*>(&out),line);
    }while(!*line);
    normalize_input_line(reinterpret_cast<char*>(&out),0x8e,r.shape.command.characters);
}
void metal(GeometryShape& out,unsigned offset,const char* prompt,const ShapeCreationRuntime& r,ShapeCreationHistory& h,bool different) {
    int index;do { read_shape_creation_line(prompt,r.shape.command.message,r);index=resolve_shape_creation_metal(r.shape.command.message,r,h);word(out,offset)=index; }
    while(index<0 || index>=r.shape.technology.metal_count || (different && index==word(out,0x68)));
}
void origin(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    for(;;) {
        read_shape_creation_line("Origin of spiral center (x y)? ",h.origin,r);
        for(char* p=h.origin;*p;++p)if(*p=='(' || *p==')' || *p==',')*p=' ';
        const int n=r.shape.command.scan(h.origin,"%lf %lf",&field(out,0x54),&field(out,0x5c));
        const long double x=load(field(out,0x54)),y=load(field(out,0x5c));
        const bool bad=x<0 || y<0 || load(r.shape.chip_x)<x || load(r.shape.chip_y)<y;
        if(bad)warning(r,"Error:  Origin falls outside of chip boundary.");
        if(n>=1 && !bad)break;
    }
}
void bounded(const char* prompt,double& destination,double minimum,double maximum,double fallback,const ShapeCreationRuntime& r) {
    for(;;) {
        read_shape_creation_line(prompt,r.shape.command.message,r);
        if(r.shape.command.scan(r.shape.command.message,"%lf",&destination)==-1) {
            destination=x87::binary64(load(fallback)+load(1e-10));formatted(r,"Note:  Using default value of %5.2lf",fallback);
        }
        const long double value=load(destination);
        if(value>load(maximum)) { formatted(r,"Error: Dimension exceeds maximum dimension of %5.2lf.",maximum);continue; }
        if(value<load(minimum))formatted(r,"Warning:  Dimension less than %5.2lf",minimum);
        break;
    }
}
void positive(const char* prompt,double& destination,double maximum,const char* chip_warning,const char* spiral_warning,const ShapeCreationRuntime& r) {
    for(;;) {
        read_shape_creation_line(prompt,r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%lf",&destination);
        long double value=load(destination);
        if(load(r.shape.chip_x)<value || load(r.shape.chip_y)<value) { warning(r,chip_warning);value=load(destination); }
        if(load(maximum)<value) { warning(r,spiral_warning);value=load(destination); }
        if(!(value<load(1e-10) || load(r.shape.chip_x)<value || load(r.shape.chip_y)<value || load(maximum)<value))break;
    }
}
double squared(long double x) { return x==0?0.:x87::binary64((x*x)*1.L); }
double metal_number(const ShapeCreationRuntime& r,int metal,unsigned offset) {
    double value;__builtin_memcpy(&value,reinterpret_cast<const unsigned char*>(r.shape.technology.metals+metal)+offset,8);return x87::binary64(load(value));
}
double down(long double x) {
    unsigned short saved,temporary;double out;asm volatile("fnstcw %0":"=m"(saved));temporary=(saved&0xf3ffu)|0x400u;
    asm volatile("fldt %1; fldcw %2; frndint; fldcw %3; fstpl %0":"=m"(out):"m"(x),"m"(temporary),"m"(saved):"st");return out;
}
}
void prompt_symmetric_square_creation(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    // Original 0x08069474. A nonzero technology spacing minimum selects the
    // transition clearance loop; zero selects the older spacing prompts.
    word(out,0x50)=16;name(out,r);word(out,0x64)=4;
    const double chip=x87::binary64(load(r.shape.chip_x));
    const double minimum=x87::binary64(load(r.shape.chip_x)/static_cast<long double>(r.cells_x));
    const double fallback=x87::binary64(load(r.shape.chip_x)*load(0.25));
    bounded("Outer Length Edge to Edge: ",field(out,0x70),minimum,chip,fallback,r);
    positive("Metal width? ",field(out,0x88),x87::binary64(load(field(out,0x70))),"Error:  Metal width exceeds chip dimensions.","Error:  Metal width exceeds spiral dimensions.",r);
    metal(out,0x68,"Metal layer? ",r,h,false);
    const double spacing_min=metal_number(r,word(out,0x68),0xc0);word(out,0x6c)=word(out,0x68);
    metal(out,0x6c,"Transition metal layer? ",r,h,true);
    if(load(spacing_min)==0) {
        positive("Spacing (metal edge to metal edge)? ",field(out,0x90),x87::binary64(load(field(out,0x70))*load(0.5)),"Error:  Spacing exceeds chip dimensions.","Error:  Spacing exceeds spiral dimensions.",r);
        const double maximum=x87::binary64(load(field(out,0x70))*load(0.5));
        const double default_space=x87::binary64(load(field(out,0x88))+load(field(out,0x90)));
        bounded("Space for Transitions: ",field(out,0x78),0.,maximum,default_space,r);
    }else {
        bounded("Spacing (metal edge to metal edge): ",field(out,0x90),spacing_min,x87::binary64(load(field(out,0x70))*load(0.5)),spacing_min,r);
        bool enough=false;
        do {
            double required;
            const long double space=load(field(out,0x90)),min=load(spacing_min);
            if(space<=min) {
                bounded("Space for Transitions:  ",field(out,0x78),spacing_min,x87::binary64(load(field(out,0x70))*load(0.5)),spacing_min,r);
                required=x87::binary64(load(spacing_min));
            }else {
                const double space_squared=x87::binary64(space*space);
                required=x87::binary64(((space+load(field(out,0x88)))*min)/x87::sqrt(load(space_squared)-min*min));
                r.shape.command.format(r.shape.command.message,"Space for Transitions (%5.2lf):  ",required);
                bounded(r.shape.command.message,field(out,0x78),x87::binary64(load(required)-load(1e-10)),x87::binary64(load(field(out,0x70))*load(0.5)),required,r);
            }
            if(load(required)<load(field(out,0x78)))enough=true;
            else warning(r,"Warning:  Need more space for transitions.");
            const double hypotenuse=x87::binary64(x87::sqrt(load(squared(load(field(out,0x78))))+load(squared(load(field(out,0x90))))));
            const double pitch_squared=squared(load(field(out,0x90))+load(field(out,0x88)));
            const long double denominator=x87::sqrt(load(squared(load(field(out,0x78))))+load(pitch_squared));
            const double transition_width=x87::binary64(load(hypotenuse)-((load(field(out,0x90))+load(field(out,0x90)))*load(field(out,0x78)))/denominator);
            const double width_min=metal_number(r,word(out,0x68),0xc8);
            if(load(width_min)!=0 && load(transition_width)<load(width_min))warning(r,"Warning:  Transition metal width less than WMIN specified in technology file");
        }while(!enough);
    }
    prompt_shape_turns(field(out,0x98),field(out,0x70),field(out,0x88),field(out,0x90),4,16,r);
    origin(out,r,h);word(out,0xac)=0;field(out,0xa0)=0.;field(out,0x80)=x87::binary64(load(field(out,0x70))*load(0.5));
}
void prompt_ring_creation(GeometryShape& out,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    // Original 0x0806be74. Negative angles retain the native asymmetric wrap;
    // unordered gap values follow the same zero branch as exact zero.
    word(out,0x50)=22;name(out,r);
    for(;;) {
        read_shape_creation_line("Radius (center to edge)? ",r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%lf",&field(out,0x80));
        long double radius=load(field(out,0x80));
        if(load(r.shape.chip_x)<radius || load(r.shape.chip_y)<radius) { warning(r,"Error:  Radius exceeds chip dimensions.");radius=load(field(out,0x80)); }
        if(!(radius<load(1e-10) || load(r.shape.chip_x)<radius || load(r.shape.chip_y)<radius))break;
    }
    do {
        read_shape_creation_line("How many sides (>2)? ",r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%d",&word(out,0x64));
        if(word(out,0x64)==4)warning(r,"You can also use the <Square> command for a square spiral.");
    }while(word(out,0x64)<3);
    positive("Metal width? ",field(out,0x88),x87::binary64(load(field(out,0x80))),"Error:  Metal width exceeds chip dimensions.","Error:  Metal width exceeds spiral dimensions.",r);
    read_shape_creation_line("Gap angle? ",r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%lf",&h.gap);
    const long double angle=load(h.gap);
    if(angle>0)field(out,0x90)=x87::binary64(angle-load(down(angle/load(360.)))*load(360.));
    else if(angle<0)field(out,0x90)=x87::binary64(load(down(angle/load(360.)))*load(360.)+angle);
    else field(out,0x90)=0.;
    field(out,0x90)=x87::binary64(load(field(out,0x90))*load(0x1.1df46a2529d39p-6));
    metal(out,0x68,"Metal layer? ",r,h,false);origin(out,r,h);field(out,0xa0)=0.;word(out,0xac)=1;
}
}
