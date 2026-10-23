#include "asitic/geometry.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
void floating_copy(void* out,const void* in) {
    asm volatile("fldl %1; fstpl %0" : "=m"(*static_cast<double*>(out)) : "m"(*static_cast<const double*>(in)) : "st");
}
ConductorSegment* mutable_segment(const ConductorSegment* p) { return const_cast<ConductorSegment*>(p); }
const ConductorSegment* segment_at(unsigned p) { return reinterpret_cast<const ConductorSegment*>(p); }
[[noreturn]] void allocation_failed(const GeometryRuntime& runtime) {
    runtime.errors.emit("Cannot allocate memory");
    if(runtime.terminate)runtime.terminate(-1,runtime.user);
    __builtin_trap();
}
ConductorSegment* allocate_segment(const GeometryRuntime& runtime) {
    auto* p=static_cast<ConductorSegment*>(runtime.memory.arrays.get(0xf0));
    if(!p)allocation_failed(runtime);
    return p;
}
void raw_copy(ConductorSegment& out,const ConductorSegment& in) {
    auto* d=reinterpret_cast<unsigned*>(&out);const auto* s=reinterpret_cast<const unsigned*>(&in);
    for(unsigned i=0;i<60;++i)d[i]=s[i];
}
long double edge_length(const double* edge) {
    const long double x=static_cast<long double>(edge[3])-edge[0],y=static_cast<long double>(edge[4])-edge[1],z=static_cast<long double>(edge[5])-edge[2];
    return x87::sqrt((x*x+y*y)+z*z);
}
void edge_direction(double* out,const double* edge) {
    for(unsigned i=0;i<3;++i)out[i]=binary64(static_cast<long double>(edge[i+3])-edge[i]);
    const long double x=out[0],y=out[1],z=out[2],length=x87::sqrt((x*x+y*y)+z*z);
    for(unsigned i=0;i<3;++i)out[i]=binary64(static_cast<long double>(out[i])/length);
}
long double ceiling(long double x) {
    unsigned short saved,temporary;long double result;
    asm volatile("fnstcw %0" : "=m"(saved));temporary=(saved&0xf3ffu)|0x0800u;
    asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(result) : "0"(x),"m"(temporary),"m"(saved));
    return result;
}
void split_record(ConductorSegment& out,const ConductorSegment& in,int index,int count) {
    double main[3],inner[3],outer[3];
    edge_direction(main,in.endpoints);edge_direction(inner,in.inner_start);edge_direction(outer,in.outer_start);
    copy_geometry_segment(out,in);
    subdivide_geometry_edge(out.outer_start,in.outer_start,outer,index,count);
    subdivide_geometry_edge(out.inner_start,in.inner_start,inner,index,count);
    subdivide_geometry_edge(out.endpoints,in.endpoints,main,index,count);
}
__attribute__((noinline)) void initialize_shape(GeometryShape* p) {
    *reinterpret_cast<volatile unsigned*>(reinterpret_cast<unsigned char*>(p)+0xa8)=0;
    *reinterpret_cast<volatile unsigned*>(reinterpret_cast<unsigned char*>(p)+0xb4)=0;
    *reinterpret_cast<volatile unsigned char*>(reinterpret_cast<unsigned char*>(p)+0xb8)=0;
}
}
void copy_geometry_block(void* destination,const void* source) {
    auto* d=static_cast<unsigned char*>(destination);const auto* s=static_cast<const unsigned char*>(source);
    for(unsigned i=0;i<0x40;i+=8)floating_copy(d+i,s+i);
    *reinterpret_cast<unsigned*>(d+0x40)=*reinterpret_cast<const unsigned*>(s+0x40);
}
void copy_geometry_segment(ConductorSegment& out,const ConductorSegment& in) {
    auto* d=reinterpret_cast<unsigned char*>(&out);const auto* s=reinterpret_cast<const unsigned char*>(&in);
    copy_geometry_block(d,s);copy_geometry_block(d+0x88,s+0x88);copy_geometry_block(d+0x44,s+0x44);
    floating_copy(&out.width,&in.width);floating_copy(&out.thickness,&in.thickness);
    out.metal=in.metal;out.width_count=in.width_count;out.thickness_count=in.thickness_count;
    out.group_next_address=out.next_address=0;
}
void subdivide_geometry_edge(double* out,const double* source,const double* direction,int index,int count) {
    const long double length=edge_length(out);
    const long double first=(length*index)/count,last=(length*static_cast<int>(static_cast<unsigned>(index)+1))/count;
    out[0]=binary64(first*direction[0]+source[0]);out[1]=binary64(first*direction[1]+source[1]);out[2]=binary64(source[2]+first*direction[2]);
    out[3]=binary64(last*direction[0]+source[0]);out[4]=binary64(last*direction[1]+source[1]);out[5]=binary64(source[2]+last*direction[2]);
}
void reverse_geometry_segments(GeometryShape& shape) {
    auto* head=mutable_segment(shape.filaments.first);auto* current=mutable_segment(segment_at(head->next_address));head->next_address=0;
    while(current) { auto* next=mutable_segment(segment_at(current->next_address));current->next_address=reinterpret_cast<unsigned>(head);head=current;current=next; }
    shape.filaments.first=head;
}
void translate_geometry_segment(ConductorSegment& segment,const GeometryShape& source,const GeometryShape& target) {
    auto* p=reinterpret_cast<unsigned char*>(&segment);
    const unsigned offsets[]={0,8,0x18,0x20,0x88,0x90,0xa0,0xa8,0x44,0x4c,0x5c,0x64};
    for(unsigned i=0;i<12;++i) {
        const unsigned axis=i%2?0x5c:0x54;
        const auto* a=reinterpret_cast<const double*>(source.filaments.prefix+axis);const auto* b=reinterpret_cast<const double*>(target.filaments.prefix+axis);
        auto* v=reinterpret_cast<double*>(p+offsets[i]);*v=binary64((static_cast<long double>(*b)-*a)+*v);
    }
}
void translate_geometry_shape(GeometryShape& source,const GeometryShape& target) {
    for(auto* s=mutable_segment(source.filaments.first);s;s=mutable_segment(segment_at(s->next_address))) {
        translate_geometry_segment(*s,source,target);
        for(auto* child=mutable_segment(segment_at(s->group_next_address));child;child=mutable_segment(segment_at(child->next_address)))translate_geometry_segment(*child,source,target);
    }
}
void prepend_geometry_segment(const ConductorSegment*& head,const ConductorSegment& source,const GeometryRuntime& runtime) {
    auto* parent=allocate_segment(runtime);raw_copy(*parent,source);
    parent->next_address=reinterpret_cast<unsigned>(head);head=parent;parent->group_next_address=0;
    for(auto* child=segment_at(source.group_next_address);child;child=segment_at(child->group_next_address)) {
        auto* node=allocate_segment(runtime);raw_copy(*node,*child);
        node->group_next_address=parent->group_next_address;parent->group_next_address=reinterpret_cast<unsigned>(node);node->next_address=parent->next_address;
    }
}
void copy_geometry_shape_fields(GeometryShape& out,const GeometryShape& in) {
    auto* d=reinterpret_cast<unsigned char*>(&out);const auto* s=reinterpret_cast<const unsigned char*>(&in);
    unsigned i=0;do { d[i]=s[i]; }while(s[i++]);
    *reinterpret_cast<unsigned*>(d+0x50)=*reinterpret_cast<const unsigned*>(s+0x50);
    floating_copy(d+0x54,s+0x54);floating_copy(d+0x5c,s+0x5c);
    for(unsigned offset=0x64;offset<=0x6c;offset+=4)*reinterpret_cast<unsigned*>(d+offset)=*reinterpret_cast<const unsigned*>(s+offset);
    for(unsigned offset=0x70;offset<=0xa0;offset+=8)floating_copy(d+offset,s+offset);
    out.attributes=in.attributes;
}
void reset_geometry_mesh_counts(GeometryShape& shape) {
    for(auto* s=mutable_segment(shape.filaments.first);s;s=mutable_segment(segment_at(s->next_address)))s->width_count=s->thickness_count=-1;
}
int count_geometry_metals(const GeometryShape& shape,int metal_count) {
    int count=0;for(auto* s=shape.filaments.first;s;s=segment_at(s->next_address))if(s->metal<metal_count)++count;return count;
}
void release_geometry_segments(const ConductorSegment* head,SolverMemory memory) {
    do {
        auto* next=segment_at(head->next_address);
        for(auto* child=segment_at(head->group_next_address);child;) { auto* after=segment_at(child->group_next_address);memory.put(child);child=after; }
        memory.put(head);head=next;
    }while(head);
}
void release_geometry_shapes(GeometryShape* head,SolverMemory memory) {
    do { auto* next=head->next;release_geometry_segments(head->filaments.first,memory);memory.put(head);head=next; }while(head);
}
void prepend_geometry_shape(GeometryShape*& head,const GeometryRuntime& runtime) {
    auto* p=static_cast<GeometryShape*>(runtime.memory.arrays.get(0xbc));initialize_shape(p);
    if(!p)allocation_failed(runtime);
    p->next=head;head=p;
}
void discretize_geometry_segment(GeometryShape& shape,const ConductorSegment& source,const GeometryDiscretization& options,const GeometryRuntime& runtime) {
    const double limit=options.length_step>options.half_width?options.half_width:options.length_step;
    double maximum=binary64(edge_length(source.endpoints));
    for(auto* s=segment_at(source.group_next_address);s;s=segment_at(s->group_next_address)) {
        const long double length=edge_length(s->endpoints);if(length>maximum)maximum=binary64(length);
    }
    if(!(limit<maximum)) { prepend_geometry_segment(shape.filaments.first,source,runtime);return; }
    // These preliminary normalizations run even when the eventual count is zero.
    double initial[3];edge_direction(initial,source.endpoints);edge_direction(initial,source.inner_start);edge_direction(initial,source.outer_start);
    const int count=x87::truncate_int(binary64(ceiling(static_cast<long double>(maximum)/options.half_width)));
    for(int i=0;i<count;++i) {
        ConductorSegment value;split_record(value,source,i,count);prepend_geometry_segment(shape.filaments.first,value,runtime);
        auto* parent=mutable_segment(shape.filaments.first);
        for(auto* child=segment_at(source.group_next_address);child;child=segment_at(child->group_next_address)) {
            split_record(value,*child,i,count);auto* p=allocate_segment(runtime);raw_copy(*p,value);
            p->next_address=parent->next_address;p->group_next_address=parent->group_next_address;parent->group_next_address=reinterpret_cast<unsigned>(p);
        }
    }
}
GeometryShape* clone_geometry_shape(GeometryShape*& head,const GeometryShape& source,const GeometryDiscretization& options,const GeometryRuntime& runtime) {
    prepend_geometry_shape(head,runtime);copy_geometry_shape_fields(*head,source);
    for(auto* s=source.filaments.first;s;s=segment_at(s->next_address))discretize_geometry_segment(*head,*s,options,runtime);
    reverse_geometry_segments(*head);return head;
}
void remove_geometry_shape(GeometryShape*& head,GeometryShape* shape,SolverMemory memory) {
    if(shape==head)head=shape->next;
    else { auto* previous=head;while(previous->next!=shape)previous=previous->next;previous->next=shape->next; }
    release_geometry_segments(shape->filaments.first,memory);memory.put(shape);
}
}
