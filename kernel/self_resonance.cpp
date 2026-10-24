#include "asitic/analysis_commands.hpp"
#include "json.hpp"
#include "two_port_records.hpp"
#ifdef __EMSCRIPTEN__
#include "x87_port.hpp"
#include <setjmp.h>
using Real=SoftReal;
#else
#include "x87.hpp"
using Real=long double;
using jmp_buf=int[64];
extern "C" int setjmp(int*) __attribute__((returns_twice));
extern "C" [[noreturn]] void longjmp(int*,int);
#endif
extern "C" int sprintf(char*,const char*,...);
namespace asitic { void capacitor_port_admittance(const ComplexMatrix&,TwoPortAdmittance&); }
namespace {
struct Row { double low,high,frequency,imaginary; };
struct Replay {
    const double* history;int count,index,limit,status,rows;double requested,last;
    asitic::TwoPortAdmittance y;Row trace[100];bool positive,negative;
    char message[512],reported[512];
};
Replay replay{};jmp_buf search_jump;
void analyze(const asitic::GeometryShape&,double frequency,int,const asitic::GeometryShape&,void*){
    if(replay.index==replay.count){replay.requested=frequency;replay.status=replay.index==replay.limit?2:1;longjmp(search_jump,1);}
    const double* sample=replay.history+replay.index*9;
    if(sample[0]!=frequency){replay.status=3;longjmp(search_jump,1);}
    __builtin_memcpy(&replay.y,sample+1,64);replay.last=frequency;++replay.index;
}
int progress(const char* format,...){
    __builtin_va_list args;__builtin_va_start(args,format);
    Row row{__builtin_va_arg(args,double),__builtin_va_arg(args,double),__builtin_va_arg(args,double),__builtin_va_arg(args,double)};
    __builtin_va_end(args);
    if(!__builtin_isfinite(row.imaginary)){replay.status=4;longjmp(search_jump,1);}
    if(replay.rows<100)replay.trace[replay.rows++]=row;
    replay.positive|=row.imaginary>0;replay.negative|=row.imaginary<0;return 0;
}
int flush(void*){return 0;}
void report(const char* text,void*){unsigned n=0;while(text[n]&&n<511){replay.reported[n]=text[n];++n;}replay.reported[n]=0;}
}

// Reverse communication through deterministic replay: stop at the first
// uncached analysis request. The worker supplies that model and yields before
// replaying the unchanged recovered loop. No bisection arithmetic runs in JS.
extern "C" const char* self_resonance_plan(double first,double second,double step,int differential,int fast,int ignored_iterations,int limit,const double* history,int count){
    static char output[24576];
    if(!__builtin_isfinite(first)||!__builtin_isfinite(second)||!__builtin_isfinite(step)||first<=0||second<=0||step<=0||
       (differential!=0&&differential!=1)||(fast!=0&&fast!=1)||limit<1||limit>101||count<0||count>limit||(count&&!history))
        return "{\"error\":\"Use positive finite search bounds/step and a sample budget from 1 to 101\"}";
    for(int i=0;i<count*9;++i)if(!__builtin_isfinite(history[i]))return "{\"error\":\"Search history must contain finite frequencies and admittance values\"}";
    replay={};replay.history=history;replay.count=count;replay.limit=limit;Real returned=0;
    if(!setjmp(search_jump)){
        asitic::GeometryShape shape{};
        const asitic::AnalysisReportRuntime reports{replay.message,sprintf,{report,nullptr},nullptr,nullptr,nullptr,nullptr};
        const asitic::SelfResonanceRuntime runtime{analyze,nullptr,&replay.y,progress,flush,nullptr,reports};
        returned=asitic::find_self_resonant_frequency(shape,first,second,step,differential,fast,ignored_iterations,shape,runtime);
    }
    if(replay.status==3||(!replay.status&&replay.index!=count))return "{\"error\":\"Search history does not match the original requested frequency sequence\"}";
    if(replay.status==4)return "{\"error\":\"The model did not produce finite input impedance for this search mode\"}";
    if((replay.status==1||replay.status==2)&&(!__builtin_isfinite(replay.requested)||replay.requested<=0))return "{\"error\":\"The original search requested an invalid model frequency\"}";
    const bool found=!replay.status&&returned>=Real(0);
    const double frequency=asitic::x87::binary64(returned);
    if(found&&!__builtin_isfinite(frequency))return "{\"error\":\"The original search returned an unrepresentable frequency\"}";
    char* out=output;out+=sprintf(out,"{\"status\":\"%s\",\"analyses\":%d,\"sample_budget\":%d,\"differential\":%s,\"fast_quantization\":%s,\"observed_sign_change\":%s",replay.status==1?"need_sample":replay.status==2?"sample_limit":found?"found":"native_limit",replay.index,limit,differential?"true":"false",fast?"true":"false",replay.positive&&replay.negative?"true":"false");
    if(replay.status==1||replay.status==2)out+=sprintf(out,",\"next_frequency_hz\":%.17g",replay.requested);
    if(replay.index)out+=sprintf(out,",\"last_analyzed_hz\":%.17g",replay.last);
    if(found){
        const Real imaginary=asitic::two_port_imaginary_impedance(replay.y,differential);
        out+=sprintf(out,",\"frequency_hz\":%.17g,\"convergence\":\"%s\"",frequency,asitic::x87::abs(imaginary)<Real(0x1.47ae147ae147bp-7L)?"impedance":"frequency_step");
    }
    out+=sprintf(out,",\"trace\":[");
    for(int i=0;i<replay.rows;++i){const auto& r=replay.trace[i];out+=sprintf(out,"%s{\"lower_ghz\":%.17g,\"upper_ghz\":%.17g,\"analyzed_ghz\":%.17g,\"imaginary_impedance_ohm\":%.17g}",i?",":"",r.low,r.high,r.frequency,r.imaginary);}
    out+=sprintf(out,"],\"native_message\":");out=reasitic::json_string(out,replay.reported);sprintf(out,"}");return output;
}

