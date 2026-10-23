#include "asitic/geometry_dispatch.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
GeometryShape* argument(const CommandArguments& a,unsigned i) { return reinterpret_cast<GeometryShape*>(a.arguments[i].value); }
GeometryShape* partner(GeometryShape& s) { return reinterpret_cast<GeometryShape*>(s.auxiliary); }
double& number(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&s)+offset); }
int attributes(const GeometryShape& s) { int n;__builtin_memcpy(&n,&s.attributes,4);return n; }
int integer(const CommandArgument& a) { int n;__builtin_memcpy(&n,&a.value,4);return n; }
const char* name(const GeometryShape& s) { return reinterpret_cast<const char*>(&s); }
bool equal(const char* a,const char* b) { while(*a && *a==*b){++a;++b;}return *a==*b; }
long double floating(const CommandArgument& a) { long double n;asm volatile("flds %1; fstpt %0":"=m"(n):"m"(a.value):"st");return n; }
void add_argument(double& n,const CommandArgument& a) { asm volatile("fldl %0; fadds %1; fstpl %0":"+m"(n):"m"(a.value):"st","memory"); }
double subtract64(const double& a,const double& b) { double out;asm volatile("fldl %1; fsubl %2; fstpl %0":"=m"(out):"m"(a),"m"(b):"st");return out; }
GeometryEditRuntime editing(const GeometryDispatchRuntime& r,GeometryShape* selected) {
    const auto& c=r.commands.construction;return {c.geometry,selected,c.warnings,c.message,c.format};
}
}
bool execute_geometry_command(int id,CommandArguments& a,GeometryCommandState& s,const GeometryDispatchRuntime& r,bool* suppress_recording) {
    bool suppressed=false;const auto& c=r.commands.construction;const int count=static_cast<int>(a.command.value);
    switch(id) {
    case 400: {
        auto& shape=*argument(a,0);if(equal(name(shape),a.arguments[1].text)) { c.warnings.emit("Error:  Cannot rename to same name.");suppressed=true; }
        else rename_geometry_shape(s.head,shape,a.arguments[1].text,editing(r,s.selected));
        break;
    }
    case 401:
        for(int i=0;i<count;++i) {
            auto* shape=argument(a,i);if(s.selected && equal(name(*s.selected),name(*shape)))s.selected=nullptr;
            if(partner(*shape))partner(*shape)->auxiliary=0;
            remove_geometry_shape(s.head,argument(a,i),c.geometry.memory);
        }break;
    case 402: {
        auto& source=*argument(a,0);
        if(equal(name(source),a.arguments[1].text)) { c.warnings.emit("Error:  Cannot copy to same name.");suppressed=true;break; }
        double x=binary64(number(source,0x54)),y=binary64(number(source,0x5c));int metal=-1;
        if(count>2)metal=r.resolve_metal(a.arguments[2].text,r.user);
        if(count>2 && metal==-1) { c.warnings.emit("Error:  Invalid metal layer.");suppressed=true;break; }
        if(count==5) { x=binary64(floating(a.arguments[3]));y=binary64(floating(a.arguments[4])); }
        auto commands=r.commands;commands.selected=s.selected;
        copy_geometry_shape(s.head,source,a.arguments[1].text,x,y,metal,r.technology,commands);
        break;
    }
    case 404: {
        auto& shape=*argument(a,0);const int phase=integer(a.arguments[1]);
        if((phase>=0 && attributes(shape)<0) || (phase<0 && attributes(shape)>0)) { reverse_geometry_directions(shape);shape.attributes=a.arguments[1].value; }
        break;
    }
    case 405:case 411:case 412: {
        auto& shape=*argument(a,0);
        if(id!=412)add_argument(number(shape,0x54),a.arguments[1]);
        if(id!=411)add_argument(number(shape,0x5c),a.arguments[id==405?2:1]);
        if(partner(shape)) {
            if(id!=412)add_argument(number(*partner(shape),0x54),a.arguments[1]);
            if(id!=411)add_argument(number(*partner(shape),0x5c),a.arguments[id==405?2:1]);
        }break;
    }
    case 406:rotate_geometry_shape(*argument(a,0),binary64(floating(a.arguments[1])));break;
    case 407:reflect_geometry_x(*argument(a,0));break;
    case 408:reflect_geometry_y(*argument(a,0));break;
    case 409:case 421: {
        if(id==409)for(int i=0;i<count;++i)for(int j=i+1;j<count;++j)
            if(equal(name(*argument(a,i)),name(*argument(a,j)))) { c.warnings.emit("Error:  Cannot join a spiral to itslef.");if(suppress_recording)*suppress_recording=true;return true; }
        bool selected=s.selected && (equal(name(*argument(a,0)),name(*s.selected)) || equal(name(*argument(a,1)),name(*s.selected)));
        for(int i=1;i<count;++i) {
            if(i>1 && s.selected && equal(name(*argument(a,i)),name(*s.selected)))selected=true;
            if(id==409) { auto commands=r.commands;commands.selected=s.selected;join_geometry_shapes(s.head,*argument(a,0),*argument(a,i),r.technology,commands); }
            else join_shunt_geometry_shapes(s.head,*argument(a,0),*argument(a,i),editing(r,s.selected));
        }
        if(selected)s.selected=argument(a,0);
        if(id==421) {
            auto& shape=*argument(a,0);const int phase=integer(a.arguments[1]);
            if((phase>=0 && attributes(shape)<0) || (phase<0 && attributes(shape)>0)) { reverse_geometry_directions(shape);shape.attributes=a.arguments[1].value; }
        }break;
    }
    case 414:reverse_geometry_segments(*argument(a,0));break;
    case 415: {
        auto& shape=*argument(a,0);const long double x=floating(a.arguments[1]),y=floating(a.arguments[2]);
        if(x<0 || x>s.chip_x || y<0 || y>s.chip_y)c.warnings.emit("Warning:  Moving spiral outside of chip area.");
        double dx=0,dy=0;if(partner(shape)) { dx=subtract64(number(*partner(shape),0x54),number(shape,0x54));dy=subtract64(number(*partner(shape),0x5c),number(shape,0x5c)); }
        number(shape,0x54)=binary64(x);number(shape,0x5c)=binary64(y);
        if(partner(shape)) { add_argument(dx,a.arguments[1]);number(*partner(shape),0x54)=dx;add_argument(dy,a.arguments[2]);number(*partner(shape),0x5c)=dy; }
        break;
    }
    case 416:
        normalize_input_line(a.arguments[2].text,0x20,r.characters);
        unjoin_geometry_shape(s.head,*argument(a,0),integer(a.arguments[1]),a.arguments[2].text,editing(r,s.selected));break;
    case 417:argument(a,0)->auxiliary=a.arguments[1].value;argument(a,1)->auxiliary=a.arguments[0].value;break;
    case 418: {
        auto* p=partner(*argument(a,0));
        if(p==argument(a,1) && partner(*p)==argument(a,0)) { argument(a,0)->auxiliary=0;argument(a,1)->auxiliary=0; }
        else { c.warnings.emit("Error:  Specified spirals are not friends.");suppressed=true; }
        break;
    }
    case 420:
        for(int i=0;i<count;++i) { auto& shape=*argument(a,i);if(shape.suffix[0])shape.suffix[0]=0;
            else { shape.suffix[0]=1;if(s.selected && equal(name(shape),name(*s.selected)))s.selected=nullptr; }
        }break;
    case 422: {
        auto* previous=s.selected;s.selected=count>0?find_geometry_shape(s.head,s.selected,a.arguments[0].text):s.head;
        if(s.selected->suffix[0]) { c.warnings.emit("Warning:  Cannot select a hidden spiral.");s.selected=previous; }break;
    }
    case 423:s.selected=nullptr;break;
    case 424: {
        const int n=convert_geometry_to_wires(s.head,*argument(a,0),editing(r,s.selected));c.format(c.message,"Created %d segments.",n);c.warnings.emit(c.message);break;
    }
    default:return false;
    }
    if(suppress_recording)*suppress_recording=suppressed;
    return true;
}
}
