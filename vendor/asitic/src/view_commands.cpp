#include "asitic/view_commands.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& n) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(n):"st");return out; }
long double load32(const unsigned& n) { long double out;asm volatile("flds %1; fstpt %0":"=m"(out):"m"(n):"st");return out; }
double b64(long double n) { volatile double out=static_cast<double>(n);return out; }
long double add(long double a,long double b) { long double out;asm volatile("fadd %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double sub(long double a,long double b) { long double out;asm volatile("fsub %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double mul(long double a,long double b) { long double out;asm volatile("fmul %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
long double div(long double a,long double b) { long double out;asm volatile("fdiv %%st(1),%%st":"=t"(out):"0"(a),"u"(b));return out; }
const double& number(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const double*>(s.filaments.prefix+offset); }
struct Words { unsigned low,high; };
Words words(long double n) { double d=b64(n);Words out;__builtin_memcpy(&out,&d,8);return out; }
bool equal(const char* a,const char* b) { while(*a && *a==*b){++a;++b;}return *a==*b; }
bool chip(const char* a) { for(;*a;++a)if(a[0]=='C' && a[1]=='H' && a[2]=='I' && a[3]=='P')return true;return false; }
void fit(long double x0,long double y0,long double x1,long double y1,NativeDispatchState& s,ViewCommandState& v) {
    const long double width=sub(x1,x0),height=sub(y1,y0);
    s.pan_x=b64(-add(mul(width,0.5L),x0));s.pan_y=b64(-add(y0,mul(height,0.5L)));
    const long double sx=div(v.canvas_width,width),sy=div(v.canvas_height,height);
    v.zoom=b64(mul(sx<sy?sx:sy,load(0.9)));
}
}
void fit_view_to_shape(const GeometryShape& shape,NativeDispatchState& s,ViewCommandState& v) {
    double x0,y0,x1,y1;geometry_bounds_xy(shape,&x0,&y0,&x1,&y1);
    const long double ox=load(number(shape,0x54)),oy=load(number(shape,0x5c));
    fit(add(load(x0),ox),add(load(y0),oy),add(ox,load(x1)),add(oy,load(y1)),s,v);
}
void fit_view_to_chip(NativeDispatchState& s,ViewCommandState& v) {
    const long double width=load(s.chip_width),height=load(s.chip_height);
    s.pan_x=b64(mul(-width,0.5L));s.pan_y=b64(mul(-height,0.5L));
    long double sx=div(v.canvas_width,width),sy=div(v.canvas_height,height);
    v.zoom=b64(mul(sx<sy?sx:sy,load(0.9)));
}
void fit_view_to_shapes(GeometryShape* head,NativeDispatchState& s,ViewCommandState& v) {
    long double x0=1e15L,y0=1e15L,x1=-1e15L,y1=-1e15L;
    if(!head) { x0=y0=0;x1=load(s.chip_width);y1=load(s.chip_height); }
    else for(auto* shape=head;shape;shape=shape->next) {
        double a,b,c,d;geometry_bounds_xy(*shape,&a,&b,&c,&d);
        const double xmin=b64(add(load(a),load(number(*shape,0x54))));
        const long double xmax=add(load(c),load(number(*shape,0x54)));
        const double ymin=b64(add(load(b),load(number(*shape,0x5c)))),ymax=b64(add(load(d),load(number(*shape,0x5c))));
        if(xmax>x1)x1=xmax;
        if(load(ymax)>y1)y1=load(ymax);
        if(load(xmin)<x0)x0=load(xmin);
        if(load(ymin)<y0)y0=load(ymin);
    }
    const double xmin=b64(x0),ymin=b64(y0),xmax=b64(x1),ymax=b64(y1);
    fit(load(xmin),load(ymin),load(xmax),load(ymax),s,v);
}
bool execute_view_command(int id,CommandArguments& arguments,NativeDispatchState& s,ViewCommandState& v,const ShapePromptRuntime& r,ViewCommandHistory& h,bool* suppress_recording) {
    bool suppress=false;auto& a=arguments.arguments;const int count=static_cast<int>(arguments.command.value);const auto& command=r.command;
    switch(id) {
    case 102: {
        GeometryShape* shape=command.selected;
        if(count==1) {
            normalize_input_line(a[0].text,0x20,command.characters);
            if(chip(a[0].text)) { fit_view_to_chip(s,v);break; }
            shape=find_geometry_shape(command.shapes,command.selected,a[0].text);
            if(!shape) { const long double n=load32(a[0].value);if(n>0)v.zoom=b64(n);break; }
        }
        if(shape)fit_view_to_shape(*shape,s,v);
        else { const auto n=words(load(v.zoom));command.format(command.message,"Current Scale = %lf.",n.low,n.high);command.warnings.emit(command.message); }
        break;
    }
    case 104:
        if(equal(a[0].text,"ALL") || equal(a[0].text,"*")) {
            for(int i=0;i<r.technology.metal_count;++i)v.visible_metals[i]=1;
        }else for(int i=0;i<count;++i) {
            const int index=find_shape_prompt_metal(a[i].text,r,h.metal[i]);
            if(index<0 || index>=r.technology.metal_count) {
                suppress=true;command.format(command.message,"Error:  Unkown metal layer <%s>.",a[i].text);command.warnings.emit(command.message);
            }else v.visible_metals[index]=v.visible_metals[index]==0;
        }break;
    case 109:fit_view_to_shapes(command.shapes,s,v);break;
    case 110: {
        const int half_x=v.canvas_width/2,half_y=v.canvas_height/2;
        const long double zoom=load(v.zoom),x=-load(s.pan_x),y=-load(s.pan_y);
        const auto upper_y=words(add(div(half_y,zoom),y));
        const auto upper_x=words(add(div(v.canvas_width-half_x,zoom),x));
        const auto lower_y=words(add(div(half_y-v.canvas_height,zoom),y));
        const auto lower_x=words(add(div(-half_x,zoom),x));
        command.format(command.message,"Viewport coordinates:\nLower Left:  (%5.2lf,%5.2lf)\nUpper Right:  (%5.2lf,%5.2lf)",lower_x.low,lower_x.high,lower_y.low,lower_y.high,upper_x.low,upper_x.high,upper_y.low,upper_y.high);
        command.warnings.emit(command.message);break;
    }
    case 111: {
        const long double x0=load32(a[0].value),x1=load32(a[2].value),y0=load32(a[1].value),y1=load32(a[3].value);
        const long double dx=sub(x1,x0),dy=sub(y1,y0),distance=x87::sqrt(add(mul(dx,dx),mul(dy,dy)));
        const auto p=words(x0),q=words(y0),u=words(x1),w=words(y1),d=words(distance);
        command.format(command.message,"Dist from (%5.2lf,%5.2lf) to (%5.2lf,%5.2lf) = %5.2lf",p.low,p.high,q.low,q.high,u.low,u.high,w.low,w.high,d.low,d.high);command.warnings.emit(command.message);break;
    }
    default:return false;
    }
    if(suppress_recording)*suppress_recording=suppress;
    return true;
}
}
