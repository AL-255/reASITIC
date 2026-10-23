#include "asitic/x11_picking.hpp"
namespace asitic {
namespace {
long double picking_load(const double& value) { long double result;asm volatile("fldl %1; fstpt %0":"=m"(result):"m"(value):"st");return result; }
unsigned picking_compare(long double first,long double second) {
    unsigned short result;asm volatile("fldt %2; fldt %1; fcompp; fnstsw %0":"=m"(result):"m"(first),"m"(second):"st","st(1)");return result&0x4500;
}
double picking_offset(const double& coordinate,const double& origin) {
    double result;asm volatile("fldl %1; faddl %2; fstpl %0":"=m"(result):"m"(coordinate),"m"(origin):"st");return result;
}
const double& picking_field(const void* object,unsigned offset) { return *reinterpret_cast<const double*>(static_cast<const unsigned char*>(object)+offset); }
struct PickingQuery { unsigned words[4]; };
PickingQuery picking_query_bits(const double& px,const double& py) {
    PickingQuery query;
    // GCC otherwise implements a typed double copy with FLD/FSTP, which
    // raises denormal/invalid before an empty walk has inspected any point.
    asm volatile("movl (%1),%%eax; movl %%eax,(%0); movl 4(%1),%%eax; movl %%eax,4(%0); movl (%2),%%eax; movl %%eax,8(%0); movl 4(%2),%%eax; movl %%eax,12(%0)"
        ::"r"(&query),"r"(&px),"r"(&py):"eax","memory");return query;
}
__attribute__((noinline)) bool picking_segment_bits(const GeometryShape& shape,const ConductorSegment& segment,const PickingQuery& query) {
    double x[4],y[4];const unsigned offsets[]={0x44,0x5c,0xa0,0x88};
    for(unsigned i=0;i<4;++i)x[i]=picking_offset(picking_field(&segment,offsets[i]),picking_field(&shape,0x54));
    for(unsigned i=0;i<4;++i)y[i]=picking_offset(picking_field(&segment,offsets[i]+8),picking_field(&shape,0x5c));
    // Query loads belong to the parity routine. In particular, a bad segment
    // must fault before a signaling query NaN can affect floating-point state.
    asm volatile("":::"memory");
    double px,py;__builtin_memcpy(&px,query.words,8);__builtin_memcpy(&py,query.words+2,8);
    return point_in_polygon_parity(4,x,y,px,py)!=0;
}
__attribute__((noinline)) bool picking_shape_bits(const GeometryShape& shape,const PickingQuery& query) {
    for(const ConductorSegment* segment=shape.filaments.first;segment;segment=reinterpret_cast<const ConductorSegment*>(segment->next_address))
        if(picking_segment_bits(shape,*segment,query))return true;
    return false;
}
}
int point_in_polygon_parity(int count,const double* x,const double* y,double px,double py) {
    const long double query_x=picking_load(px),query_y=picking_load(py);int inside=0;
    int previous=static_cast<int>(static_cast<unsigned>(count)-1u);
    for(int i=0;i<count;++i) {
        const long double current_y=picking_load(y[i]);
        const unsigned first=picking_compare(current_y,query_y);
        bool crossing=false;long double previous_y;
        if(first==0x100 || first==0x4000) {
            previous_y=picking_load(y[previous]);
            crossing=picking_compare(previous_y,query_y)==0;
        }
        if(!crossing) {
            previous_y=picking_load(y[previous]);
            const unsigned second=picking_compare(previous_y,query_y);
            if(second==0x100 || second==0x4000)crossing=picking_compare(query_y,current_y)==0x100;
        }
        if(crossing) {
            const long double current_x=picking_load(x[i]),previous_x=picking_load(x[previous]);
            const long double delta_x=previous_x-current_x;
            const long double delta_y=query_y-current_y;
            const long double divisor=previous_y-current_y;
            const long double product=delta_y*delta_x;
            const long double intersection=product/divisor+current_x;
            if(picking_compare(intersection,query_x)==0)inside^=1;
        }
        previous=i;
    }
    return inside;
}
bool geometry_segment_contains_point(const GeometryShape& shape,const ConductorSegment& segment,double px,double py) {
    const auto query=picking_query_bits(px,py);return picking_segment_bits(shape,segment,query);
}
bool geometry_shape_contains_point(const GeometryShape& shape,double px,double py) {
    const auto query=picking_query_bits(px,py);return picking_shape_bits(shape,query);
}
bool select_geometry_shape_at_point(GeometryShape* head,GeometryShape*& selected,double px,double py) {
    const auto query=picking_query_bits(px,py);
    for(auto* shape=head;shape;shape=shape->next)
        if(picking_shape_bits(*shape,query) && !shape->suffix[0]) { selected=shape;return true; }
    selected=nullptr;return false;
}
}
