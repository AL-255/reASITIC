#include "asitic/contacts.hpp"
#include "asitic/lapack.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
void text_copy(char* out,const char* in) { do { *out=*in; ++out; }while(*in++); }
bool text_equal(const char* a,const char* b) { while(*a && *a==*b) { ++a;++b; }return *a==*b; }
double shape_coordinate(const ShapeFilamentList& shape,unsigned offset) {
    double value;__builtin_memcpy(&value,shape.prefix+offset,8);return value;
}
double front_width(const ConductorSegment& s) { double value;__builtin_memcpy(&value,s.prefix_rest,8);return value; }
int front_metal(const ConductorSegment& s) { int value;__builtin_memcpy(&value,s.prefix_rest+16,4);return value; }
}
void prepend_potential_group(PotentialContactGroup*& groups,SolverMemory memory) {
    auto* g=static_cast<PotentialContactGroup*>(memory.arrays.get(sizeof(PotentialContactGroup)));
    // This store precedes the original null check.
    g->first=nullptr;
    if(!g)lapack::application_fatal_error("501:  Cannot allocate memory");
    g->next=groups;groups=g;
}
void prepend_potential_contact(PotentialContact*& head,const PotentialContact& value,SolverMemory memory) {
    auto* c=static_cast<PotentialContact*>(memory.arrays.get(sizeof(PotentialContact)));
    if(!c)lapack::application_fatal_error("505:  Cannot allocate memory");
    const auto* in=reinterpret_cast<const unsigned*>(&value);auto* out=reinterpret_cast<unsigned*>(c);
    for(unsigned i=0;i<15;++i)out[i]=in[i];
    c->next_address=reinterpret_cast<unsigned>(head);head=c;
}
void release_potential_contacts(PotentialContactGroup*& groups,SolverMemory memory) {
    for(auto* g=groups;g;) {
        auto* next=g->next;
        if(g==groups)groups=next;
        else { auto* previous=groups;while(previous->next!=g)previous=previous->next;previous->next=next; }
        for(auto* c=g->first;c;) {
            auto* after=reinterpret_cast<PotentialContact*>(c->next_address);memory.put(c);c=after;
        }
        memory.put(g);g=next;
    }
}
void reverse_potential_groups(PotentialContactGroup*& groups) {
    auto* current=groups->next;groups->next=nullptr;
    while(current) { auto* next=current->next;current->next=groups;groups=current;current=next; }
}
bool prepare_potential_contact_group(const ConductorSegment& s,const ShapeFilamentList& shape,
    PotentialContactGroup*& current,bool reuse,PotentialContactGroup*& groups,
    const PotentialContactOptions& o,SolverMemory memory,WarningSink warnings) {
    double length_step=o.length_step,width_step=o.width_step;
    if(s.width_count==-1 && s.thickness_count==-1 && s.metal<=o.metal_count) {
        const long double x=(static_cast<long double>(o.width)*10.L)/o.nx,y=(static_cast<long double>(o.height)*10.L)/o.ny;
        length_step=binary64(y<x?x:y);width_step=binary64(y<x?x:y);
    }
    const double ox=shape_coordinate(shape,0x54),oy=shape_coordinate(shape,0x5c);
    const double x0=binary64(static_cast<long double>(s.endpoints[0])+ox),x1=binary64(static_cast<long double>(ox)+s.endpoints[3]);
    const long double y0=static_cast<long double>(s.endpoints[1])+oy;
    const double y0_saved=binary64(y0),y1=binary64(static_cast<long double>(oy)+s.endpoints[4]);
    if(x0<0 || o.width<x0 || x1<0 || o.width<x1 || y0<0 || o.height<y0 || y1<0 || o.height<y1) {
        warnings.emit("Error:  Coordinate out of range.");return false;
    }
    if(!reuse) { prepend_potential_group(groups,memory);current=groups; }
    text_copy(current->name,reinterpret_cast<const char*>(shape.prefix));
    const long double dx=static_cast<long double>(s.endpoints[3])-s.endpoints[0],dy=static_cast<long double>(s.endpoints[4])-s.endpoints[1],dz=static_cast<long double>(s.endpoints[5])-s.endpoints[2];
    const long double length=x87::sqrt((dx*dx+dy*dy)+dz*dz);
    int along=x87::truncate_int(length/length_step+0.5L);
    int across=x87::truncate_int(static_cast<long double>(s.width)/width_step+0.5L);
    if(across<1)across=1;
    if(along<1)along=1;
    const long double pitch=(o.height<o.width?static_cast<long double>(o.width):o.height)/(o.ny>o.nx?o.nx:o.ny);
    while(static_cast<long double>(s.width)/across<pitch && across>0)--across;
    while(length/along<pitch && along>0)--along;
    if(across==0)across=1;
    if(along==0)along=1;
    current->width_cells=across;current->length_cells=along;
    current->start_x=x0;current->end_x=x1;current->start_y=y0_saved;current->end_y=y1;current->metal=s.metal;
    return true;
}
void build_potential_contacts(PotentialContactGroup& g,const ConductorSegment& segment,
    const PotentialContactOptions& o,SolverMemory memory,PotentialContactHistory history) {
    const long double dx=static_cast<long double>(g.end_x)-g.start_x,dy=static_cast<long double>(g.end_y)-g.start_y;
    const long double length=x87::sqrt(dx*dx+dy*dy),half=static_cast<long double>(front_width(segment))*0.5L;
    const long double offset_x=(half*dy)/length;
    const double offset_y=binary64((half*dx)/length);
    const double step_x=binary64(static_cast<long double>(binary64(offset_x+offset_x))/g.width_cells);
    const double step_y=binary64((static_cast<long double>(offset_y)+offset_y)/g.width_cells);
    const double cell_width=binary64(static_cast<long double>(front_width(segment))/g.width_cells);
    const double cell_length=binary64(length/g.length_cells);
    const double along_x=binary64(dx/g.length_cells),along_y=binary64(dy/g.length_cells);
    const double angle=binary64(x87::atan2(dy,dx));
    for(int i=0;i<g.width_cells;++i)for(int j=0;j<g.length_cells;++j) {
        const long double base_x=static_cast<long double>(binary64(static_cast<long double>(g.start_x)-binary64(offset_x)))+static_cast<long double>(i)*step_x+binary64(static_cast<long double>(step_x)*0.5L);
        const long double base_y=((static_cast<long double>(g.start_y)+offset_y)-binary64(static_cast<long double>(i)*step_y))-binary64(static_cast<long double>(step_y)*0.5L);
        const long double ax=base_x+static_cast<long double>(j)*along_x,ay=base_y+static_cast<long double>(j)*along_y;
        const long double bx=base_x+static_cast<long double>(j+1)*along_x;
        const double by=binary64(base_y+static_cast<long double>(j+1)*along_y);
        const long double center_x=x87::abs(ax-bx)*0.5L+(ax<bx?ax:bx);
        const long double center_y=(ay<by?ay:by)+x87::abs(ay-by)*0.5L;
        const bool horizontal=x87::abs(angle)<0x1.921fb54442d18p-1 || 0x1.2d97c7f3321d2p+1<x87::abs(angle);
        const long double half_x=static_cast<long double>(horizontal?cell_length:cell_width)*0.5L;
        const long double half_y=static_cast<long double>(horizontal?cell_width:cell_length)*0.5L;
        const long double x0=horizontal?-half_x+center_x:center_x-half_x;
        const long double x1=half_x+center_x;
        const long double y0=horizontal?center_y-half_y:-half_y+center_y;
        const long double y1=center_y+half_y;
        PotentialContact value;
        const double bounds[]={binary64(x0),binary64(x1),binary64(y0),binary64(y1)};
        __builtin_memcpy(value.prefix,bounds,32);
        value.x0=x87::truncate_int((x0*o.nx)/o.width);value.x1=x87::truncate_int((x1*o.nx)/o.width);
        value.y0=x87::truncate_int((y0*o.ny)/o.height);value.y1=x87::truncate_int((y1*o.ny)/o.height);
        value.metal=front_metal(segment);value.index=history.index;value.next_address=0;
        prepend_potential_contact(g.first,value,memory);
    }
}
bool prepare_potential_contacts(const ShapeFilamentList& first,const ShapeFilamentList& second,
    PotentialContactGroup*& groups,const PotentialContactOptions& o,SolverMemory memory,PotentialContactHistory history,WarningSink warnings) {
    const auto walk=[&](const ShapeFilamentList& shape) {
        for(auto* s=shape.first;s;s=reinterpret_cast<const ConductorSegment*>(s->next_address)) {
            if(s->metal>=o.metal_count)continue;
            PotentialContactGroup* current=nullptr;
            if(!prepare_potential_contact_group(*s,shape,current,false,groups,o,memory,warnings))return false;
            build_potential_contacts(*current,*s,o,memory,history);
            for(auto* extra=reinterpret_cast<const ConductorSegment*>(s->group_next_address);extra;
                extra=reinterpret_cast<const ConductorSegment*>(extra->group_next_address)) {
                if(!prepare_potential_contact_group(*extra,shape,current,true,groups,o,memory,warnings))return false;
                build_potential_contacts(*current,*extra,o,memory,history);
            }
        }
        return true;
    };
    if(!walk(first))return false;
    if(!text_equal(reinterpret_cast<const char*>(first.prefix),reinterpret_cast<const char*>(second.prefix)) && !walk(second))return false;
    reverse_potential_groups(groups);return true;
}
}
