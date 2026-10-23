#include "asitic/composite_geometry.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
const char* name(const GeometryShape& shape) { return reinterpret_cast<const char*>(shape.filaments.prefix); }
char* name(GeometryShape& shape) { return reinterpret_cast<char*>(shape.filaments.prefix); }
int& integer(GeometryShape& shape,unsigned offset) { return *reinterpret_cast<int*>(shape.filaments.prefix+offset); }
int integer(const GeometryShape& shape,unsigned offset) { return *reinterpret_cast<const int*>(shape.filaments.prefix+offset); }
const double& number(const GeometryShape& shape,unsigned offset) { return *reinterpret_cast<const double*>(shape.filaments.prefix+offset); }
double& number(GeometryShape& shape,unsigned offset) { return *reinterpret_cast<double*>(shape.filaments.prefix+offset); }
ConductorSegment* segment(unsigned address) { return reinterpret_cast<ConductorSegment*>(address); }
bool equal(const char* a,const char* b) { while(*a==*b) { if(!*a)return true;++a;++b; }return false; }
void copy_text(char* out,const char* input) { unsigned i=0;do { out[i]=input[i]; }while(input[i++]); }
void floating_copy(double& out,const double& in) { asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st","memory"); }
void raw_copy(ConductorSegment& out,const ConductorSegment& in) {
    for(unsigned i=0;i<sizeof(out);++i)reinterpret_cast<volatile unsigned char*>(&out)[i]=reinterpret_cast<const unsigned char*>(&in)[i];
}
double floor_binary64(double value) {
    unsigned short saved,temporary;long double result;asm volatile("fnstcw %0" : "=m"(saved));temporary=(saved&0xf3ffu)|0x0400u;
    asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(result) : "0"(static_cast<long double>(value)),"m"(temporary),"m"(saved));return binary64(result);
}
bool via_joins(int encoded,int other,const GeometryTechnology& technology,const GeometryCommandRuntime& runtime) {
    const unsigned index=static_cast<unsigned>(encoded)-static_cast<unsigned>(technology.metal_count);
    const auto* p=reinterpret_cast<const unsigned char*>(technology.vias)+index*0xf0u;
    const int upper=*reinterpret_cast<const volatile int*>(p+0xd0);if(upper==other)return true;
    const int lower=*reinterpret_cast<const volatile int*>(p+0xcc);if(lower==other)return true;
    runtime.construction.format(runtime.construction.message,"Error:  Via layer %d can only join to metals %d or %d.",static_cast<int>(index),upper,lower);
    runtime.construction.warnings.emit(runtime.construction.message);return false;
}
}
GeometryShape* find_geometry_shape(GeometryShape* head,GeometryShape* selected,const char* value) {
    if(value[0]=='.' && value[1]==0)return selected;
    for(auto* p=head;p;p=p->next)if(equal(value,name(*p)))return p;
    return nullptr;
}
GeometryShape* copy_geometry_shape(GeometryShape*& head,const GeometryShape& source,const char* requested,double x,double y,int metal,const GeometryTechnology& technology,const GeometryCommandRuntime& runtime) {
    prepend_geometry_shape(head,runtime.construction.geometry);copy_geometry_shape_fields(*head,source);
    if(requested) {
        if(find_geometry_shape(head,runtime.selected,requested)) { runtime.construction.warnings.emit("Error:  Cannot copy using a spiral name which is already in use.");return nullptr; }
        copy_text(name(*head),requested);
    } else {
        unsigned length=0;while(name(source)[length])++length;
        const unsigned capacity=length+2u>81u?length+2u:81u;auto* temporary=static_cast<char*>(runtime.names.arrays.get(capacity));
        for(unsigned i=0;i<length;++i)temporary[i]=name(source)[i];
        temporary[length++]='_';temporary[length]=0;
        while(find_geometry_shape(head,runtime.selected,temporary)) {
            if(length>79)break;
            temporary[length++]=static_cast<char>(runtime.random(runtime.user));temporary[length]=0;
        }
        copy_text(name(*head),temporary);runtime.names.put(temporary);
    }
    head->filaments.first=nullptr;
    if(x==1e15 && y==-1e15) { floating_copy(number(*head,0x54),number(source,0x54));floating_copy(number(*head,0x5c),number(source,0x5c)); }
    else { floating_copy(number(*head,0x54),x);floating_copy(number(*head,0x5c),y); }
    integer(*head,0x68)=metal==-1?integer(source,0x68):metal;
    for(auto* p=source.filaments.first;p;p=segment(p->next_address)) {
        ConductorSegment temporary;raw_copy(temporary,*p);
        if(temporary.metal!=metal && metal!=-1)refresh_geometry_metal(temporary,metal,technology);
        prepend_geometry_segment(head->filaments.first,temporary,runtime.construction.geometry);
    }
    reverse_geometry_segments(*head);return head;
}
void join_geometry_shapes(GeometryShape*& head,GeometryShape& target,GeometryShape& source,const GeometryTechnology& technology,const GeometryCommandRuntime& runtime) {
    if(equal(name(target),name(source))) { runtime.construction.warnings.emit("Error:  Cannot join a spiral to itself.");return; }
    if(integer(target,0x50)==14 && !via_joins(integer(target,0x68),integer(source,0x68),technology,runtime))return;
    if(integer(source,0x50)==14 && !via_joins(integer(source,0x68),integer(target,0x68),technology,runtime))return;
    reverse_geometry_segments(target);
    for(auto* p=const_cast<ConductorSegment*>(source.filaments.first);p;p=segment(p->next_address)) {
        translate_geometry_segment(*p,target,source);
        for(auto* child=segment(p->group_next_address);child;child=segment(child->group_next_address))translate_geometry_segment(*child,target,source);
        prepend_geometry_segment(target.filaments.first,*p,runtime.construction.geometry);
    }
    if(integer(target,0x50)==2 && static_cast<unsigned>(integer(source,0x50))<2u) {
        floating_copy(number(target,0x54),number(source,0x54));floating_copy(number(target,0x5c),number(source,0x5c));
        for(unsigned offset=0x64;offset<=0x6c;offset+=4)integer(target,offset)=integer(source,offset);
        for(unsigned offset=0x70;offset<=0x90;offset+=8)floating_copy(number(target,offset),number(source,offset));
        number(target,0x98)=binary64(0.25L+number(source,0x98));floating_copy(number(target,0xa0),number(source,0xa0));target.attributes=source.attributes;
    }
    integer(target,0x50)=21;remove_geometry_shape(head,&source,runtime.construction.geometry.memory);reverse_geometry_segments(target);
}
void build_multimetal_square(GeometryShape*& head,GeometryShape& args,const GeometryTechnology& technology,const GeometryCommandRuntime& runtime,const SquareConstructionHistory& history) {
    const long double fraction=static_cast<long double>(number(args,0x98))-floor_binary64(number(args,0x98));
    const bool alternate=fraction==0.L || fraction==0.5L;bool reflect_y=alternate,first=true;
    const int delta=static_cast<int>(static_cast<unsigned>(integer(args,0x68))-static_cast<unsigned>(integer(args,0x6c)));
    if(delta<1) {
        runtime.construction.warnings.emit("Error:  Must specify more than one metal layer for multi-metal inductor");
        if(runtime.construction.structure_violation)runtime.construction.structure_violation(runtime.construction.user);
        __builtin_trap();
    }
    auto** shapes=static_cast<GeometryShape**>(runtime.construction.geometry.memory.arrays.get(static_cast<unsigned>(delta)*4u));
    integer(args,0x6c)=-1;build_square_spiral(head,args,3,technology,runtime.construction,history);
    integer(args,0x6c)=static_cast<int>(static_cast<unsigned>(integer(args,0x68))-static_cast<unsigned>(delta));
    integer(*head,0x50)=18;integer(*head,0x6c)=integer(args,0x6c);shapes[0]=head;
    for(int i=1;i<=delta;i=static_cast<int>(static_cast<unsigned>(i)+1u)) {
        char destination[80];runtime.construction.format(destination,"%s-%d",name(*shapes[0]),i);
        copy_geometry_shape(head,*shapes[i-1],destination,number(*shapes[0],0x54),number(*shapes[0],0x5c),
            static_cast<int>(static_cast<unsigned>(integer(*shapes[0],0x68))-static_cast<unsigned>(i)),technology,runtime);
        if(reflect_y)reflect_geometry_y(*head);else reflect_geometry_x(*head);
        if(alternate) { if(first) { first=false;reflect_y=false; }else { first=true;reflect_y=true; } }
        reverse_geometry_segments(*head);reverse_geometry_directions(*head);shapes[i]=head;
    }
    for(int i=1;i<=delta;i=static_cast<int>(static_cast<unsigned>(i)+1u))join_geometry_shapes(head,*shapes[0],*shapes[i],technology,runtime);
    runtime.construction.geometry.memory.put(shapes);
}
}
