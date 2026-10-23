#include "asitic/shape_construction.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
ConductorSegment* segment(unsigned p) { return reinterpret_cast<ConductorSegment*>(p); }
ConductorSegment* first(const GeometryShape& shape) { return const_cast<ConductorSegment*>(shape.filaments.first); }
double& value(ConductorSegment& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
const double& value(const ConductorSegment& s,unsigned offset) { return *reinterpret_cast<const double*>(reinterpret_cast<const unsigned char*>(&s)+offset); }
double shape_value(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const double*>(s.filaments.prefix+offset); }
int shape_integer(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const int*>(s.filaments.prefix+offset); }
void floating_copy(double& out,const double& in) { asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st"); }
long double floating_load(const double& in) {
    long double out;asm volatile("fldl %1; fstpt %0" : "=m"(out) : "m"(in) : "st","memory");return out;
}
long double extreme(const ConductorSegment& s,double accumulator,unsigned axis,bool maximum) {
    long double result=accumulator;const unsigned offsets[]={0x44,0x5c,0x88,0xa0,0,0x18};
    for(unsigned offset:offsets) { const long double v=value(s,offset+axis);if(maximum?result<v:v<result)result=v; }
    return result;
}
void set_metals(ConductorSegment& s,int metal) {
    const unsigned offsets[]={0xdc,0x84,0xc8,0x40};for(unsigned offset:offsets)*reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset)=metal;
}
void set_heights(ConductorSegment& s,int metal,const GeometryTechnology& technology) {
    const unsigned offsets[]={0x6c,0x54,0xb0,0x98,0x28,0x10};
    for(unsigned offset:offsets)floating_copy(value(s,offset),technology.metal_heights[metal]);
}
void set_thickness(ConductorSegment& s,int metal,const GeometryTechnology& technology) {
    const long double thickness=floating_load(technology.metals[metal].thickness);
    const unsigned offsets[]={0x7c,0xc0,0x38,0xd4};
    for(unsigned offset:offsets)value(s,offset)=binary64(thickness);
}
}
long double geometry_minimum_y(const ConductorSegment& s,double accumulator) { return extreme(s,accumulator,8,false); }
long double geometry_minimum_x(const ConductorSegment& s,double accumulator) { return extreme(s,accumulator,0,false); }
long double geometry_maximum_y(const ConductorSegment& s,double accumulator) { return extreme(s,accumulator,8,true); }
long double geometry_maximum_x(const ConductorSegment& s,double accumulator) { return extreme(s,accumulator,0,true); }
void geometry_minimum_xy(const GeometryShape& shape,double* x,double* y) {
    long double min_x=1e15L,min_y=1e15L;
    for(auto* s=first(shape);s;s=segment(s->next_address)) {
        min_x=geometry_minimum_x(*s,binary64(min_x));min_y=geometry_minimum_y(*s,binary64(min_y));
        for(auto* child=segment(s->group_next_address);child;child=segment(child->group_next_address)) {
            min_x=geometry_minimum_x(*child,binary64(min_x));min_y=geometry_minimum_y(*child,binary64(min_y));
        }
    }
    *x=binary64(min_x);*y=binary64(min_y);
}
void geometry_bounds_xy(const GeometryShape& shape,double* xmin,double* ymin,double* xmax,double* ymax) {
    double x0=1e15,y0=1e15,x1=-1e15,y1=-1e15;
    const auto include=[&](const ConductorSegment& s) {
        x1=binary64(geometry_maximum_x(s,x1));x0=binary64(geometry_minimum_x(s,x0));
        y1=binary64(geometry_maximum_y(s,y1));y0=binary64(geometry_minimum_y(s,y0));
    };
    for(auto* s=first(shape);s;s=segment(s->next_address)) { include(*s);for(auto* child=segment(s->group_next_address);child;child=segment(child->group_next_address))include(*child); }
    *xmin=x0;*ymin=y0;*xmax=x1;*ymax=y1;
}
void rotate_geometry_point(double* point,double angle,double center_x,double center_y) {
    const long double x=static_cast<long double>(point[0])-center_x,y=static_cast<long double>(point[1])-center_y;
    const long double radius=x87::sqrt(x*x+y*y);
    if(0x1.b7cdfd9d7bdbbp-34L<radius) {
        const double polar=binary64(x87::atan2(y,x));
        const long double rotated=static_cast<long double>(polar)+angle;
        const double saved=binary64(rotated);
        point[0]=binary64(x87::cos(rotated)*radius+center_x);point[1]=binary64(x87::sin(saved)*radius+center_y);
    }
}
void rotate_geometry_segment(ConductorSegment& s,double angle,double x,double y) {
    const unsigned offsets[]={0x44,0x5c,0x88,0xa0,0,0x18};
    for(unsigned offset:offsets)rotate_geometry_point(&value(s,offset),angle,x,y);
}
void rotate_geometry_shape(GeometryShape& shape,double angle) {
    double x0,y0,x1,y1;geometry_bounds_xy(shape,&x0,&y0,&x1,&y1);
    const double center_x=binary64(static_cast<long double>(x0)+(static_cast<long double>(x1)-x0)*0.5L);
    const double center_y=binary64(static_cast<long double>(y0)+(static_cast<long double>(y1)-y0)*0.5L);
    for(auto* s=first(shape);s;s=segment(s->next_address)) {
        rotate_geometry_segment(*s,angle,center_x,center_y);
        for(auto* child=segment(s->group_next_address);child;child=segment(child->group_next_address))rotate_geometry_segment(*child,angle,center_x,center_y);
    }
    geometry_minimum_xy(shape,&x0,&y0);
    GeometryShape previous;floating_copy(*reinterpret_cast<double*>(previous.filaments.prefix+0x54),*reinterpret_cast<const double*>(shape.filaments.prefix+0x54));
    floating_copy(*reinterpret_cast<double*>(previous.filaments.prefix+0x5c),*reinterpret_cast<const double*>(shape.filaments.prefix+0x5c));
    *reinterpret_cast<double*>(shape.filaments.prefix+0x54)=binary64(static_cast<long double>(shape_value(shape,0x54))+x0);
    *reinterpret_cast<double*>(shape.filaments.prefix+0x5c)=binary64(static_cast<long double>(shape_value(shape,0x5c))+y0);
    translate_geometry_shape(shape,previous);
}
void reverse_geometry_endpoints(ConductorSegment& s) {
    for(unsigned axis=0;axis<24;axis+=8) {
        const long double outer=value(s,0x44+axis),inner=value(s,0x88+axis),primary=value(s,axis);
        floating_copy(value(s,0x44+axis),value(s,0x5c+axis));value(s,0x5c+axis)=binary64(outer);
        floating_copy(value(s,0x88+axis),value(s,0xa0+axis));value(s,0xa0+axis)=binary64(inner);
        floating_copy(value(s,axis),value(s,0x18+axis));value(s,0x18+axis)=binary64(primary);
    }
}
void reverse_geometry_directions(GeometryShape& shape) {
    for(auto* s=first(shape);s;s=segment(s->next_address)) { reverse_geometry_endpoints(*s);for(auto* child=segment(s->group_next_address);child;child=segment(child->group_next_address))reverse_geometry_endpoints(*child); }
}
void offset_geometry_shape(GeometryShape& shape,double x,double y) {
    const unsigned offsets[]={0x88,0xa0,0,0x18,0x44,0x5c};
    for(auto* s=first(shape);s;s=segment(s->next_address)) {
        for(unsigned offset:offsets)value(*s,offset)=binary64(static_cast<long double>(value(*s,offset))+x);
        for(unsigned offset:offsets)value(*s,offset+8)=binary64(static_cast<long double>(value(*s,offset+8))+y);
    }
}
void initialize_geometry_metal(ConductorSegment& s,const GeometryShape& shape,const GeometryTechnology& technology) {
    const int metal=shape_integer(shape,0x68);s.group_next_address=0;s.metal=metal;
    floating_copy(s.width,*reinterpret_cast<const double*>(shape.filaments.prefix+0x88));const long double thickness=floating_load(technology.metals[metal].thickness);s.thickness=binary64(thickness);
    const unsigned heights[]={0xb0,0x98,0x6c,0x54,0x28,0x10};for(unsigned offset:heights)floating_copy(value(s,offset),technology.metal_heights[metal]);
    const unsigned widths[]={0x30,0x74,0xb8};
    for(unsigned offset:widths)floating_copy(value(s,offset),*reinterpret_cast<const double*>(shape.filaments.prefix+0x88));
    const unsigned metals[]={0x40,0x84,0xc8};for(unsigned offset:metals)*reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset)=metal;
    const unsigned thicknesses[]={0x38,0xc0,0x7c};for(unsigned offset:thicknesses)value(s,offset)=binary64(thickness);
}
void set_geometry_metal(ConductorSegment& s,int metal,const GeometryTechnology& technology) { set_metals(s,metal);set_heights(s,metal,technology);set_thickness(s,metal,technology); }
void refresh_geometry_metal(ConductorSegment& s,int metal,const GeometryTechnology& technology) {
    set_heights(s,metal,technology);const long double thickness=floating_load(technology.metals[metal].thickness);
    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+0x40)=metal;
    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+0xc8)=metal;*reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+0x84)=metal;s.metal=metal;
    const unsigned offsets[]={0x38,0xc0,0x7c,0xd4};for(unsigned offset:offsets)value(s,offset)=binary64(thickness);
}
void collapse_geometry_xy(ConductorSegment& s) {
    const unsigned offsets[]={0x44,0x4c,0x88,0x90,0,8};for(unsigned offset:offsets)floating_copy(value(s,offset),value(s,offset+0x18));
}
void extend_geometry_xy(unsigned direction,ConductorSegment& s,double length,double inset) {
    collapse_geometry_xy(s);long double saved_inset=inset;if(direction>1)saved_inset+=saved_inset;
    const long double sign=direction==0 || direction==3?1.L:-1.L;
    const unsigned axis=direction<2?8:0;
    value(s,0x5c+axis)=binary64(sign*length+value(s,0x5c+axis));
    value(s,0xa0+axis)=binary64(sign*(static_cast<long double>(length)-saved_inset)+value(s,0xa0+axis));
    value(s,0x18+axis)=binary64(sign*(static_cast<long double>(length)-saved_inset*0.5L)+value(s,0x18+axis));
}
void build_wire_shape(GeometryShape*& head,const GeometryShape& arguments,const GeometryTechnology& technology,const GeometryRuntime& runtime,WireConstructionHistory history) {
    prepend_geometry_shape(head,runtime);copy_geometry_shape_fields(*head,arguments);head->filaments.first=nullptr;
    ConductorSegment s;const int metal=shape_integer(arguments,0x68);
    const long double length=shape_value(*head,0x70),width=shape_value(*head,0x88);const double angle=shape_value(*head,0xa0);
    floating_copy(s.width,*reinterpret_cast<const double*>(arguments.filaments.prefix+0x88));const long double thickness=floating_load(technology.metals[metal].thickness);
    for(unsigned block=0;block<=0x88;block+=0x44) {
        value(s,block)=0;value(s,block+0x18)=binary64(length);
        value(s,block+8)=value(s,block+0x20)=binary64(block==0?width*0.5L:width);
        floating_copy(value(s,block+0x10),technology.metal_heights[metal]);floating_copy(value(s,block+0x28),technology.metal_heights[metal]);
        floating_copy(value(s,block+0x30),*reinterpret_cast<const double*>(arguments.filaments.prefix+0x88));value(s,block+0x38)=binary64(thickness);
        *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+block+0x40)=metal;
    }
    // Outer edge lies at y=0; inner edge lies at y=width.
    s.outer_start[1]=s.outer_end[1]=0;
    s.thickness=binary64(thickness);
    s.metal=metal;s.width_count=history.width_count;s.thickness_count=history.thickness_count;s.group_next_address=0;s.next_address=0;
    prepend_geometry_segment(head->filaments.first,s,runtime);
    if(0x1.b7cdfd9d7bdbbp-34L<x87::abs(angle))rotate_geometry_shape(*head,angle);
    if(static_cast<int>(head->attributes)<0)reverse_geometry_directions(*head);
}
void build_via_shape(GeometryShape*& head,const GeometryShape& arguments,const GeometryTechnology& technology,const GeometryRuntime& runtime,const ViaConstructionHistory& history) {
    prepend_geometry_shape(head,runtime);copy_geometry_shape_fields(*head,arguments);head->filaments.first=nullptr;
    const int metal=shape_integer(arguments,0x68),nx=shape_integer(arguments,0x64),ny=shape_integer(arguments,0x6c);
    const auto* via=reinterpret_cast<const unsigned char*>(technology.vias)+(static_cast<unsigned>(metal)-static_cast<unsigned>(technology.metal_count))*0xf0u;
    const long double step=*reinterpret_cast<const double*>(via+0xa0),space=*reinterpret_cast<const double*>(via+0xa8);
    const long double height=step*ny+space*(static_cast<long double>(ny)-1.L);
    const long double length=(static_cast<long double>(nx)-1.L)*space+step*nx;
    const double width=binary64(height);const long double left=-length*0.5L,right=length*0.5L,top=height*0.5L,bottom=-height*0.5L;
    const int lower=*reinterpret_cast<const int*>(via+0xcc),upper=*reinterpret_cast<const int*>(via+0xd0);
    ConductorSegment s;
    for(unsigned block=0;block<=0x88;block+=0x44) {
        value(s,block)=binary64(left);value(s,block+0x18)=binary64(right);
        value(s,block+8)=value(s,block+0x20)=block==0?0:binary64(block==0x44?top:bottom);
        floating_copy(value(s,block+0x10),technology.metal_heights[lower]);floating_copy(value(s,block+0x28),technology.metal_heights[upper]);
        value(s,block+0x30)=width;
        *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+block+0x40)=metal;
    }
    s.width=width;s.metal=metal;s.width_count=nx;s.thickness_count=ny;s.group_next_address=0;s.next_address=0;
    const unsigned offsets[]={0x38,0x7c,0xc0,0xd4};
    for(unsigned i=0;i<4;++i) {
        auto* words=reinterpret_cast<unsigned*>(reinterpret_cast<unsigned char*>(&s)+offsets[i]);
        words[0]=history.thickness[2*i];words[1]=history.thickness[2*i+1];
    }
    prepend_geometry_segment(head->filaments.first,s,runtime);
    if(static_cast<int>(head->attributes)<0)reverse_geometry_directions(*head);
}
}