// The capacitor branch of the original grounded analysis uses this conversion
// rather than the inductive PI2 node system. Input is a row-major 2x2 C matrix.
namespace {
const char* capacitor_ports(const double* values,asitic::TwoPortAdmittance& ports){
    if(!values)return "{\"error\":\"Supply the folded 2x2 capacitor matrix\"}";
    for(int i=0;i<8;++i)if(!__builtin_isfinite(values[i]))return "{\"error\":\"Capacitor admittance must be finite\"}";
    asitic::ComplexValue cells[]={{values[0],values[1]},{values[4],values[5]},{values[2],values[3]},{values[6],values[7]}};
    const asitic::ComplexMatrix cap{cells,4,0,2,2,2,0};asitic::capacitor_port_admittance(cap,ports);
    const auto* p=reinterpret_cast<const double*>(&ports);
    for(int i=0;i<8;++i)if(!__builtin_isfinite(p[i]))return "{\"error\":\"Capacitor port conversion overflowed\"}";
    return nullptr;
}
}
extern "C" const char* capacitor_port_matrix(const double* values){
    asitic::TwoPortAdmittance ports{};const auto* error=capacitor_ports(values,ports);if(error)return error;
    static char output[512];const auto* p=reinterpret_cast<const double*>(&ports);
    sprintf(output,"{\"matrix\":[[[%.17g,%.17g],[%.17g,%.17g]],[[%.17g,%.17g],[%.17g,%.17g]]]}",p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);return output;
}
extern "C" const char* capacitor_port_response(const double* values,double frequency){
    if(!__builtin_isfinite(frequency)||frequency<=0)return "{\"error\":\"Use a positive finite capacitor port frequency\"}";
    asitic::TwoPortAdmittance ports{};const auto* error=capacitor_ports(values,ports);if(error)return error;
    static char output[16384];const auto* p=reinterpret_cast<const double*>(&ports);char* out=output;
    out+=sprintf(out,"{\"model\":\"CAP\",\"frequency_hz\":%.17g,\"matrix\":[[[%.17g,%.17g],[%.17g,%.17g]],[[%.17g,%.17g],[%.17g,%.17g]]],\"two_port\":",frequency,p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);
    // Capacitive dispatch updates Y but leaves the command's previous Pi
    // scalar bank untouched. Export only the S/Y records defined by this solve.
    out=reasitic::two_port_json(out,ports,nullptr,false,frequency,false);sprintf(out,"}");return output;
}
