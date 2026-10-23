#include "asitic/port_dispatch.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double number(const CommandArgument& argument) {
    long double value;asm volatile("flds %1":"=t"(value):"m"(argument.value));return value;
}
double floating_copy(const double& value) { double out;asm volatile("fldl %1; fstpl %0":"=m"(out):"m"(value):"st");return out; }
unsigned length(const char* text) { unsigned n=0;while(text[n])++n;return n; }
bool contains(const char* text,const char* needle) { for(;*text;++text) { unsigned n=0;while(needle[n] && needle[n]==text[n])++n;if(!needle[n])return true; }return false; }
bool less(long double value,const double& bound) {
    unsigned short flags;asm volatile("fcoml %2; fnstsw %%ax":"=a"(flags):"t"(value),"m"(bound):"cc");return (flags&0x4500u)==0x0100u;
}
bool below_step_limit(const double& value) {
    const double bound=1e-10;unsigned short flags;
    asm volatile("fldl %1; fcompl %2; fnstsw %%ax":"=a"(flags):"m"(bound),"m"(value):"st","cc");return (flags&0x4500u)==0;
}
void range(const CommandArguments& arguments,unsigned index,bool multiple,bool clamp,double& first,double& last,double& step) {
    const auto& a=arguments.arguments;
    if(!multiple) { const long double value=number(a[index]);first=x87::binary64(value);last=x87::binary64(value);step=x87::binary64(value);return; }
    first=x87::binary64(number(a[index]));
    const long double end=number(a[index+1]);last=x87::binary64(end);step=x87::binary64(number(a[index+2]));
    if(less(end,first)) { const double saved=first;last=floating_copy(first);if(!clamp)first=x87::binary64(end);else first=saved; }
    if(below_step_limit(step))step=floating_copy(first);
}
void replace(PortSweepSettings& state,const PortDispatchRuntime& runtime,unsigned position,unsigned remove,const char* text) {
    runtime.replace_filename(state.filename,position,remove,text,length(text),runtime.user);
}
const GeometryShape* shape(const CommandArgument& argument) { return reinterpret_cast<const GeometryShape*>(argument.value); }
}
bool execute_port_command(int id,CommandArguments& arguments,PortSweepSettings& state,const PortDispatchRuntime& runtime) {
    if(id!=524 && id!=529 && id!=530 && id!=536 && id!=539)return false;
    auto& a=arguments.arguments;const int count=static_cast<int>(arguments.command.value);
    const bool extended=id==539,transformer=id==524,pad=id==530,three=id==536;
    const unsigned index=extended?1:pad || three?3:2;
    const GeometryShape* shapes[4]={shape(a[0]),extended?shape(a[0]):shape(a[1]),pad || three?shape(a[2]):nullptr,nullptr};
    if(id==529 || pad)state.two_port_option=0;
    double first,last,step;
    range(arguments,index,count>(extended?3:pad?5:4),transformer,first,last,step);
    int parameter=pad?state.parameter_format:1;
    const unsigned format_index=extended?4:pad || three?6:5;
    if(count>static_cast<int>(format_index)) {
        parameter=parse_port_parameter_format(a[format_index].text,runtime.formats);
        if(pad)state.parameter_format=parameter;
    }
    if(extended || three)state.polar=1;
    if(count>static_cast<int>(format_index+1))state.polar=parse_port_polar_format(a[format_index+1].text,runtime.formats);
    if(extended) {
        state.two_port_option=1;
        if(count>6) {
            normalize_input_line(a[6].text,32,runtime.formats.characters);
            if(contains(a[6].text,"SLOW"))state.two_port_option=0;
            else {
                normalize_input_line(a[6].text,32,runtime.formats.characters);
                if(!contains(a[6].text,"FAST"))runtime.formats.warnings.emit("Warning:  Cannot understand 6th argument, defaulting to FAST.");
            }
        }
    }
    if(three) {
        replace(state,runtime,0,~0u,reinterpret_cast<const char*>(shapes[0]));
        replace(state,runtime,state.filename.length,0,"_");
        replace(state,runtime,state.filename.length,0,reinterpret_cast<const char*>(shapes[1]));
        replace(state,runtime,state.filename.length,0,".out");
    }
    const unsigned file_index=extended?7:pad || three?8:7;
    if(count>static_cast<int>(file_index))replace(state,runtime,0,~0u,a[file_index].text);
    if(extended)shapes[1]=count>8?shape(a[8]):shapes[0];
    else if(pad || three)shapes[3]=count>9?shape(a[9]):shapes[0];
    else if(transformer)shapes[2]=count>8?shape(a[8]):shapes[0];
    const double termination=transformer && count>9?x87::binary64(number(a[9])):0.;
    state.first_frequency=floating_copy(first);state.last_frequency=floating_copy(last);state.frequency_step=floating_copy(step);
    if(!pad)state.parameter_format=parameter;
    const auto kind=extended?PortSweepKind::Extended:transformer?PortSweepKind::Transformer:pad?PortSweepKind::Pad:three?PortSweepKind::ThreePort:PortSweepKind::Ground;
    runtime.sweep(kind,shapes,termination,state,runtime.sweep_runtime);return true;
}
}
