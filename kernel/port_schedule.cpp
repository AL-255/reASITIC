#include "asitic/port_commands.hpp"
#include "solver_limits.hpp"
extern "C" int sprintf(char*,const char*,...);
namespace {
struct Schedule {
    double values[reasitic::maximum_port_samples]{};int count=0,limit;bool stalled=false,exceeded=false;
};
void* open(const char*,const char*,void* user){return user;}
int finish(void*,void*){return 0;}
void header(void*,unsigned char,const asitic::GeometryShape*,const char*,void*){}
void columns(void*,int,unsigned char,void*){}
void three_columns(void*,void*){}
void update(double,void*){}
void record(void*,double,int,unsigned char,void*){}
void three_record(void*,double,void*){}
bool sample(asitic::PortSweepKind,const asitic::GeometryShape* const*,double frequency,double,unsigned char,bool,void* user){
    auto& s=*static_cast<Schedule*>(user);
    if(s.count && frequency<=s.values[s.count-1]){s.stalled=true;return false;}
    if(s.count==s.limit){s.exceeded=true;return false;}
    s.values[s.count++]=frequency;return true;
}
}
// Execute the recovered frequency loop with a recording analysis callback. Its
// recurrence and unrounded upper-bound comparison are shared by all port kinds.
// The Ground callback contract permits a bounded stop before any solver runs.
extern "C" const char* port_frequency_schedule(double first,double last,double step,int limit){
    static char output[4096];
    if(!__builtin_isfinite(first)||!__builtin_isfinite(last)||!__builtin_isfinite(step)||first<=0||last<first||step<=0||limit<1||limit>reasitic::maximum_port_samples)
        return "{\"error\":\"Use positive finite start/stop/step frequencies, an ascending range, and a sample limit from 1 to 101\"}";
    Schedule schedule{};schedule.limit=limit;
    asitic::PortSweepSettings settings{};settings.first_frequency=first;settings.last_frequency=last;settings.frequency_step=step;
    const asitic::GeometryShape* shapes[]={nullptr,nullptr,nullptr,nullptr};
    const asitic::PortSweepRuntime runtime{open,finish,finish,header,columns,three_columns,update,sample,record,three_record,{},&schedule};
    asitic::run_port_frequency_sweep(asitic::PortSweepKind::Ground,shapes,0,settings,runtime);
    if(schedule.stalled)return "{\"error\":\"The frequency step does not advance the saved native frequency; increase the step\"}";
    if(schedule.exceeded)return "{\"error\":\"The frequency range exceeds the selected sample limit; increase the step or shorten the range\"}";
    char* out=output;out+=sprintf(out,"{\"frequencies_hz\":[");
    for(int i=0;i<schedule.count;++i)out+=sprintf(out,"%s%.17g",i?",":"",schedule.values[i]);sprintf(out,"]}");return output;
}
