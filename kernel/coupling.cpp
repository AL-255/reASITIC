#include "engine.hpp"
#include "json.hpp"
#include "asitic/analysis_commands.hpp"
#ifdef __EMSCRIPTEN__
#include "x87_port.hpp"
#else
#include "x87.hpp"
#endif
extern "C" void* calloc(unsigned,unsigned);
extern "C" void free(void*);
extern "C" int sprintf(char*,const char*,...);
using namespace asitic;
namespace {
const ConductorSegment* next(const ConductorSegment* p) { return reinterpret_cast<const ConductorSegment*>(p->next_address); }
// The original translates endpoints in place and can retain translations on
// error. Copy its main chain without discretizing or touching the live graph.
struct PrivateShape {
    ShapeFilamentList shape{};ConductorSegment* records=nullptr;
    ~PrivateShape(){free(records);}
    bool copy(const ShapeFilamentList& source,unsigned count){
        shape=source;shape.first=nullptr;if(!count)return true;
        records=static_cast<ConductorSegment*>(calloc(count,sizeof(ConductorSegment)));if(!records)return false;
        unsigned i=0;for(auto* p=source.first;p;p=next(p),++i){records[i]=*p;records[i].group_next_address=0;records[i].next_address=i+1<count?reinterpret_cast<unsigned>(records+i+1):0;}
        shape.first=records;return true;
    }
};
char output[1024];
}
extern "C" const char* coupling(int first,int second,int pair_limit){
    reasitic::clear_diagnostics();const auto engine=reasitic::engine_view();
    if(first<0||second<0||first==second)return "{\"error\":\"Select two distinct windings for COUPLING\"}";
    if(pair_limit<1||pair_limit>1000000)return "{\"error\":\"Use a coupling evaluation budget from 1 to 1000000\"}";
    const GeometryShape* sources[2]{};int index=0;
    for(auto* p=engine.shapes;p;p=p->next,++index){if(index==first)sources[0]=p;if(index==second)sources[1]=p;}
    if(!sources[0]||!sources[1])return "{\"error\":\"Select existing windings for COUPLING\"}";
    unsigned counts[2]{},conductors[2]{};
    for(int i=0;i<2;++i)for(auto* p=sources[i]->filaments.first;p;p=next(p)){
        if(++counts[i]>60000)return "{\"error\":\"Coupling geometry exceeds the segment budget\"}";
        if(p->metal<engine.metal_count)++conductors[i];
    }
    const unsigned long long total=conductors[0]+conductors[1],evaluations=total>1?total*(total-1)/2:0;
    if(evaluations>static_cast<unsigned>(pair_limit))return "{\"error\":\"Coupling estimate exceeds the selected segment-pair budget\"}";
    PrivateShape a,b;
    if(!a.copy(sources[0]->filaments,counts[0])||!b.copy(sources[1]->filaments,counts[1]))return "{\"error\":\"Unable to allocate private coupling geometry\"}";
    double mutual=0;const ResistanceTechnology technology{engine.metals,engine.vias,engine.metal_count};
    const double coefficient=x87::binary64(shape_coupling_estimate(a.shape,b.shape,mutual,technology,engine.warnings));
    if(!__builtin_isfinite(coefficient)||!__builtin_isfinite(mutual))return "{\"error\":\"Original coupling estimate produced a nonfinite result\"}";
    char* out=output;out+=sprintf(out,"{\"first\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(sources[0]));out+=sprintf(out,",\"second\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(sources[1]));
    sprintf(out,",\"mutual_nh\":%.17g,\"coefficient\":%.17g,\"segment_pairs\":%u,\"evaluation_pairs\":%u}",mutual,coefficient,conductors[0]*conductors[1],static_cast<unsigned>(evaluations));return output;
}
