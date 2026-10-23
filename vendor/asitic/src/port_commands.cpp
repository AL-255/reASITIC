#include "asitic/port_commands.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
bool same(const char* a,const char* b) { while(*a && *a==*b) { ++a;++b; }return *a==*b; }
bool member(const char* value,const char* const* names) { while(**names) { if(same(value,*names))return true;++names; }return false; }
double floating_copy(const double& value) { double out;asm volatile("fldl %1; fstpl %0":"=m"(out):"m"(value):"st");return out; }
}
int parse_port_parameter_format(char* text,const PortFormatRuntime& runtime) {
    const char* const scattering[]={"S","S PARAM","S PARAMETERS","S PARAMETER","S-PARAM","S-PARAMETERS",""};
    const char* const admittance[]={"Y","Y PARAM","Y PARAMETERS","Y PARAMETER","Y-PARAM","Y-PARAMETERS",""};
    const char* const pi[]={"PI","PI PARAM","PI PARAMETERS","PI PARAMETER","PI-PARAM","PI-PARAMETERS",""};
    const char* const transformer[]={"TRANSPI","TRANS PARAMETERS","TRANS PARAMETER","TRANS-PARAM","TRANS-PARAMETERS",""};
    const char* const impedance[]={"Z","Z PARAM","Z PARAMETERS","Z PARAMETER","Z-PARAM","Z-PARAMETERS",""};
    normalize_input_line(text,0xae,runtime.characters);
    if(member(text,scattering))return 1;
    if(member(text,admittance))return 2;
    if(member(text,pi))return 4;
    if(member(text,transformer))return 5;
    if(member(text,impedance))return 3;
    runtime.format(runtime.message,"Sorry, I don't understand output format '%s'; defaulting to pi.",text);
    runtime.warnings.emit(runtime.message);return 0;
}
bool parse_port_polar_format(char* text,const PortFormatRuntime& runtime) {
    const char* const polar[]={"POLAR","MAGPHASE","CIRCULAR","CIRC","RT",""};
    const char* const rectangular[]={"RECT","SQUARE","REIM","XY",""};
    normalize_input_line(text,0xae,runtime.characters);
    if(member(text,polar))return true;
    if(member(text,rectangular))return false;
    runtime.warnings.emit("Warning:  Cannot understand 8th argument, defaulting to POLAR.");return true;
}
void run_port_frequency_sweep(PortSweepKind kind,const GeometryShape* const* shapes,double termination,
    const PortSweepSettings& settings,const PortSweepRuntime& runtime) {
    const char* filename="";
    if(settings.filename.length) { settings.filename.data[settings.filename.length]=0;filename=settings.filename.data; }
    const double first=floating_copy(settings.first_frequency),last=floating_copy(settings.last_frequency),step=floating_copy(settings.frequency_step);
    const unsigned char polar=settings.polar,option=settings.two_port_option,three_option=settings.three_port_option;
    const int parameter=settings.parameter_format;
    void* file=runtime.open(filename,"a",runtime.user);
    if(!file) { runtime.warnings.emit("Could not open requested file.");return; }
    const char* label=kind==PortSweepKind::Pad?"2PortPad":kind==PortSweepKind::Transformer?"2PortTrans":kind==PortSweepKind::ThreePort?"3Port":"2Port";
    const bool pair=kind==PortSweepKind::Ground || kind==PortSweepKind::Extended;
    runtime.header(file,pair?option:0,shapes[0],label,runtime.user);
    if(kind==PortSweepKind::ThreePort)runtime.three_port_columns(file,runtime.user);
    else runtime.two_port_columns(file,parameter,polar,runtime.user);
    double frequency=first;long double test=first;
    while(test<=static_cast<long double>(last)) {
        runtime.flush(file,runtime.user);
        if(!pair && settings.progress && *settings.progress)runtime.update_frequency_geometry(frequency,runtime.user);
        const bool success=runtime.analyze(kind,shapes,frequency,termination,
            kind==PortSweepKind::ThreePort?three_option:pair?option:0,kind==PortSweepKind::ThreePort?three_option!=0:true,runtime.user);
        if(pair && !success)break;
        if(kind==PortSweepKind::ThreePort)runtime.three_port_record(file,frequency,runtime.user);
        else runtime.two_port_record(file,frequency,parameter,polar,runtime.user);
        test=static_cast<long double>(frequency)+static_cast<long double>(step);
        frequency=x87::binary64(test);
    }
    runtime.close(file,runtime.user);
}
}
