#include "asitic/native_dispatch.hpp"
namespace asitic {
namespace {
struct Words { unsigned low,high; };
Words words(const double& n) { Words result;__builtin_memcpy(&result,&n,8);return result; }
long double floating(const unsigned& n) {
    long double result;asm volatile("flds %1; fstpt %0":"=m"(result):"m"(n):"st");return result;
}
long double floating(const float& n) {
    long double result;asm volatile("flds %1; fstpt %0":"=m"(result):"m"(n):"st");return result;
}
double binary64(long double n) { volatile double result=static_cast<double>(n);return result; }
float binary32(long double n) { volatile float result=static_cast<float>(n);return result; }
void subtract_float(double& target,const unsigned& n) {
    asm volatile("fldl %0; fsubs %1; fstpl %0":"+m"(target):"m"(n):"st");
}
void print2(const char* format,const double& a,const double& b,const CommandRuntime& r) {
    const auto x=words(a),y=words(b);r.format(r.message,format,x.low,x.high,y.low,y.high);r.warnings.emit(r.message);
}
void print3(const char* format,const double& a,const double& b,const double& c,const CommandRuntime& r) {
    const auto x=words(a),y=words(b),z=words(c);r.format(r.message,format,x.low,x.high,y.low,y.high,z.low,z.high);r.warnings.emit(r.message);
}
bool equal(const char* a,const char* b) { while(*a && *a==*b){++a;++b;}return *a==*b; }
}
bool execute_native_command(int id,const CommandArguments& arguments,NativeDispatchState& s,const CommandRuntime& r,bool* suppress_recording) {
    bool suppressed=false;
    const auto& a=arguments.arguments;
    const int count=static_cast<int>(arguments.command.value);
    switch(id) {
    case -3:case 103:case 800:break;
    case -2:case -1:r.warnings.emit("Unknown or Mistyped Commnd");break;
    case 100:subtract_float(s.pan_x,a[0].value);break;
    case 101:subtract_float(s.pan_y,a[0].value);break;
    case 105:s.pan_x=0;s.pan_y=0;break;
    case 106:subtract_float(s.pan_x,a[0].value);subtract_float(s.pan_y,a[1].value);break;
    case 107:
        if(count==2) { s.pan_x=binary64(floating(a[0].value));s.pan_y=binary64(floating(a[1].value));print2("New origin = %lf, %lf.",s.pan_x,s.pan_y,r); }
        else if(!count)print2("Current Position:  %lf, %lf.",s.pan_x,s.pan_y,r);
        break;
    case 113:s.show_dimensions=s.show_dimensions?0:1;break;
    case 114:case 115:case 116: {
        float* target=id==114?s.origin3d:id==115?s.scale3d:s.angle3d;
        double x,y,z;
        const char* format;
        if(count==3) {
            const long double first=floating(a[0].value),second=floating(a[1].value),third=floating(a[2].value);
            target[0]=binary32(first);target[1]=binary32(second);target[2]=binary32(third);
            x=binary64(first);y=binary64(second);z=binary64(third);
            format=id==114?"New origin = (%5.3f, %5.3f, %5.3f).":id==115?"New scale = (%5.3f, %5.3f, %5.3f).":"New angle = (%5.3f, %5.3f, %5.3f).";
        } else if(!count) {
            x=binary64(floating(target[0]));y=binary64(floating(target[1]));z=binary64(floating(target[2]));
            format=id==114?"Current Position:  (%5.3f, %5.2f, %5.3f).":id==115?"Current scale:  (%5.3f, %5.2f, %5.3f).":"Current angle:  (%5.3f, %5.2f, %5.3f).";
        } else { r.warnings.emit("Specify at least three arguments.");break; }
        print3(format,x,y,z,r);break;
    }
    case 217:
        if(!count)print2("Current Chip Size:  %5.2lf x %5.2lf",s.chip_width,s.chip_height,r);
        if(static_cast<int>(arguments.command.value)>0) {
            const long double n=floating(a[0].value);
            if(n>0)s.chip_width=binary64(n);
            else { suppressed=true;r.warnings.emit("Error:  Chip x-dim must be non-negative."); }
        }
        if(static_cast<int>(arguments.command.value)>1) {
            const long double n=floating(a[1].value);
            if(n>0)s.chip_height=binary64(n);
            else { suppressed=true;r.warnings.emit("Error:  Chip y-dimensions must be non-negative."); }
        }break;
    case 218:
        if(count) { if(static_cast<int>(a[0].value)>0)s.maximum_width_cells=static_cast<int>(a[0].value); }
        else { r.format(r.message,"maxNW = %5d",s.maximum_width_cells);r.warnings.emit(r.message); }break;
    case 220:
        if(!count)s.verbose=s.verbose?0:1;
        else if(equal(a[0].text,"TRUE") || equal(a[0].text,"YES") || equal(a[0].text,"T"))s.verbose=1;
        else if(equal(a[0].text,"FALSE") || equal(a[0].text,"NO") || equal(a[0].text,"N"))s.verbose=0;
        else { suppressed=true;r.warnings.emit("Error:  Did not understand argument, specify 'true' or 'false'"); }break;
    case 221:
        s.eddy=count==1?(static_cast<unsigned char>(a[0].value)!=0):(s.eddy==0);
        r.warnings.emit(s.eddy?"Turning ON eddy current calculation mode.":"Turning OFF eddy current calculation mode.");break;
    case 228:case 229:case 230: {
        unsigned char& target=id==228?s.timing:id==229?s.dump_matrices:s.opengl;
        if(count==1)target=static_cast<unsigned char>(a[0].value);else target^=1;
        const char* message=id==228?(target?"Turning ON Timer Mode.":"Turning OFF Timer Mode."):id==229?(target?"Turning ON Matrix Dump Mode.":"Turning OFF Matrix Dump Mode."):(target?"Turning ON OpenGL 3D Interface.":"Turning OFF OpenGL 3D Interface.");
        r.warnings.emit(message);break;
    }
    default:return false;
    }
    if(suppress_recording)*suppress_recording=suppressed;
    return true;
}
}
