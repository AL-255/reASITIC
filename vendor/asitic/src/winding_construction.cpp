#include "asitic/winding_construction.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
ConductorSegment* segment(unsigned address) { return reinterpret_cast<ConductorSegment*>(address); }
ConductorSegment* first(GeometryShape& shape) { return const_cast<ConductorSegment*>(shape.filaments.first); }
double& field(ConductorSegment& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
double& number(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(s.filaments.prefix+offset); }
int integer(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const int*>(s.filaments.prefix+offset); }
void floating_copy(double& out,const double& in) { asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st"); }
void reflect(GeometryShape& shape,unsigned axis) {
    double x0,y0,x1,y1;geometry_bounds_xy(shape,&x0,&y0,&x1,&y1);
    const double low=axis?y0:x0,high=axis?y1:x1;
    // FADDP forms 2*low separately before adding the retained difference.
    const long double mirror=(static_cast<long double>(high)-low)+(static_cast<long double>(low)+low);
    const unsigned offsets[]={0x44,0x5c,0x88,0xa0,0,0x18};
    const auto apply=[&](ConductorSegment& s) { for(unsigned offset:offsets)field(s,offset+axis*8)=binary64(mirror-field(s,offset+axis*8)); };
    for(auto* s=first(shape);s;s=segment(s->next_address)) {
        apply(*s);for(auto* child=segment(s->group_next_address);child;child=segment(child->group_next_address))apply(*child);
    }
    GeometryShape previous;floating_copy(number(previous,0x54),number(shape,0x54));floating_copy(number(previous,0x5c),number(shape,0x5c));
    number(shape,0x54)=binary64(static_cast<long double>(number(shape,0x54))+x0);number(shape,0x5c)=binary64(static_cast<long double>(number(shape,0x5c))+y0);
    translate_geometry_shape(shape,previous);
}
double floor_binary64(double value) {
    unsigned short saved,temporary;long double result;asm volatile("fnstcw %0" : "=m"(saved));temporary=(saved&0xf3ffu)|0x0400u;
    asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(result) : "0"(static_cast<long double>(value)),"m"(temporary),"m"(saved));return binary64(result);
}
}
void reflect_geometry_x(GeometryShape& shape) { reflect(shape,0); }
void reflect_geometry_y(GeometryShape& shape) { reflect(shape,1); }
void build_planar_transformer(GeometryShape*& head,const GeometryShape& primary,const GeometryShape& secondary,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const TransformerConstructionHistory& history) {
    build_square_spiral(head,primary,3,technology,runtime,history.winding[0]);build_square_spiral(head,secondary,3,technology,runtime,history.winding[1]);
    reflect_geometry_x(*head);reflect_geometry_y(*head);
    auto& a=*head;auto& b=*head->next;auto& front_a=*first(a);auto& front_b=*first(b);
    const long double width_b=number(b,0x88),shift_a=width_b+(static_cast<long double>(number(a,0x90))-width_b)*0.5L;
    const unsigned starts[]={0,0x44,0x88};for(unsigned offset:starts)field(front_a,offset)=binary64(static_cast<long double>(field(front_a,offset))+shift_a);
    const long double width_a=number(a,0x88),shift_b=(static_cast<long double>(number(b,0x90))-width_a)*0.5L+width_a;
    for(unsigned offset:starts)field(front_b,offset)=binary64(static_cast<long double>(field(front_b,offset))-shift_b);
    number(b,0x54)=binary64(((static_cast<long double>(number(a,0x90))-width_a)*0.5L+width_a)+number(b,0x54));
    number(b,0x5c)=binary64(((static_cast<long double>(number(a,0x90))-width_a)*0.5L+width_a)+number(b,0x5c));
    const long double fraction=static_cast<long double>(number(a,0x98))-floor_binary64(number(a,0x98));
    if(integer(a,0x6c)!=-1) {
        if(fraction==0.25L)number(b,0x54)=binary64(static_cast<long double>(number(b,0x54))+number(a,0x88));
        else if(fraction==0.75L)number(b,0x54)=binary64(static_cast<long double>(number(b,0x54))-number(a,0x88));
        else if(fraction==0.5L)number(b,0x5c)=binary64(static_cast<long double>(number(b,0x5c))-number(a,0x88));
        else number(b,0x5c)=binary64(static_cast<long double>(number(b,0x5c))+number(a,0x88));
    }
    a.auxiliary=reinterpret_cast<unsigned>(&b);b.auxiliary=reinterpret_cast<unsigned>(&a);
}
void build_ring_shape(GeometryShape*& head,const GeometryShape& args,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,WireConstructionHistory history) {
    prepend_geometry_shape(head,runtime.geometry);copy_geometry_shape_fields(*head,args);head->filaments.first=nullptr;
    const double gap=binary64(x87::abs(number(*head,0x90))),width=number(*head,0x88);
    const int sides=integer(*head,0x64),last=static_cast<int>(static_cast<unsigned>(sides)-1u);
    const double cosine=binary64(x87::cos(0x1.921fb54442d18p+1L/static_cast<long double>(sides)));
    const double ribbon=binary64(static_cast<long double>(width)/cosine);
    const long double radius=number(*head,0x80),step=(0x1.921fb54442d18p+2L-gap)/static_cast<long double>(last),angle=static_cast<long double>(gap)*0.5L+number(*head,0xa0);
    if(step<0.L) {
        runtime.warnings.emit("Error:  Gap too large.");if(runtime.structure_violation)runtime.structure_violation(runtime.user);__builtin_trap();
    }
    ConductorSegment s;initialize_geometry_metal(s,args,technology);s.width_count=history.width_count;s.thickness_count=history.thickness_count;s.next_address=0;
    const long double center_radius=radius-static_cast<long double>(ribbon)*0.5L,inner_radius=radius-ribbon;
    s.outer_start[0]=binary64(x87::cos(angle)*radius);s.outer_start[1]=binary64(x87::sin(angle)*radius);
    s.endpoints[0]=binary64(x87::cos(angle)*center_radius);s.endpoints[1]=binary64(x87::sin(angle)*center_radius);
    s.inner_start[0]=binary64(x87::cos(angle)*inner_radius);s.inner_start[1]=binary64(x87::sin(angle)*inner_radius);
    for(int i=1;i<last;i=static_cast<int>(static_cast<unsigned>(i)+1u)) {
        const long double theta=step*i+angle;
        s.outer_end[0]=binary64(x87::cos(theta)*radius);s.outer_end[1]=binary64(x87::sin(theta)*radius);
        s.endpoints[3]=binary64(x87::cos(theta)*center_radius);s.endpoints[4]=binary64(x87::sin(theta)*center_radius);
        s.inner_end[0]=binary64(x87::cos(theta)*inner_radius);s.inner_end[1]=binary64(x87::sin(theta)*inner_radius);
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);collapse_geometry_xy(s);
    }
    const long double theta=static_cast<long double>(number(*head,0xa0))-static_cast<long double>(gap)*0.5L;
    s.outer_end[0]=binary64(x87::cos(theta)*radius);s.outer_end[1]=binary64(x87::sin(theta)*radius);
    s.endpoints[3]=binary64(static_cast<long double>(binary64(x87::cos(theta)))*center_radius);s.endpoints[4]=binary64(x87::sin(theta)*center_radius);
    s.inner_end[0]=binary64(x87::cos(theta)*inner_radius);s.inner_end[1]=binary64(x87::sin(theta)*inner_radius);
    prepend_geometry_segment(head->filaments.first,s,runtime.geometry);reverse_geometry_segments(*head);
    double x0,y0,x1,y1;geometry_bounds_xy(*head,&x0,&y0,&x1,&y1);
    const long double dx=(static_cast<long double>(x1)-x0)*0.5L,dy=(static_cast<long double>(y1)-y0)*0.5L;const unsigned offsets[]={0x88,0xa0,0,0x18,0x44,0x5c};
    for(auto* p=first(*head);p;p=segment(p->next_address)) {
        for(unsigned offset:offsets)field(*p,offset)=binary64(static_cast<long double>(field(*p,offset))+dx);
        for(unsigned offset:offsets)field(*p,offset+8)=binary64(static_cast<long double>(field(*p,offset+8))+dy);
    }
}
}
