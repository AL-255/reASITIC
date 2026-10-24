#include "eddy_runtime.hpp"
#include "solver_limits.hpp"
#include "runtime_math.hpp"
#ifdef __EMSCRIPTEN__
#include "x87_port.hpp"
using Real=SoftReal;
#else
#include "x87.hpp"
using Real=long double;
#endif
extern "C" void* calloc(unsigned,unsigned);
extern "C" void free(void*);
extern "C" int sprintf(char*,const char*,...);
extern "C" int vsnprintf(char*,unsigned,const char*,__builtin_va_list);
namespace {
reasitic::EddySession* active=nullptr;
unsigned long long hash_word(unsigned long long hash,double value) {
    unsigned long long word;__builtin_memcpy(&word,&value,8);
    // Invalid evaluations retain their classification in the trace, without
    // claiming cross-architecture identity of NaN payload bits.
    if(__builtin_isnan(value))word=0x7ff8000000000000ULL;
    for(int i=0;i<8;++i){hash^=static_cast<unsigned char>(word);hash*=1099511628211ULL;word>>=8;}return hash;
}
void observe(void* user,double x) {
    auto& s=*static_cast<reasitic::EddySession*>(user);++s.evaluations;s.sample_hash=hash_word(s.sample_hash,x);
    if(s.evaluations>s.limit)s.exceeded=true;
}
Real integrand(const double* x) {
    auto& s=*active;if(s.exceeded)return Real(0);
    const Real value=s.state.kernel==asitic::GreenKernel::reflection?asitic::green_reflection_kernel(*x,s.state.height):asitic::green_oscillating_kernel(*x,s.state.frequency,s.state.layers,s.state.height,s.state.separation);
    s.value_hash=hash_word(s.value_hash,asitic::x87::binary64(value));return value;
}
void diagnostic(void* user,const char* text,int length,int status,int) {
    auto& s=*static_cast<reasitic::EddySession*>(user);if(status>=0 && status<8)++s.status[status];
    char message[1024];int n=length<1023?length:1023;if(n<0)n=0;__builtin_memcpy(message,text,n);message[n]=0;s.warnings.emit(message);
}
int print(const char* format,...) {
    char message[1024];__builtin_va_list args;__builtin_va_start(args,format);const int n=vsnprintf(message,sizeof(message),format,args);__builtin_va_end(args);if(active)active->warnings.emit(message);return n;
}
}
namespace asitic { bool eddy_aborted(){return active && active->exceeded;} }
namespace reasitic {
EddyScope::EddyScope(EddySession* s):session(s),previous(active){active=s;}
EddyScope::~EddyScope(){active=previous;free(session);}
bool configure_eddy(EddySession& s,const EngineView& engine,int mode,int count,int first,int second,int budget) {
    if(mode<0 || mode>maximum_eddy_mode || count<0 || count>maximum_eddy_layers || budget<1 || budget>maximum_green_evaluations)return false;
    const int indices[]={first,second};for(int i=0;i<count;++i)if(indices[i]<0 || indices[i]>=engine.substrate_count || !(engine.substrate[indices[i]].resistivity>0))return false;
    s.mode=mode;s.layer_count=count;s.indices[0]=first;s.indices[1]=second;s.limit=budget;s.warnings=engine.warnings;
    s.sample_hash=s.value_hash=14695981039346656037ULL;
    if(count)asitic::configure_green_layers(s.state,s.layer_count,engine.substrate,engine.substrate_count,indices,count,s.warnings);
    return true;
}
asitic::EddyRunOptions eddy_options(EddySession& s,const EngineView& engine) {
    const asitic::GreenIntegrationRuntime runtime{{runtime_sin,runtime_cos},{diagnostic,&s},print,observe,&s,integrand};
    return {s.mode,s.layer_count,engine.metal_heights,{1e-6,&s.state,&s.workspace,runtime},nullptr,nullptr};
}
char* eddy_metadata(char* out,const EddySession& s) {
    out+=sprintf(out,"{\"mode\":%d,\"layer_count\":%d,\"indices\":[",s.mode,s.layer_count);
    for(int i=0;i<s.layer_count;++i)out+=sprintf(out,"%s%d",i?",":"",s.indices[i]);
    out+=sprintf(out,"],\"kernel\":\"%s\",\"evaluations\":%u,\"sample_hash\":\"%016llx\",\"value_hash\":\"%016llx\",\"quadrature_status\":[",s.state.kernel==asitic::GreenKernel::reflection?"reflection":"oscillating",s.evaluations,s.sample_hash,s.value_hash);
    for(int i=0;i<8;++i)out+=sprintf(out,"%s%u",i?",":"",s.status[i]);out+=sprintf(out,"]}");return out;
}
}
extern "C" const char* probe_green(int count,int first,int second,double frequency,double separation,double height,double width,int budget) {
    static char result[2048];reasitic::clear_diagnostics();
    if(!__builtin_isfinite(frequency) || frequency<0 || !__builtin_isfinite(separation) || !__builtin_isfinite(height) || height<=0 || !__builtin_isfinite(width))return "{\"error\":\"Invalid Green probe dimensions or frequency\"}";
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate Green workspace\"}";
    reasitic::EddyScope scope(session);const auto engine=reasitic::engine_view();
    if(!reasitic::configure_eddy(*session,engine,1,count,first,second,budget))return "{\"error\":\"Invalid eddy layers or evaluation budget\"}";
    auto options=reasitic::eddy_options(*session,engine);const Real value=asitic::green_integral(frequency,separation,height,width,session->state,session->workspace,options.environment.runtime);
    if(session->exceeded)return "{\"error\":\"Green evaluation budget exceeded\"}";const double rounded=asitic::x87::binary64(value);
    if(!__builtin_isfinite(rounded))return "{\"error\":\"Original Green integration produced a nonfinite result\"}";
    char* out=result;out+=sprintf(out,"{\"value\":%.17g,\"eddy\":",rounded);out=reasitic::eddy_metadata(out,*session);sprintf(out,"}");return result;
}
