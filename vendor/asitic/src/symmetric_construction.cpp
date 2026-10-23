#include "asitic/symmetric_construction.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
constexpr long double pi=0x1.921fb54442d18p+1L;
ConductorSegment* segment(unsigned address) { return reinterpret_cast<ConductorSegment*>(address); }
ConductorSegment* front(GeometryShape& shape) { return const_cast<ConductorSegment*>(shape.filaments.first); }
double number(const GeometryShape& shape,unsigned offset) { return *reinterpret_cast<const double*>(shape.filaments.prefix+offset); }
int integer(const GeometryShape& shape,unsigned offset) { return *reinterpret_cast<const int*>(shape.filaments.prefix+offset); }
void floating_copy(double& out,const double& in) { asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st","memory"); }
void mesh(ConductorSegment& s,WireConstructionHistory history) { s.width_count=history.width_count;s.thickness_count=history.thickness_count;s.group_next_address=s.next_address=0; }
struct Vias {
    const ViaConstructionHistory* records;unsigned index=0;ViaConstructionHistory zero{};
    void emit(GeometryShape& shape,int first,int second,double width,double x,double y,int dy,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime) {
        const auto* before=shape.filaments.first;
        emit_geometry_via_for_pair(shape,first,second,width,x,y,0,dy,technology,runtime,records?records[index]:zero);
        if(shape.filaments.first!=before)++index;
    }
};
void endpoint(ConductorSegment& s,double angle,long double outer_angle,double radius,double width,double n,long double half,bool initial) {
    const long double sector=pi/n;
    s.inner_end[0]=binary64(x87::cos(outer_angle)*radius);
    const long double outer_y=x87::sin(angle)*radius;
    s.inner_end[1]=binary64((initial?static_cast<long double>(binary64(outer_y)):outer_y)+half);
    const double c=binary64(x87::cos(sector));
    s.endpoints[3]=binary64(x87::cos(angle)*(static_cast<long double>(radius)-static_cast<long double>(width)/(static_cast<long double>(c)+c)));
    s.endpoints[4]=binary64((static_cast<long double>(radius)-static_cast<long double>(width)/(static_cast<long double>(c)+c))*x87::sin(angle)+half);
    s.outer_end[0]=binary64(x87::cos(angle)*(static_cast<long double>(radius)-static_cast<long double>(width)/c));
    s.outer_end[1]=binary64(x87::sin(angle)*(static_cast<long double>(radius)-static_cast<long double>(width)/c)+half);
}
}
void bridge_symmetric_geometry(int direction,ConductorSegment& s,double height,double width,double spacing) {
    const long double h=height,w=width,gap=spacing;
    const double hypotenuse=binary64(x87::sqrt(h*h+gap*gap));
    const long double angle=x87::atan2(h/gap,1.L),ratio=w/hypotenuse;
    const double arc=binary64(x87::atan2(x87::sqrt(1.L-ratio*ratio),ratio));
    const long double difference=(pi-angle)-arc;volatile double discarded=binary64(difference);discarded=binary64(difference*0.5L);(void)discarded;
    const long double half=difference*0.5L;
    asm volatile("fptan; fstp %%st(0); fstp %%st(0)" :: "t"(half) : "st","st(1)","memory");
    collapse_geometry_xy(s);
    long double sx=0,sy=0;
    if(direction==4) { sx=-1;sy=1; }else if(direction==5) { sx=1;sy=1; }else if(direction==6) { sx=-1;sy=-1; }else if(direction==7) { sx=1;sy=-1; }
    const long double dx=(gap+w)*sx;
    s.inner_end[0]=binary64(static_cast<long double>(s.inner_end[0])+dx);s.outer_end[0]=binary64(static_cast<long double>(s.outer_end[0])+dx);s.endpoints[3]=binary64(dx+s.endpoints[3]);
    // The subtraction from zero and multiplication by the sign both round at
    // the caller's x87 precision, even when the sign is one.
    long double rounded_height,dy;
    asm volatile("fldz; fsubrl %1; fstpt %0" : "=m"(rounded_height) : "m"(height) : "st","memory");
    asm volatile("fldt %1; fldt %2; fmulp; fstpt %0" : "=m"(dy) : "m"(rounded_height),"m"(sy) : "st","st(1)","memory");
    s.inner_end[1]=binary64(static_cast<long double>(s.inner_end[1])+dy);s.outer_end[1]=binary64(static_cast<long double>(s.outer_end[1])+dy);s.endpoints[4]=binary64(dy+s.endpoints[4]);
}
void build_symmetric_square(GeometryShape*& head,const GeometryShape& args,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const SymmetricConstructionHistory& history) {
    prepend_geometry_shape(head,runtime.geometry);copy_geometry_shape_fields(*head,args);head->filaments.first=nullptr;
    double span=number(*head,0x70);const double height=number(*head,0x78),spacing=number(*head,0x90),turns=number(*head,0x98),width=number(*head,0x88),angle=number(*head,0xa0);
    const int primary=integer(*head,0x68),secondary=integer(*head,0x6c);ConductorSegment s;initialize_geometry_metal(s,args,technology);mesh(s,history.mesh);Vias vias{history.vias};
    s.endpoints[3]=binary64(static_cast<long double>(width)*0.5L);s.endpoints[4]=binary64(static_cast<long double>(span)*0.5L+height);
    s.outer_end[0]=0;s.outer_end[1]=s.endpoints[4];s.inner_end[0]=width;s.inner_end[1]=s.endpoints[4];
    double vertical=binary64((static_cast<long double>(span)-height)*0.5L),progress=0.;unsigned state=0;int side=0,bridge_metal=primary;
    if(0.L<=static_cast<long double>(turns)-0.5L)do {
        progress=binary64(static_cast<long double>(progress)+0.25L);++side;
        int selected=primary;if(state>3) { bridge_metal=bridge_metal==primary?secondary:primary;selected=bridge_metal; }
        refresh_geometry_metal(s,selected,technology);
        switch(state) {
            case 0:extend_geometry_xy(0,s,vertical,width);state=side<3?3:5;break;
            case 1:extend_geometry_xy(1,s,vertical,width);state=side<3?2:6;break;
            case 2:extend_geometry_xy(2,s,span,width);state=0;break;
            case 3:extend_geometry_xy(3,s,span,width);state=1;break;
            case 5:case 6: {
                const unsigned direction=state;const int first_y=state==5?-1:1;
                vias.emit(*head,bridge_metal,primary,width,s.endpoints[3],s.endpoints[4],first_y,technology,runtime);
                bridge_symmetric_geometry(static_cast<int>(direction),s,height,width,spacing);
                vias.emit(*head,bridge_metal,primary,width,s.endpoints[3],s.endpoints[4],-first_y,technology,runtime);
                const long double pitch=static_cast<long double>(width)+spacing;
                span=binary64(static_cast<long double>(span)-(pitch+pitch));vertical=binary64(static_cast<long double>(vertical)-pitch);state=direction==5?0:1;side=0;break;
            }
        }
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);
    }while(static_cast<long double>(progress)<=static_cast<long double>(turns)-0.5L);
    collapse_geometry_xy(s);const bool down=state==6;
    const long double center=static_cast<long double>(height)*0.5L;
    s.outer_end[1]=binary64(down?static_cast<long double>(s.outer_end[1])-center:static_cast<long double>(s.outer_end[1])+center);
    s.inner_end[1]=binary64(down?static_cast<long double>(s.inner_end[1])-center:static_cast<long double>(s.inner_end[1])+center);
    s.endpoints[4]=binary64(down?static_cast<long double>(s.endpoints[4])-center:center+s.endpoints[4]);
    prepend_geometry_segment(head->filaments.first,s,runtime.geometry);collapse_geometry_xy(s);
    s.outer_end[1]=binary64(down?static_cast<long double>(s.outer_end[1])-center:static_cast<long double>(s.outer_end[1])+center);
    s.inner_end[1]=binary64(down?static_cast<long double>(s.inner_end[1])-center:static_cast<long double>(s.inner_end[1])+center);
    s.endpoints[4]=binary64(down?static_cast<long double>(s.endpoints[4])-center:center+s.endpoints[4]);
    prepend_geometry_segment(head->filaments.first,s,runtime.geometry);state=down?1:0;side=0;
    while(0.L<progress) {
        progress=binary64(static_cast<long double>(progress)-0.25L);++side;
        int selected=primary;if(state>3) { bridge_metal=bridge_metal==primary?secondary:primary;selected=bridge_metal; }refresh_geometry_metal(s,selected,technology);
        switch(state) {
            case 0:extend_geometry_xy(0,s,vertical,width);state=side<3?3:4;break;
            case 1:extend_geometry_xy(1,s,vertical,width);state=side<3?2:7;break;
            case 2:extend_geometry_xy(2,s,span,width);state=0;break;
            case 3:extend_geometry_xy(3,s,span,width);state=1;break;
            case 4:case 7: {
                const unsigned direction=state;
                vias.emit(*head,bridge_metal,primary,width,s.endpoints[3],s.endpoints[4],-1,technology,runtime);
                bridge_symmetric_geometry(static_cast<int>(direction),s,height,width,spacing);
                vias.emit(*head,bridge_metal,primary,width,s.endpoints[3],s.endpoints[4],1,technology,runtime);
                const long double pitch=static_cast<long double>(width)+spacing;
                span=binary64((pitch+pitch)+span);vertical=binary64(pitch+vertical);state=direction==4?0:1;side=0;break;
            }
        }
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);
    }
    if(0x1.b7cdfd9d7bdbbp-34L<x87::abs(angle))rotate_geometry_shape(*head,angle);
    reverse_geometry_segments(*head);if(static_cast<int>(head->attributes)<0)reverse_geometry_directions(*head);
}
void build_symmetric_polygon(GeometryShape*& head,const GeometryShape& args,const GeometryTechnology& technology,const SpiralConstructionRuntime& runtime,const SymmetricConstructionHistory& history) {
    prepend_geometry_shape(head,runtime.geometry);copy_geometry_shape_fields(*head,args);head->filaments.first=nullptr;
    const double height=number(*head,0x78),spacing=number(*head,0x90),width=number(*head,0x88),turns=number(*head,0x98);double radius=number(*head,0x80),angle=number(*head,0xa0);
    const double n=integer(*head,0x64);const int primary=integer(*head,0x68),secondary=integer(*head,0x6c);int bridge_metal=history.polygon_metal;
    const double step=binary64((pi+pi)/n);ConductorSegment s;mesh(s,history.mesh);floating_copy(s.width,width);
    // Widths precede the native metal setter, which supplies all Z/thickness fields.
    floating_copy(*reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+0x30),width);floating_copy(*reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+0x74),width);floating_copy(*reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+0xb8),width);
    set_geometry_metal(s,primary,technology);Vias vias{history.vias};
    const long double initial_half=static_cast<long double>(height)*0.5L;endpoint(s,angle,angle,radius,width,n,initial_half,true);double half=binary64(initial_half);
    const double limit=binary64(static_cast<long double>(turns)+turns);
    for(int turn=1;static_cast<long double>(turn)<=limit;++turn) {
        for(int side=1;static_cast<long double>(side)<=static_cast<long double>(n)*0.5L;++side) {
            collapse_geometry_xy(s);const long double next=static_cast<long double>(angle)+step;angle=binary64(next);endpoint(s,angle,next,radius,width,n,half,false);
            prepend_geometry_segment(head->filaments.first,s,runtime.geometry);
        }
        half=-half;collapse_geometry_xy(s);if(static_cast<long double>(turn)==limit)break;
        if(static_cast<long double>(turns)==turn) {
            const long double shift=static_cast<long double>(half)+half;
            s.inner_end[1]=binary64(static_cast<long double>(s.inner_end[1])+shift);s.endpoints[4]=binary64(static_cast<long double>(s.endpoints[4])+shift);s.outer_end[1]=binary64(shift+s.outer_end[1]);
        } else {
            bridge_metal=bridge_metal==primary?secondary:primary;set_geometry_metal(s,bridge_metal,technology);
            const long double pitch=static_cast<long double>(width)+spacing;
            const long double radial=pitch/x87::cos(pi/n);const bool inward=static_cast<long double>(turn)<turns;
            radius=binary64(inward?static_cast<long double>(radius)-radial:radial+radius);
            const bool negative=half<0.;const int first_y=negative?1:-1;int direction;
            if(inward)direction=negative?7:4;else direction=negative?6:5;
            vias.emit(*head,bridge_metal,primary,width,s.endpoints[3],s.endpoints[4],first_y,technology,runtime);
            bridge_symmetric_geometry(direction,s,height,width,spacing);
            vias.emit(*head,bridge_metal,primary,width,s.endpoints[3],s.endpoints[4],direction==4 || direction==5?1:-1,technology,runtime);
        }
        prepend_geometry_segment(head->filaments.first,s,runtime.geometry);set_geometry_metal(s,primary,technology);
    }
    reverse_geometry_segments(*head);if(static_cast<int>(head->attributes)<0)reverse_geometry_directions(*head);
}
void extend_geometry_tail_to_bounds(GeometryShape& shape) {
    double x0,y0,x1,y1;geometry_bounds_xy(shape,&x0,&y0,&x1,&y1);auto* p=front(shape);while(p->next_address)p=segment(p->next_address);
    const long double dy=static_cast<long double>(p->endpoints[4])-p->endpoints[1],dx=static_cast<long double>(p->endpoints[3])-p->endpoints[0];
    constexpr long double threshold=0x1.b7cdfd9d7bdbbp-34L;
    if(!(x87::abs(dy)<threshold)) {
        const long double next=threshold<dy?static_cast<long double>(y1)+p->width:static_cast<long double>(y0)-p->width;
        p->endpoints[4]=binary64(next);p->outer_end[1]=binary64(next);p->inner_end[1]=binary64(next);
    } else {
        const long double next=threshold<dx?static_cast<long double>(x1)+p->width:static_cast<long double>(x0)-p->width;
        p->endpoints[3]=binary64(next);p->outer_end[0]=binary64(next);p->inner_end[0]=binary64(next);
    }
}
void extend_geometry_tail(GeometryShape& shape) {
    auto* p=front(shape);while(p->next_address)p=segment(p->next_address);auto& s=*p;double direction[3];
    for(unsigned i=0;i<3;++i)asm volatile("fldl %1; fsubl %2; fstpl %0" : "=m"(direction[i]) : "m"(s.endpoints[i+3]),"m"(s.endpoints[i]) : "st","memory");
    const long double dx=direction[0],dy=direction[1],dz=direction[2],norm=x87::sqrt((dx*dx+dy*dy)+dz*dz);
    for(unsigned i=0;i<3;++i)direction[i]=binary64(static_cast<long double>(direction[i])/norm);
    double inner[3],outer[3];for(unsigned i=0;i<3;++i)inner[i]=binary64(static_cast<long double>(s.inner_end[i])-s.endpoints[i+3]);
    for(unsigned i=0;i<3;++i)outer[i]=binary64(static_cast<long double>(s.outer_end[i])-s.endpoints[i+3]);
    const long double x=static_cast<long double>(s.endpoints[3])-s.endpoints[0],y=static_cast<long double>(s.endpoints[4])-s.endpoints[1],z=static_cast<long double>(s.endpoints[5])-s.endpoints[2];
    const long double length=x87::sqrt((x*x+y*y)+z*z)*0.5L+s.width;long double next[3];
    for(unsigned i=0;i<3;++i) { next[i]=length*direction[i]+s.endpoints[i];s.endpoints[i+3]=binary64(next[i]); }
    for(unsigned i=0;i<3;++i)s.inner_end[i]=binary64(next[i]+inner[i]);
    for(unsigned i=0;i<3;++i)s.outer_end[i]=binary64(next[i]+outer[i]);
}
}
