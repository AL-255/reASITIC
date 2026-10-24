#include "engine.hpp"
#include "asitic/composite_geometry.hpp"
#include "asitic/symmetric_construction.hpp"
#include "asitic/transformer3d.hpp"
#include "asitic/resistance.hpp"
#include "asitic/analysis_commands.hpp"
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
extern "C" void abort();
using namespace asitic;
namespace {
GeometryShape* head=nullptr;
MetalLayer metals[32]{};ViaLayer vias[32]{};double heights[32]{};int metal_count=0,via_count=0;
double substrate[32]{};int substrate_count=0;
double origin_x=0,origin_y=0,parsed_ring_gap=0;int winding_phase=1;bool has_parsed_gap=false;
char result[4*1024*1024],message[1024],warnings[4096];unsigned warning_length=0;
void* allocate(unsigned n,void*) { return calloc(1,n+16); } // pad native MMSQUARE pointer workspace overrun
void release(void* p,void*) { free(p); }
void warn(const char* s,void*) { if(warning_length>=sizeof(warnings)-2)return;while(*s && warning_length<sizeof(warnings)-2)warnings[warning_length++]=*s++;warnings[warning_length++]='\n';warnings[warning_length]=0; }
void terminate(int,void*) { abort(); }
void invalid(void*) { abort(); }
int random(void*) { return 65; }
double& number(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(s.filaments.prefix+offset); }
int& integer(GeometryShape& s,unsigned offset) { return *reinterpret_cast<int*>(s.filaments.prefix+offset); }
GeometryRuntime geometry{{{allocate,nullptr},release},{warn,nullptr},terminate,nullptr};
void clear() { reasitic::release_simulation_result();if(head)release_geometry_shapes(head,geometry.memory);head=nullptr;warning_length=0;warnings[0]=0; }
}
namespace reasitic {
EngineView engine_view() { return {head,metals,vias,metal_count,{warn,nullptr}}; }
void clear_diagnostics() { warning_length=0;warnings[0]=0; }
}
extern "C" {
void reset_technology() { clear();metal_count=via_count=substrate_count=0;origin_x=origin_y=0;winding_phase=1; }
void set_geometry_options(double x,double y,int phase) { origin_x=x;origin_y=y;winding_phase=phase?phase:1;has_parsed_gap=false; }
void set_ring_gap_radians(double gap) { parsed_ring_gap=gap;has_parsed_gap=true; }
void set_substrate(int i,double thickness) { if(i<0 || i>=32)return;substrate[i]=thickness;if(i>=substrate_count)substrate_count=i+1; }
void finalize_technology() { for(int i=substrate_count-2;i>=0;--i)substrate[i]=x87::binary64(Real(substrate[i])+substrate[i+1]); }
void set_metal(int i,double height,double thickness,double resistance,int layer) {
    if(i<0 || i>=32)return;if(layer<0 || layer>=substrate_count)return;
    const double z=x87::binary64(Real(substrate[layer])-height);
    heights[i]=x87::binary64(Real(substrate[layer])-z);metals[i].thickness=thickness;metals[i].resistance=x87::binary64(Real(resistance)*Real(0.001));
    if(i>=metal_count)metal_count=i+1;
}
void set_via(int i,int bottom,int top,double width,double spacing,double over1,double over2,double resistance) {
    if(i<0 || i>=32)return;auto* p=reinterpret_cast<unsigned char*>(&vias[i]);
    *reinterpret_cast<double*>(p+0xa0)=width;*reinterpret_cast<double*>(p+0xa8)=spacing;
    *reinterpret_cast<double*>(p+0xb0)=over1;*reinterpret_cast<double*>(p+0xb8)=over2;
    *reinterpret_cast<double*>(p+0xc0)=resistance;*reinterpret_cast<int*>(p+0xcc)=bottom;*reinterpret_cast<int*>(p+0xd0)=top;
    if(i>=via_count)via_count=i+1;
}
const char* analyze(double frequency_hz) {
    warning_length=0;warnings[0]=0;
    if(!head || !__builtin_isfinite(frequency_hz) || frequency_hz<=0)return "{\"error\":\"Generate a layout and supply a positive frequency\"}";
    char* out=result;out+=sprintf(out,"{\"frequency_hz\":%.17g,\"windings\":[",frequency_hz);int index=0;
    for(auto* shape=head;shape;shape=shape->next,++index) {
        ResistanceTechnology tech{metals,vias,metal_count};
        const double l=x87::binary64(shape_inductance_estimate(shape->filaments,tech,{warn,nullptr}));
        const double r=x87::binary64(shape_resistance_high_frequency(shape->filaments,frequency_hz,tech));
        if(!__builtin_isfinite(l) || !__builtin_isfinite(r))return "{\"error\":\"Original estimate produced a nonfinite result\"}";
        out+=sprintf(out,"%s{\"name\":\"%s\",\"inductance_nh\":%.17g,\"resistance_hf\":%.17g}",index?",":"",shape->filaments.prefix,l,r);
    }
    sprintf(out,"]}");return result;
}
const char* last_warnings() { return warnings; }
// p: length,height,width,spacing,turns,radius,sides,metal,exit,angle,inner,secondary_turns
const char* generate(int kind,const double* p) {
    clear();
    for(int i=0;i<12;++i)if(!__builtin_isfinite(p[i]))return "{\"error\":\"Parameters must be finite\"}";
    if(p[7]<0 || p[7]>=metal_count || p[8]< -1 || p[8]>=metal_count)return "{\"error\":\"Invalid metal selection\"}";
    if(!__builtin_isfinite(origin_x) || !__builtin_isfinite(origin_y))return "{\"error\":\"Origins must be finite\"}";
    const int metal=static_cast<int>(p[7]),exit=static_cast<int>(p[8]);
    if(p[7]!=metal || p[8]!=exit)return "{\"error\":\"Metal indices must be integers\"}";
    if(metal<0 || metal>=metal_count || exit < -1 || exit>=metal_count || p[2]<=0 || p[3]<0 || p[4]<0.25 || p[4]>64 || p[11]<=0 || p[11]>64 || p[6]<3 || p[6]>256 || kind<0 || kind>9)
        return "{\"error\":\"Invalid dimensions, turns, sides or metal selection\"}";
    if(p[0]<=2*p[2] || p[1]<=2*p[2] || p[5]<=p[2] || p[10]<=0)return "{\"error\":\"Dimensions are too small\"}";
    if(kind==5 && p[3]>=360)return "{\"error\":\"Ring gap must be below 360 degrees\"}";
    if((kind==0 || kind==6 || kind==8) && p[4]*4!=static_cast<int>(p[4]*4))return "{\"error\":\"Square turns must be multiples of one quarter\"}";
    if((kind==3 || kind==4) && p[4]<0.5)return "{\"error\":\"Symmetric windings need at least half a turn\"}";
    if(kind==8 && (exit<0 || metal<=exit))return "{\"error\":\"Multi-metal square needs a lower exit metal\"}";
    if((kind==0 || kind==3 || kind==4 || kind==6 || kind==7) && exit==metal)return "{\"error\":\"Exit or second metal must differ from primary metal\"}";
    if((kind==3 || kind==4 || kind==6 || kind==7) && exit<0)return "{\"error\":\"This winding needs a second metal\"}";
    if(kind==0 || kind==1 || kind==8) {
        const double available=kind==1?p[5]:((p[0]<p[1]?p[0]:p[1])*0.5);
        if(p[4]*(p[2]+p[3])+p[2]>available)return "{\"error\":\"Turns do not fit the available winding area\"}";
    }
    GeometryShape a{};integer(a,0x50)=kind==1?1:kind==2?2:kind==3?16:kind==4?17:kind==5?22:kind==8?18:0;
    a.filaments.prefix[0]='P';a.attributes=static_cast<unsigned>(winding_phase);number(a,0x54)=origin_x;number(a,0x5c)=origin_y;integer(a,0x64)=static_cast<int>(p[6]);integer(a,0x68)=metal;integer(a,0x6c)=exit;
    number(a,0x70)=p[0];number(a,0x78)=(kind==3 || kind==4)?p[10]:p[1];number(a,0x80)=p[5];
    number(a,0x88)=p[2];number(a,0x90)=kind==5?(has_parsed_gap?parsed_ring_gap:x87::binary64(Real(p[3])*Real(0x1.1df46a2529d39p-6))):p[3];number(a,0x98)=p[4];number(a,0xa0)=p[9];
    GeometryTechnology tech{metals,vias,heights,metal_count};
    SpiralConstructionRuntime runtime{geometry,via_count,{warn,nullptr},sprintf,message,invalid,nullptr};
    SquareConstructionHistory square{{-1,-1},{}};
    switch(kind) {
    case 0:build_square_spiral(head,a,3,tech,runtime,square);break;
    case 1:build_polygon_spiral(head,a,tech,runtime,{-1,-1});break;
    case 2:build_wire_shape(head,a,tech,geometry,{-1,-1});break;
    case 3:build_symmetric_square(head,a,tech,runtime,{{-1,-1},metal,nullptr});break;
    case 4:build_symmetric_polygon(head,a,tech,runtime,{{-1,-1},0,nullptr});break;
    case 5:build_ring_shape(head,a,tech,runtime,{-1,-1});break;
    case 6:case 7: {
        GeometryShape b=a;b.filaments.prefix[0]='S';number(b,0x98)=p[11];
        if(kind==6)number(a,0x90)=number(b,0x90)=x87::binary64(Real(p[3])*2+Real(p[2]));
        if(kind==6)build_planar_transformer(head,a,b,tech,runtime,{{square,square}});
        else { integer(b,0x68)=exit;build_3d_transformer(head,a,b,tech,runtime,{{{-1,-1},{-1,-1}},nullptr}); }
        break;
    }
    case 8:build_multimetal_square(head,a,tech,{runtime,nullptr,geometry.memory,random,nullptr},square);break;
    case 9:integer(a,0x50)=14;integer(a,0x64)=2;integer(a,0x68)=metal_count;integer(a,0x6c)=2;
        if(!via_count)return "{\"error\":\"Technology has no via layers\"}";
        build_via_shape(head,a,tech,geometry,{});break;
    }
    char* out=result;out+=sprintf(out,"{\"shapes\":[");int index=0;
    for(auto* shape=head;shape;shape=shape->next,++index) {
        const double resistance=x87::binary64(shape_resistance_dc(shape->filaments,{metals,vias,metal_count}));
        out+=sprintf(out,"%s{\"name\":\"%s\",\"resistance_dc\":%.17g,\"origin\":[%.17g,%.17g],\"segments\":[",index?",":"",shape->filaments.prefix,resistance,number(*shape,0x54),number(*shape,0x5c));
        int count=0;
        for(auto* s=shape->filaments.first;s;s=reinterpret_cast<const ConductorSegment*>(s->next_address),++count) {
            if(out-result>static_cast<int>(sizeof(result))-2048)return "{\"error\":\"Geometry exceeds export buffer\"}";
            out+=sprintf(out,"%s{\"metal\":%d,\"width\":%.17g,\"thickness\":%.17g,\"nx\":%d,\"ny\":%d,\"points\":[",count?",":"",s->metal,s->width,s->metal<metal_count?s->thickness:0.,s->width_count,s->thickness_count);
            const double* points[]={s->endpoints,s->endpoints+3,s->outer_start,s->outer_end,s->inner_start,s->inner_end};
            for(int j=0;j<6;++j)out+=sprintf(out,"%s[%.17g,%.17g,%.17g]",j?",":"",points[j][0],points[j][1],points[j][2]);
            out+=sprintf(out,"]}");
        }
        out+=sprintf(out,"]}");
    }
    sprintf(out,"]}");return result;
}
}
