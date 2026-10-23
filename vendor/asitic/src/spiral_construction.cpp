#include "asitic/spiral_construction.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
double& field(ConductorSegment& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
double number(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const double*>(s.filaments.prefix+offset); }
int integer(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const int*>(s.filaments.prefix+offset); }
ConductorSegment* front(GeometryShape& s) { return const_cast<ConductorSegment*>(s.filaments.first); }
int increment(int value) { return static_cast<int>(static_cast<unsigned>(value)+1); }
void floating_copy(double& out,const double& in) { asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st"); }
void raw_thickness(ConductorSegment& s,const ViaConstructionHistory& history) {
    const unsigned offsets[]={0x38,0x7c,0xc0,0xd4};for(unsigned i=0;i<4;++i) {
        auto* out=reinterpret_cast<unsigned*>(reinterpret_cast<unsigned char*>(&s)+offsets[i]);out[0]=history.thickness[2*i];out[1]=history.thickness[2*i+1];
    }
}
void xy(ConductorSegment& s,long double px,long double py,long double qx,long double qy,
    long double ox,long double oy,long double ex,long double ey,long double ix,long double iy,long double jx,long double jy) {
    s.endpoints[0]=binary64(px);s.endpoints[1]=binary64(py);s.endpoints[3]=binary64(qx);s.endpoints[4]=binary64(qy);
    s.outer_start[0]=binary64(ox);s.outer_start[1]=binary64(oy);s.outer_end[0]=binary64(ex);s.outer_end[1]=binary64(ey);
    s.inner_start[0]=binary64(ix);s.inner_start[1]=binary64(iy);s.inner_end[0]=binary64(jx);s.inner_end[1]=binary64(jy);
}
void mesh(ConductorSegment& s,WireConstructionHistory history) { s.width_count=history.width_count;s.thickness_count=history.thickness_count;s.next_address=0; }
double floor_binary64(double value) {
    unsigned short saved,temporary;long double result;
    asm volatile("fnstcw %0" : "=m"(saved));temporary=(saved&0xf3ffu)|0x0400u;
    asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(result) : "0"(static_cast<long double>(value)),"m"(temporary),"m"(saved));return binary64(result);
}
}
void emit_geometry_via(GeometryShape& shape,int nx,int ny,double x,double y,int via_index,const GeometryTechnology& technology,const GeometryRuntime& runtime,const ViaConstructionHistory& history) {
    const auto* via=reinterpret_cast<const unsigned char*>(technology.vias)+static_cast<unsigned>(via_index)*0xf0u;
    const long double step=*reinterpret_cast<const double*>(via+0xa0),space=*reinterpret_cast<const double*>(via+0xa8);
    const long double height=step*ny+space*(static_cast<long double>(ny)-1.L);
    const long double half_width=((static_cast<long double>(nx)-1.L)*space+step*nx)*0.5L;
    const long double left=static_cast<long double>(x)-half_width,right=static_cast<long double>(x)+half_width;
    const long double top=static_cast<long double>(y)+height*0.5L,bottom=static_cast<long double>(y)-height*0.5L;
    const int lower=*reinterpret_cast<const int*>(via+0xcc),upper=*reinterpret_cast<const int*>(via+0xd0),metal=static_cast<int>(static_cast<unsigned>(via_index)+static_cast<unsigned>(technology.metal_count));
    ConductorSegment s;
    xy(s,left,y,right,y,left,top,right,top,left,bottom,right,bottom);
    for(unsigned block=0;block<=0x88;block+=0x44) {
        floating_copy(field(s,block+0x10),technology.metal_heights[lower]);floating_copy(field(s,block+0x28),technology.metal_heights[upper]);field(s,block+0x30)=binary64(height);
        *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+block+0x40)=metal;
    }
    raw_thickness(s,history);s.width=binary64(height);s.metal=metal;s.width_count=nx;s.thickness_count=ny;s.group_next_address=0;s.next_address=history.next_address;
    prepend_geometry_segment(shape.filaments.first,s,runtime);
}
void emit_geometry_via_for_pair(GeometryShape& shape,int a,int b,double width,double x,double y,int dx,int dy,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const ViaConstructionHistory& history) {
    if(a==b || runtime.via_count==0)return;
    int index=-1;
    for(int i=0;i<runtime.via_count;++i) {
        const auto* p=reinterpret_cast<const unsigned char*>(technology.vias)+static_cast<unsigned>(i)*0xf0u;
        const int lower=*reinterpret_cast<const int*>(p+0xcc),upper=*reinterpret_cast<const int*>(p+0xd0);
        if((upper==a && lower==b) || (lower==a && upper==b)) { index=i;break; }
    }
    if(index<0) { runtime.format(runtime.message,"Warning:  No via layer connects metal %d to metal %d.",a,b);runtime.warnings.emit(runtime.message);return; }
    const auto* p=reinterpret_cast<const unsigned char*>(technology.vias)+static_cast<unsigned>(index)*0xf0u;
    const long double first=*reinterpret_cast<const double*>(p+0xb0),second=*reinterpret_cast<const double*>(p+0xb8);
    const long double overplot=second<first?first:second,space=*reinterpret_cast<const double*>(p+0xa8),step=*reinterpret_cast<const double*>(p+0xa0);
    const int count=x87::truncate_int(((static_cast<long double>(width)-(overplot+overplot))+space)/(space+step));
    const long double radius=(((static_cast<long double>(count)-1.L)*space+static_cast<long double>(count)*step)*0.5L)+overplot;
    if(count==0) { runtime.warnings.emit("Warning:  Insufficient space to insert via.");return; }
    emit_geometry_via(shape,count,count,binary64(radius*dx+x),binary64(radius*dy+y),index,technology,runtime.geometry,history);
}
void extend_geometry_head(GeometryShape& shape) {
    auto& s=*front(shape);double direction[3];
    // Native normalize-difference loads the end first. Keep that order even
    // for an empty shape, whose first access faults at segment+0x18.
    for(unsigned i=0;i<3;++i)asm volatile("fldl %1; fsubl %2; fstpl %0" : "=m"(direction[i]) : "m"(s.endpoints[i+3]),"m"(s.endpoints[i]) : "st","memory");
    const long double dx=direction[0],dy=direction[1],dz=direction[2],norm=x87::sqrt((dx*dx+dy*dy)+dz*dz);
    for(unsigned i=0;i<3;++i)direction[i]=binary64(static_cast<long double>(direction[i])/norm);
    const long double x=static_cast<long double>(s.endpoints[3])-s.endpoints[0],y=static_cast<long double>(s.endpoints[4])-s.endpoints[1],z=static_cast<long double>(s.endpoints[5])-s.endpoints[2];
    const long double length=x87::sqrt((x*x+y*y)+z*z)*0.5L+s.width;
    double inner[3],outer[3];for(unsigned i=0;i<3;++i)inner[i]=binary64(static_cast<long double>(s.inner_start[i])-s.endpoints[i]);
    for(unsigned i=0;i<3;++i)outer[i]=binary64(static_cast<long double>(s.outer_start[i])-s.endpoints[i]);
    long double next[3];for(unsigned i=0;i<3;++i) { next[i]=static_cast<long double>(s.endpoints[i+3])-length*direction[i];s.endpoints[i]=binary64(next[i]); }
    for(unsigned i=0;i<3;++i)s.inner_start[i]=binary64(next[i]+inner[i]);
    for(unsigned i=0;i<3;++i)s.outer_start[i]=binary64(next[i]+outer[i]);
}
void build_square_spiral(GeometryShape*& head,const GeometryShape& args,int mode,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const SquareConstructionHistory& history) {
    prepend_geometry_shape(head,runtime.geometry);copy_geometry_shape_fields(*head,args);head->filaments.first=nullptr;
    const double dimension_x=number(*head,0x70),dimension_y=number(*head,0x78),spacing=number(*head,0x90),turns=number(*head,0x98),width=number(*head,0x88),angle=number(*head,0xa0);
    ConductorSegment s;initialize_geometry_metal(s,args,technology);mesh(s,history.mesh);
    const int full=x87::truncate_int(turns),last=increment(full);unsigned phase=0;
    for(int turn=1,count=0;turn<=last;turn=increment(turn),count=increment(count)) {
        phase=0;const int fraction=x87::truncate_int(static_cast<long double>(turns)*4.L-static_cast<long double>(full)*4.L);
        if(turn==last && fraction==0) { auto* tail=front(*head);tail->endpoints[4]=binary64(static_cast<long double>(tail->inner_end[1])-s.width);floating_copy(tail->outer_end[1],tail->inner_end[1]);break; }
        const long double raw_pitch=static_cast<long double>(width)+spacing;const double pitch=binary64(raw_pitch);const long double shift=static_cast<long double>(pitch)*count;
        const long double half=static_cast<long double>(width)*0.5L,x1=static_cast<long double>(dimension_x)-shift,y1=static_cast<long double>(dimension_y)-shift,inner_x=x1-width,inner_y=y1-width,center_x=x1-half,center_y=y1-half;
        const long double start_outer=turn==1?0.L:static_cast<long double>(pitch)*(turn-2);
        const long double start_inner=turn==1?0.L:start_outer+width,start_primary=turn==1?0.L:start_outer+half;
        const bool extra=turn==1 && integer(*head,0x50)==5;
        xy(s,extra?-raw_pitch:start_primary,center_y,center_x,center_y,extra?-raw_pitch:start_outer,y1,x1,y1,extra?-raw_pitch:start_inner,inner_y,inner_x,inner_y);
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);phase=1;
        if(turn==last && fraction==1) { auto* tail=front(*head);tail->endpoints[3]=binary64(static_cast<long double>(tail->inner_end[0])-s.width);floating_copy(tail->outer_end[0],tail->inner_end[0]);break; }
        const long double low_inner=shift+width,low_center=shift+half;
        xy(s,center_x,center_y,center_x,low_center,x1,y1,x1,shift,inner_x,inner_y,inner_x,low_inner);
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);phase=2;
        if(turn==last && fraction==2) { auto* tail=front(*head);tail->endpoints[4]=binary64(static_cast<long double>(tail->inner_end[1])+s.width);floating_copy(tail->outer_end[1],tail->inner_end[1]);break; }
        xy(s,center_x,low_center,low_center,low_center,x1,shift,shift,shift,inner_x,low_inner,low_inner,low_inner);
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);phase=3;
        if(turn==last && fraction==3) { auto* tail=front(*head);tail->endpoints[3]=binary64(static_cast<long double>(tail->inner_end[0])+s.width);floating_copy(tail->outer_end[0],tail->inner_end[0]);break; }
        const long double next_y=static_cast<long double>(dimension_y)-static_cast<long double>(pitch)*turn;
        xy(s,low_center,low_center,low_center,next_y-half,shift,shift,shift,next_y,low_inner,low_inner,low_inner,next_y-width);
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);
    }
    auto* tail=front(*head);
    if(integer(args,0x6c)>=0) {
        const int direction_x=phase==1?-1:phase==3?1:0,direction_y=phase==0?-1:phase==2?1:0;
        emit_geometry_via_for_pair(*head,integer(args,0x68),integer(args,0x6c),number(args,0x88),
            binary64(static_cast<long double>(s.width)*direction_x+s.endpoints[3]),binary64(static_cast<long double>(s.width)*direction_y+s.endpoints[4]),0,0,technology,runtime,history.via);
        const int metal=integer(args,0x6c);const unsigned heights[]={0xb0,0x98,0x6c,0x54,0x28,0x10};for(unsigned offset:heights)floating_copy(field(s,offset),technology.metal_heights[metal]);
        s.outer_start[0]=tail->outer_end[0];s.outer_start[1]=tail->outer_end[1];s.inner_start[0]=tail->inner_end[0];s.inner_start[1]=tail->inner_end[1];s.endpoints[0]=tail->endpoints[3];s.endpoints[1]=tail->endpoints[4];
        if(phase==1 || phase==3) {
            const long double delta=phase==1?-static_cast<long double>(s.width):s.width;
            s.outer_start[0]=binary64(static_cast<long double>(s.outer_start[0])+delta);s.inner_start[0]=binary64(static_cast<long double>(s.inner_start[0])+delta);
            s.endpoints[3]=binary64(phase==1?static_cast<long double>(s.width)+dimension_x:-static_cast<long double>(s.width));s.endpoints[4]=s.endpoints[1];s.outer_end[0]=s.inner_end[0]=s.endpoints[3];s.outer_end[1]=s.outer_start[1];s.inner_end[1]=s.inner_start[1];
        } else {
            const long double delta=phase==0?-static_cast<long double>(s.width):s.width;
            s.outer_start[1]=binary64(static_cast<long double>(s.outer_start[1])+delta);s.inner_start[1]=binary64(static_cast<long double>(s.inner_start[1])+delta);
            s.endpoints[4]=binary64(phase==0?static_cast<long double>(s.width)+dimension_y:-static_cast<long double>(s.width));s.endpoints[3]=s.endpoints[0];s.outer_end[1]=s.inner_end[1]=s.endpoints[4];s.outer_end[0]=s.outer_start[0];s.inner_end[0]=s.inner_start[0];
        }
        const unsigned metals[]={0x40,0x84,0xc8,0xdc};for(unsigned offset:metals)*reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset)=metal;
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);
    } else {
        const unsigned axis=phase==1 || phase==3?0:1;const long double delta=phase==0 || phase==1?static_cast<long double>(s.width):-static_cast<long double>(s.width);
        tail->inner_end[axis]=binary64(static_cast<long double>(tail->inner_end[axis])+delta);floating_copy(tail->endpoints[3+axis],tail->inner_end[axis]);floating_copy(tail->outer_end[axis],tail->inner_end[axis]);
    }
    if(0x1.b7cdfd9d7bdbbp-34L<x87::abs(angle))rotate_geometry_shape(*head,angle);
    reverse_geometry_segments(*head);if(static_cast<int>(head->attributes)<0)reverse_geometry_directions(*head);if(mode==0 || mode==2)extend_geometry_head(*head);
}
void build_polygon_spiral(GeometryShape*& head,const GeometryShape& args,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,WireConstructionHistory history) {
    prepend_geometry_shape(head,runtime.geometry);copy_geometry_shape_fields(*head,args);head->filaments.first=nullptr;
    const int sides=integer(*head,0x64);const double n=sides,turns=number(*head,0x98),width=number(*head,0x88),spacing=number(*head,0x90);
    const long double angle=number(*head,0xa0),initial_radius=number(*head,0x80);
    const long double sector=0x1.921fb54442d18p+1L/n;
    const double ribbon=binary64(static_cast<long double>(width)/x87::cos(sector));
    const double shrink=binary64(((static_cast<long double>(width)+spacing)*turns)/x87::cos(binary64(sector)));
    const double decrement=binary64((initial_radius-(initial_radius-shrink))/(static_cast<long double>(turns)*n));
    ConductorSegment s;initialize_geometry_metal(s,args,technology);mesh(s,history);
    const double half_ribbon=binary64(static_cast<long double>(ribbon)*0.5L);long double radius=initial_radius;
    for(int turn=1;turn<=x87::truncate_int(static_cast<long double>(turns)+1.L);turn=increment(turn)) {
        s.outer_start[0]=binary64(x87::cos(angle)*radius);s.outer_start[1]=binary64(x87::sin(angle)*radius);
        s.endpoints[0]=binary64(x87::cos(angle)*(radius-half_ribbon));s.endpoints[1]=binary64(x87::sin(angle)*(radius-half_ribbon));
        s.inner_start[0]=binary64(x87::cos(angle)*(radius-ribbon));s.inner_start[1]=binary64(x87::sin(angle)*(radius-ribbon));
        const int count=turn==x87::truncate_int(static_cast<long double>(turns)+1.L)?
            x87::truncate_int((static_cast<long double>(turns)-floor_binary64(turns))*n+1.L/(static_cast<long double>(n)+n)):sides;
        for(int side=1;side<=count;side=increment(side)) {
            radius-=decrement;
            if(radius<0x1.b7cdfd9d7bdbbp-34L) {
                remove_geometry_shape(head,head,runtime.geometry.memory);
                if(runtime.structure_violation)runtime.structure_violation(runtime.user);
                __builtin_trap();
            }
            const long double theta=(0x1.921fb54442d18p+2L*side)/n+angle;
            s.outer_end[0]=binary64(x87::cos(theta)*radius);s.outer_end[1]=binary64(x87::sin(theta)*radius);
            s.endpoints[3]=binary64(x87::cos(theta)*(radius-static_cast<long double>(ribbon)*0.5L));s.endpoints[4]=binary64(x87::sin(theta)*(radius-static_cast<long double>(ribbon)*0.5L));
            s.inner_end[0]=binary64(x87::cos(theta)*(radius-ribbon));s.inner_end[1]=binary64(x87::sin(theta)*(radius-ribbon));
            prepend_geometry_segment(head->filaments.first,s,runtime.geometry);collapse_geometry_xy(s);
        }
    }
    reverse_geometry_segments(*head);
    double x0,y0,x1,y1;geometry_bounds_xy(*head,&x0,&y0,&x1,&y1);
    offset_geometry_shape(*head,binary64((static_cast<long double>(x1)-x0)*0.5L),binary64((static_cast<long double>(y1)-y0)*0.5L));
}
}
