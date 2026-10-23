#include "asitic/geometry_reporting.hpp"
#include "asitic/numerics.hpp"
namespace asitic {
namespace {
// On the native i386 variadic ABI this POD occupies exactly a binary64 slot.
// Passing words avoids quietly loading signaling NaNs used only for printing.
struct Raw { unsigned low,high; };
Raw raw(const void* p,unsigned offset=0) { const auto* words=reinterpret_cast<const volatile unsigned*>(static_cast<const unsigned char*>(p)+offset);Raw r;r.high=words[1];r.low=words[0];return r; }
Raw rounded(long double n) { volatile double d=static_cast<double>(n);return raw(const_cast<const double*>(&d)); }
int integer(const GeometryShape& s,unsigned offset) { int n;__builtin_memcpy(&n,s.filaments.prefix+offset,4);return n; }
long double wide(const GeometryShape& s,unsigned offset) { long double n;asm volatile("fldl %1; fstpt %0":"=m"(n):"m"(*reinterpret_cast<const double*>(s.filaments.prefix+offset)):"st");return n; }
const char* name(const GeometryShape& s) { return reinterpret_cast<const char*>(s.filaments.prefix); }
const char* metal_name(const GeometryTechnology& t,int i) { return reinterpret_cast<const char*>(t.metals+i); }
const char* layer_name(const GeometryTechnology& t,int i) { return i<t.metal_count?metal_name(t,i):reinterpret_cast<const char*>(t.vias+(i-t.metal_count)); }
void copy(char* d,const char* s) { do { *d=*s;++d; } while(*s++); }
void append(char* d,const char* s) { while(*d)++d;copy(d,s); }
template<class... A>void emit(const CommandRuntime& r,const char* format,A... args) { r.format(r.message,format,args...);r.warnings.emit(r.message); }
void segment_length_width(const ConductorSegment& s,long double& length,long double& width) {
    asm volatile("fldl 24(%2); fsubl 0(%2); fldl 32(%2); fsubl 8(%2); fxch %%st(1); fmul %%st(0),%%st; fxch %%st(1); fmul %%st(0),%%st; faddp; fldl 40(%2); fsubl 16(%2); fmul %%st(0),%%st; faddp; fldl 204(%2); fxch %%st(1); fsqrt; fstpt %0; fstpt %1"
        :"=m"(length),"=m"(width):"r"(&s):"st","st(1)","st(2)");
}
long double add(long double a,long double b) { long double r;asm volatile("fadd %%st(1),%%st":"=t"(r):"0"(a),"u"(b));return r; }
long double multiply(long double a,long double b) { long double r;asm volatile("fmul %%st(1),%%st":"=t"(r):"0"(a),"u"(b));return r; }
long double subtract(long double a,long double b) { long double r;asm volatile("fsub %%st(1),%%st":"=t"(r):"0"(a),"u"(b));return r; }
double binary64(long double n) { volatile double d=static_cast<double>(n);return d; }
}
long double geometry_metal_area(const GeometryShape& shape) {
    long double area=0;
    for(const auto* segment=shape.filaments.first;segment;segment=reinterpret_cast<const ConductorSegment*>(segment->next_address)) {
        long double length,width;segment_length_width(*segment,length,width);
        if(length>0x1.b7cdfd9d7bdbbp-34L)area=add(multiply(length,width),area);
    }return area;
}
void print_geometry_segments(const GeometryShape& shape,const GeometryTechnology& t,const CommandRuntime& r) {
    unsigned index=0;char layer[80];
    for(const auto* segment=shape.filaments.first;segment;segment=reinterpret_cast<const ConductorSegment*>(segment->next_address)) {
        copy(layer,layer_name(t,segment->metal));
        emit(r,"Segment %3d:  (%7.1lf,%7.1lf)-(%7.1lf,%7.1lf) on <%s>",index++,raw(segment,0),raw(segment,8),raw(segment,24),raw(segment,32),layer);
        for(const auto* child=reinterpret_cast<const ConductorSegment*>(segment->group_next_address);child;child=reinterpret_cast<const ConductorSegment*>(child->group_next_address)) {
            copy(layer,layer_name(t,child->metal));
            emit(r,"\tShunt:  (%7.1lf,%7.1lf)-(%7.1lf,%7.1lf) on <%s>",raw(child,0),raw(child,8),raw(child,24),raw(child,32),layer);
        }
    }
}
void print_geometry_report(const GeometryShape& s,const GeometryTechnology& t,const CommandRuntime& r) {
    double total_length=0,total_area=0;unsigned count=0;
    for(const auto* segment=s.filaments.first;segment;segment=reinterpret_cast<const ConductorSegment*>(segment->next_address)) {
        long double length,width;segment_length_width(*segment,length,width);
        total_length=binary64(add(total_length,length));total_area=binary64(add(multiply(length,width),total_area));++count;
    }
    const int kind=integer(s,0x50);const auto* partner=reinterpret_cast<const GeometryShape*>(s.auxiliary);
    switch(kind) {
    case 0:case 16:case 18:
        emit(r,kind==0?"Square spiral <%s> has the following geometry:":kind==16?"Symmetric square spiral <%s> has the following geometry:":"Multi-Metal Square spiral <%s> has the following geometry:",name(s));
        emit(r,"L1 =%7.2lf, L2 =%7.2lf, W =%7.2lf, S =%7.2lf, N =%7.2lf",raw(&s,0x70),raw(&s,0x78),raw(&s,0x88),raw(&s,0x90),raw(&s,0x98));
        if(kind==18)emit(r,"This spiral occuplies %s-%s.",metal_name(t,integer(s,0x6c)),metal_name(t,integer(s,0x68)));
        break;
    case 1:
        emit(r,"Spiral <%s> is a %d-sided spiral with the following geometry:",name(s),integer(s,0x64));
        emit(r,"R = %7.2lf, W =%7.2lf, S =%7.2lf, N =%7.2lf",raw(&s,0x80),raw(&s,0x88),raw(&s,0x90),raw(&s,0x98));break;
    case 2:
        emit(r,"Wire <%s> has the following geometry:",name(s));
        emit(r,"L =%7.2lf, W =%7.2lf, Metal = %s",raw(&s,0x70),raw(&s,0x88),metal_name(t,integer(s,0x68)));break;
    case 3:case 5: {
        if(partner)emit(r,"Transformer <%s:%s> has the following geometry:",name(s),name(*partner));
        else emit(r,"Transformer winding <%s> has the following geometry:",name(s));
        const long double width=wide(s,0x88);const Raw spacing=rounded(multiply(subtract(wide(s,0x90),width),0.5L));
        if(partner)emit(r,"L1 =%7.2lf, L2 =%7.2lf, Wp =%7.2lf, Ws = %7.2lf, S =%7.2lf, Np =%7.2lf, Ns = %7.2lf",raw(&s,0x70),raw(&s,0x78),rounded(width),raw(partner,0x88),spacing,raw(&s,0x98),raw(partner,0x98));
        else emit(r,"L1 =%7.2lf, L2 =%7.2lf, W =%7.2lfS =%7.2lf, N =%7.2lf",raw(&s,0x70),raw(&s,0x78),rounded(width),spacing,raw(&s,0x98));
        break;
    }
    case 4:
        emit(r,"Capacitor: <%s>\nWidth =%7.2f\nLength =%7.2f\nTop Plate Metal =%d\nBottom Plate Metal =%d\nOrigin = (%5.2f,%5.2f)\nOrientation=%5.2f",name(s),raw(&s,0x88),raw(&s,0x70),integer(s,0x68),integer(s,0x6c),raw(&s,0x54),raw(&s,0x5c),raw(&s,0xa0));break;
    case 14:
        emit(r,"Via <%s> is located on layer %s:",name(s),reinterpret_cast<const char*>(t.vias+(integer(s,0x68)-t.metal_count)));
        emit(r,"There are a total of %d vias in parallel.",static_cast<unsigned>(integer(s,0x64))*static_cast<unsigned>(integer(s,0x6c)));break;
    case 17:
        emit(r,"Symmetric polygon spiral <%s> with <%d> sides has the following geometry:",name(s),integer(s,0x64));
        emit(r,"R =%7.2lf, L2 =%7.2lf, W =%7.2lf, S =%7.2lf, N =%7.2lf",raw(&s,0x80),raw(&s,0x78),raw(&s,0x88),raw(&s,0x90),raw(&s,0x98));break;
    default:emit(r,kind==21?"<%s> is a user defined structure.":"Warning:  Spiral <%s> is not recognized or supported.",name(s));break;
    }
    emit(r,"Total length = %7.2lf (um), Total Area = %7.2lf (um^2)",raw(&total_length),raw(&total_area));
    emit(r,"Located at (%5.2lf,%5.2lf) with %d segments.",raw(&s,0x54),raw(&s,0x5c),count);
}
void print_geometry_intersections(const GeometryShape& shape,const CommandRuntime& r) {
    struct Scratch { double values[8];unsigned metal; };
    const double scale=0x1.a36e2eb1c432dp-14;
    unsigned first_index=0;
    for(const auto* first=shape.filaments.first;first;first=reinterpret_cast<const ConductorSegment*>(first->next_address),++first_index) {
        unsigned second_index=first_index+1;
        for(const auto* second=reinterpret_cast<const ConductorSegment*>(first->next_address);second;second=reinterpret_cast<const ConductorSegment*>(second->next_address),++second_index) {
            Scratch a,b;
            const auto* source_a=reinterpret_cast<const volatile unsigned*>(first);const auto* source_b=reinterpret_cast<const volatile unsigned*>(second);
            auto* destination_a=reinterpret_cast<volatile unsigned*>(&a);auto* destination_b=reinterpret_cast<volatile unsigned*>(&b);
            for(unsigned i=0;i<17;++i)destination_a[i]=source_a[i];
            for(unsigned i=0;i<17;++i)destination_b[i]=source_b[i];
            for(double& value:a.values)asm volatile("fldl %0; fmull %1; fstpl %0":"+m"(value):"m"(scale):"st");
            for(double& value:b.values)asm volatile("fldl %0; fmull %1; fstpl %0":"+m"(value):"m"(scale):"st");
            if(canonicalize_segment_pair(a.values,b.values)==3) {
                emit(r,"\nFound intersection between segment %d and %d.",first_index,second_index);
                emit(r,"A: (%5.2lf, %5.2lf, %5.2lf) <-> (%5.2lf, %5.2lf, %5.2lf)",raw(first,0),raw(first,8),raw(first,16),raw(first,24),raw(first,32),raw(first,40));
                emit(r,"B: (%5.2lf, %5.2lf, %5.2lf) <-> (%5.2lf, %5.2lf, %5.2lf)",raw(second,0),raw(second,8),raw(second,16),raw(second,24),raw(second,32),raw(second,40));
            }
        }
    }
}
bool execute_geometry_reporting_command(int id,const CommandArguments& arguments,GeometryShape* head,const GeometryTechnology& t,const CommandRuntime& r) {
    const auto* shape=reinterpret_cast<const GeometryShape*>(arguments.arguments[0].value);
    switch(id) {
    case 224:
        if(arguments.command.value)print_geometry_report(*shape,t,r);
        else {
            r.warnings.emit("List of Spirals:\n");
            for(auto* current=head;current;current=current->next) {
                r.format(r.message,"%s",name(*current));if(current->suffix[0])append(r.message," (hidden)");r.warnings.emit(r.message);
            }
        }break;
    case 210:print_geometry_segments(*shape,t,r);break;
    case 410:print_geometry_report(*shape,t,r);break;
    case 413:emit(r,"Total Metal Area of %s = %lf (microns^2).",name(*shape),rounded(geometry_metal_area(*shape)));break;
    case 419:print_geometry_intersections(*shape,r);break;
    default:return false;
    }return true;
}
}
