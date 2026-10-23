#include "asitic/geometry_edit.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
ConductorSegment* segment(unsigned address) { return reinterpret_cast<ConductorSegment*>(address); }
ConductorSegment* mutable_segment(const ConductorSegment* value) { return const_cast<ConductorSegment*>(value); }
char* name(GeometryShape& s) { return reinterpret_cast<char*>(&s); }
double& number(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
int& integer(GeometryShape& s,unsigned offset) { return *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&s)+offset); }
void copy(char* out,const char* in) { unsigned i=0;do { out[i]=in[i]; }while(in[i++]); }
void append(char* out,const char* in) { while(*out)++out;copy(out,in); }
void raw_copy(ConductorSegment& out,const ConductorSegment& in) {
    for(unsigned i=0;i<sizeof(out);++i)reinterpret_cast<volatile unsigned char*>(&out)[i]=reinterpret_cast<const unsigned char*>(&in)[i];
}
ConductorSegment* allocate_segment(const GeometryRuntime& r) {
    auto* p=static_cast<ConductorSegment*>(r.memory.arrays.get(0xf0));
    if(!p) { r.errors.emit("Cannot allocate memory");r.terminate(-1,r.user); }
    return p;
}
void insert_secondary(ConductorSegment* cursor,const ConductorSegment& source,const GeometryRuntime& r) {
    auto* p=allocate_segment(r);raw_copy(*p,source);
    p->next_address=cursor->next_address;p->group_next_address=cursor->group_next_address;
    cursor->group_next_address=reinterpret_cast<unsigned>(p);
}
void create_wire(GeometryShape*& head,const GeometryShape& source,const ConductorSegment& record,int main_index,int child_index,const GeometryEditRuntime& r) {
    prepend_geometry_shape(head,r.geometry);copy_geometry_shape_fields(*head,source);
    if(child_index<0)r.format(r.message,"_%d",main_index);else r.format(r.message,"_%d_%d",main_index,child_index);
    append(name(*head),r.message);integer(*head,0x50)=21;head->filaments.first=nullptr;
    prepend_geometry_segment(head->filaments.first,record,r.geometry);
}
}
void rename_geometry_shape(GeometryShape* head,GeometryShape& shape,const char* requested,const GeometryEditRuntime& r) {
    r.format(r.message,"Error:  %s already in use.",requested);
    if(!find_geometry_shape(head,r.selected,requested))copy(name(shape),requested);else r.warnings.emit(r.message);
}
void unjoin_geometry_shape(GeometryShape*& head,GeometryShape& shape,int index,const char* requested,const GeometryEditRuntime& r) {
    if(find_geometry_shape(head,r.selected,requested)) { r.warnings.emit("Error:  Spiral name already in use.");return; }
    auto* cut=mutable_segment(shape.filaments.first);int count=1;
    for(auto* p=segment(cut->next_address);p;p=segment(p->next_address))++count;
    if(!count) { r.warnings.emit("Error:  Spiral contains no segments.");return; }
    if(index < -count || index > count) { r.warnings.emit("Error:  Index out of range.");return; }
    if(!index) {
        if(count<2) { r.warnings.emit("Error:  Cannot split a single segment in half.  This feature will be added in the future.");return; }
        index=count/2;
    }
    if(index<0)index=index-1+count;
    for(int i=1;i<index;++i)cut=segment(cut->next_address);
    const auto* tail=segment(cut->next_address);cut->next_address=0;
    const int turns=index/integer(shape,0x64);number(shape,0x98)=binary64(static_cast<long double>(number(shape,0x98))-turns);
    prepend_geometry_shape(head,r.geometry);copy_geometry_shape_fields(*head,shape);copy(name(*head),requested);
    integer(*head,0x50)=2;head->filaments.first=tail;
    double x,y;geometry_minimum_xy(*head,&x,&y);
    number(*head,0x54)=binary64(static_cast<long double>(number(shape,0x54))+x);
    number(*head,0x5c)=binary64(static_cast<long double>(number(shape,0x5c))+y);
    translate_geometry_shape(*head,shape);
}
void join_shunt_geometry_shapes(GeometryShape*& head,GeometryShape& target,GeometryShape& source,const GeometryEditRuntime& r) {
    reverse_geometry_segments(target);reverse_geometry_segments(source);
    auto* cursor=mutable_segment(target.filaments.first);
    for(auto* p=mutable_segment(source.filaments.first);p;p=segment(p->next_address)) {
        translate_geometry_segment(*p,target,source);insert_secondary(cursor,*p,r.geometry);
        cursor=segment(cursor->next_address);
    }
    remove_geometry_shape(head,&source,r.geometry.memory);reverse_geometry_segments(target);
}
int convert_geometry_to_wires(GeometryShape*& head,GeometryShape& source,const GeometryEditRuntime& r) {
    int total=0,index=0;
    for(auto* p=source.filaments.first;p;p=segment(p->next_address)) {
        create_wire(head,source,*p,index,-1,r);
        int child_index=0;
        for(auto* child=segment(p->group_next_address);child;child=segment(child->next_address)) {
            create_wire(head,source,*child,index,child_index++,r);++total;
        }
        ++index;++total;
    }
    remove_geometry_shape(head,&source,r.geometry.memory);return total;
}
}
