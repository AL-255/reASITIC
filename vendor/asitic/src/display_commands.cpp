#include "asitic/display_commands.hpp"
namespace asitic {
namespace {
struct Words { unsigned low,high; };
Words words(const double& x) { Words w;__builtin_memcpy(&w,&x,8);return w; }
void copy(char* d,const char* s) { do { *d=*s;++d; } while(*s++); }
void append(char* d,const char* s) { while(*d)++d;copy(d,s); }
void one(const char* format,const double& n,const CommandRuntime& r) {
    const auto a=words(n);r.format(r.message,format,a.low,a.high);r.warnings.emit(r.message);
}
void two(const char* format,const double& x,const double& y,const CommandRuntime& r) {
    const auto a=words(x),b=words(y);r.format(r.message,format,a.low,a.high,b.low,b.high);r.warnings.emit(r.message);
}
}
void print_view_options(const ViewOptionDisplay& s,const CommandRuntime& r) {
    copy(r.message,"Invisible metal layers:  ");char local[80];
    for(int i=0;i<s.metal_count;++i)if(!s.visible[i]) {
        const unsigned address=reinterpret_cast<unsigned long>(s.metals)+static_cast<unsigned>(i)*0xecu;
        r.format(local,"%d ",static_cast<int>(address));append(r.message,local);
    }
    r.warnings.emit(r.message);
    if(s.snap_enabled)one("Snap to grid is on and set to %5.2lf.",s.snap_size,r);
    else r.warnings.emit("Snap to grid is OFF.");
    if(s.grid_enabled)one("The viewing grid is on and set to %5.2lf",s.grid_size,r);
    else r.warnings.emit("Grid is OFF.");
    one("The current view scale is %5.2lf",s.view_scale,r);
    r.warnings.emit(s.technology_io?"Data files are stored and read from disk.":"Data files are not stored to disk.");
}
void print_port_options(const PortOptionDisplay& s,const CommandRuntime& r) {
    r.format(r.message,"Verbose = %s",static_cast<unsigned>(s.verbose));r.warnings.emit(r.message);
    r.format(r.message,"AutoCell = %s",static_cast<unsigned>(s.automatic_cells));r.warnings.emit(r.message);
    if(s.automatic_cells)two("alpha = %5.2lf, beta = %5.2lf",s.alpha,s.beta,r);
    else {
        const auto a=words(s.maximum_length),b=words(s.maximum_width),c=words(s.maximum_thickness);
        r.format(r.message,"maxL = %5.2lf, maxW = %5.2lf, maxT = %5.2lf",a.low,a.high,b.low,b.high,c.low,c.high);r.warnings.emit(r.message);
        two("CmaxL = %5.2lf, CmaxW = %5.2lf",s.cap_length,s.cap_width,r);
    }
    r.format(r.message,"maxNW = %3d",s.maximum_width_cells);r.warnings.emit(r.message);
    r.warnings.emit(s.eddy?"Eddy current calculation is ON.":"Eddy current calculation is OFF.");
    copy(r.message,"Output Style:  ");append(r.message,s.polar?"Polar":"Rectangular");
    const char* format=nullptr;
    switch(s.parameter_type) {
    case 1:format=" S-Parameters";break;case 2:format=" Y-Parameters";break;case 3:format=" Z-Parameters";break;
    case 4:format=" Pi-Parameters";break;case 5:format=" Trans-Parameters";break;default:break;
    }
    if(format)r.format(r.message,format);
    r.warnings.emit(r.message);
    r.format(r.message,"Fast calculation mode:  %s",static_cast<unsigned>(s.fast));r.warnings.emit(r.message);
    const char* name="";
    if(s.filename_length) { s.filename[s.filename_length]=0;name=s.filename; }
    r.format(r.message,"Output filename:  ",name);r.warnings.emit(r.message);
}
void print_native_options(const ViewOptionDisplay& state,const CommandRuntime& runtime) { print_view_options(state,runtime); }
void record_command_transcript(const CommandArguments& a,const CommandRecordingRuntime& r) {
    r.print(r.output_log,"\n%s ",a.command.text);r.print(r.input_log,"\nASITIC>  %s ",a.command.text);
    for(int i=0;i<static_cast<int>(a.command.value);++i) {
        r.print(r.output_log,"%s ",a.arguments[i].text);r.print(r.input_log,"%s ",a.arguments[i].text);
    }
}
bool execute_display_command(int id,const CommandArguments&,const ViewOptionDisplay& state,const CommandRuntime& r) {
    if(id==208) { r.format(r.message,"ASITIC version %s","10.20.99.0.0.0");r.warnings.emit(r.message);return true; }
    if(id==219) { print_native_options(state,r);return true; }return false;
}
}
