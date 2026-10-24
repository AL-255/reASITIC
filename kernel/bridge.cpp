#include "engine.hpp"
#include "json.hpp"
#include "asitic/console_geometry.hpp"
#ifdef __EMSCRIPTEN__
#include <setjmp.h>
#else
// The native harness needs only i386 libc, without development headers.
// glibc i386 jmp_buf occupies 156 bytes; keep aligned backing with room.
using jmp_buf=int[64];
extern "C" int setjmp(int*) __attribute__((returns_twice));
extern "C" [[noreturn]] void longjmp(int*,int);
#endif
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
extern "C" int sscanf(const char*,const char*,...);
extern "C" int vsnprintf(char*,unsigned,const char*,__builtin_va_list);
extern "C" char* strtok(char*,const char*);
extern "C" double strtod(const char*,char**);
namespace asitic { bool create_native_geometry(int,CommandArguments&,ConsoleGeometryState&,bool&); }
extern "C" void abort();
using namespace asitic;
namespace {
GeometryShape* head=nullptr;
MetalLayer metals[32]{};ViaLayer vias[32]{};double heights[32]{};int metal_count=0,via_count=0;
double substrate[32]{};int substrate_count=0;
double origin_x=0,origin_y=0,parsed_ring_gap=0;int winding_phase=1;bool has_parsed_gap=false;
char result[4*1024*1024],message[1024],warnings[4096];unsigned warning_length=0;
jmp_buf creator_jump;bool creator_active=false;
struct alignas(16) Allocation { Allocation* previous;Allocation* next;bool tracked; };
Allocation* pending=nullptr;unsigned pending_bytes=0,pending_count=0;
void fail_creator(const char*);
void* allocate(unsigned n,void*) {
    if(creator_active && (n>32*1024*1024 || pending_bytes>32*1024*1024-n || pending_count>=65536))fail_creator("Geometry exceeds the creation memory budget");
    auto* a=static_cast<Allocation*>(calloc(1,sizeof(Allocation)+n+16)); // native MMSQUARE workspace padding
    if(!a) { if(creator_active)fail_creator("Unable to allocate geometry");abort(); }
    if(creator_active) { a->tracked=true;a->next=pending;if(pending)pending->previous=a;pending=a;pending_bytes+=n;++pending_count; }
    return a+1;
}
void release(void* p,void*) {
    if(!p)return;auto* a=static_cast<Allocation*>(p)-1;
    if(a->tracked) { if(a->previous)a->previous->next=a->next;else pending=a->next;if(a->next)a->next->previous=a->previous; }
    free(a);
}
void warn(const char* s,void*) { if(warning_length>=sizeof(warnings)-2)return;while(*s && warning_length<sizeof(warnings)-2)warnings[warning_length++]=*s++;warnings[warning_length++]='\n';warnings[warning_length]=0; }
void fail_creator(const char* text) { warn(text,nullptr);longjmp(creator_jump,1); }
void terminate(int,void*) { if(creator_active)fail_creator("Original creator terminated");abort(); }
void invalid(void*) { if(creator_active)fail_creator("Original creator rejected these arguments");abort(); }
int random(void*) { return 65; }
double& number(GeometryShape& s,unsigned offset) { return *reinterpret_cast<double*>(s.filaments.prefix+offset); }
int& integer(GeometryShape& s,unsigned offset) { return *reinterpret_cast<int*>(s.filaments.prefix+offset); }
GeometryRuntime geometry{{{allocate,nullptr},release},{warn,nullptr},terminate,nullptr};
void clear() { reasitic::release_simulation_result();while(head) { auto* next=head->next;head->next=nullptr;if(head->filaments.first)release_geometry_shapes(head,geometry.memory);else release(head,nullptr);head=next; }warning_length=0;warnings[0]=0; }
// C locale tables use the recovered glibc signed-char table layout.
unsigned short classes[384];int upper[384],lower[384];
CharacterTables character_tables() {
    for(int i=-128;i<256;++i) {
        int c=i<0?i+256:i;upper[i+128]=c>='a' && c<='z'?c-32:c;lower[i+128]=c>='A' && c<='Z'?c+32:c;
        classes[i+128]=(c>='0' && c<='9'?0x800:0)|((c==32 || (c>=9 && c<=13))?0x2000:0)|((c<32 || c==127)?2:0);
    }
    upper[127]=lower[127]=-1;return {classes+128,upper+128,lower+128};
}
int bounded_format(char* out,const char* format,...) {
    __builtin_va_list args;__builtin_va_start(args,format);const int n=vsnprintf(out,256,format,args);__builtin_va_end(args);return n;
}
const char* answers=nullptr;
void read_answer(const char* prompt,char* out,void*) {
    if(!answers || !*answers) { warn(prompt,nullptr);fail_creator("Supply another line of interactive input"); }
    unsigned n=0;while(*answers && *answers!='\n') { if(n>=70)fail_creator("Interactive lines must fit 70 bytes");out[n++]=*answers++; }
    if(*answers=='\n')++answers;out[n]=0;
    // Reject nonfinite numeric prompt values before native loops and casts.
    const char* at=out;while(*at) { char* end;double value=strtod(at,&end);if(end==at)break;
        if(!__builtin_isfinite(value) || value>1000000 || value< -1000000)fail_creator("Interactive numeric input is outside the finite supported range");at=end; }
}
ConsoleGeometryState creator_state;CommandArguments creator_arguments;
GeometryShape* original_head=nullptr;
bool safe_keywords(const char* line) {
    unsigned total=0;while(line[total])if(++total>255)return false;
    if(!total)return false;
    // Guard native fixed name buffers and count loops; do not round values here.
    char copy[256];__builtin_memcpy(copy,line,total+1);bool token_found=false;
    for(char* token=strtok(copy,":");token;token=strtok(nullptr,":")) {
        token_found=true;char* value=token;while(*value && *value!='=')++value;
        if(!*value)return false;*value++=0;if(!*value)return false;
        for(char* p=token;*p;++p)if(*p>='a' && *p<='z')*p-=32;
        bool name=__builtin_strcmp(token,"NAME")==0 || __builtin_strcmp(token,"PNAME")==0 || __builtin_strcmp(token,"SNAME")==0;
        if(name) { unsigned n=0;while(value[n])++n;if(n>74 || !n)return false;continue; }
        // Names of technology layers are resolved by the original parser.
        if(__builtin_strncmp(token,"METAL",5)==0 || __builtin_strcmp(token,"EXIT")==0 || __builtin_strcmp(token,"VIA")==0)continue;
        char* end;const double n=strtod(value,&end);if(end==value || !__builtin_isfinite(n) || n>1000000 || n< -1000000)return false;
        const bool turns=__builtin_strcmp(token,"N")==0 || __builtin_strcmp(token,"NP")==0 || __builtin_strcmp(token,"NS")==0;
        if(turns && (n<=0 || n>64))return false;
        if(__builtin_strcmp(token,"SIDES")==0 && (n<3 || n>256 || n!=static_cast<int>(n)))return false;
        if((__builtin_strcmp(token,"NX")==0 || __builtin_strcmp(token,"NY")==0) && (n<1 || n>256 || n!=static_cast<int>(n)))return false;
        if((__builtin_strcmp(token,"W")==0 || __builtin_strcmp(token,"W1")==0 || __builtin_strcmp(token,"W2")==0) && n<=0)return false;
        if(__builtin_strcmp(token,"S")==0 && n<0)return false;
    }
    return token_found;
}

const char* serialize_geometry() {
    char* out=result;out+=sprintf(out,"{\"shapes\":[");int index=0;
    for(auto* shape=head;shape;shape=shape->next,++index) {
        const double resistance=x87::binary64(shape_resistance_dc(shape->filaments,{metals,vias,metal_count}));
        out+=sprintf(out,"%s{\"name\":",index?",":"");out=reasitic::json_string(out,reinterpret_cast<const char*>(shape->filaments.prefix));
        out+=sprintf(out,",\"resistance_dc\":%.17g,\"origin\":[%.17g,%.17g],\"segments\":[",resistance,number(*shape,0x54),number(*shape,0x5c));
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
namespace asitic {
void guard_native_creation(int id,const ConsoleGeometryState& state) {
    const auto& a=state.arguments[0];auto& s=const_cast<GeometryShape&>(a);
    const unsigned offsets[]={0x54u,0x5cu,0x70u,0x78u,0x80u,0x88u,0x90u,0x98u,0xa0u};
    for(unsigned off:offsets)
        if(!__builtin_isfinite(number(s,off)))fail_creator("Original descriptor is nonfinite");
    if(number(s,0x88)<=0)fail_creator("Conductor width must be positive");
    if(id==14) {
        if(integer(s,0x64)<1 || integer(s,0x64)>256 || integer(s,0x6c)<1 || integer(s,0x6c)>256)fail_creator("Via counts must be between 1 and 256");
        if(integer(s,0x68)<metal_count || integer(s,0x68)>=metal_count+via_count)fail_creator("Invalid via layer");return;
    }
    if(integer(s,0x68)<0 || integer(s,0x68)>=metal_count)fail_creator("Invalid primary metal");
    if(id==18 && (integer(s,0x6c)<0 || integer(s,0x6c)>=integer(s,0x68)))fail_creator("Multi-metal square requires a lower exit metal");
    if(id==10) { auto& b=const_cast<GeometryShape&>(state.arguments[1]);if(integer(b,0x68)<0 || integer(b,0x68)>=metal_count)fail_creator("Invalid secondary metal"); }
    if(id!=2 && id!=4 && id!=22) {
        const double n=number(s,0x98);if(n<=0 || n>64 || integer(s,0x64)<3 || integer(s,0x64)>256 || n*integer(s,0x64)<1)fail_creator("Turns or sides exceed the supported creation range");
        if(number(s,0x90)<0)fail_creator("Spacing must be nonnegative");
    }
}
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
    sprintf(reinterpret_cast<char*>(&metals[i]),"M%d",i+1);if(i>=metal_count)metal_count=i+1;
}
void set_via(int i,int bottom,int top,double width,double spacing,double over1,double over2,double resistance) {
    if(i<0 || i>=32)return;auto* p=reinterpret_cast<unsigned char*>(&vias[i]);
    *reinterpret_cast<double*>(p+0xa0)=width;*reinterpret_cast<double*>(p+0xa8)=spacing;
    *reinterpret_cast<double*>(p+0xb0)=over1;*reinterpret_cast<double*>(p+0xb8)=over2;
    *reinterpret_cast<double*>(p+0xc0)=resistance;*reinterpret_cast<int*>(p+0xcc)=bottom;*reinterpret_cast<int*>(p+0xd0)=top;
    sprintf(reinterpret_cast<char*>(&vias[i]),"VIA%d",i+1);if(i>=via_count)via_count=i+1;
}
void set_layer_name(int via,int i,const char* name) {
    if(i<0 || i>=(via?via_count:metal_count) || !name)return;
    auto* out=via?reinterpret_cast<char*>(&vias[i]):reinterpret_cast<char*>(&metals[i]);
    unsigned n=0;while(n<79 && name[n]) { out[n]=name[n];++n; }out[n]=0;
}
const char* create_geometry(int id,const char* keywords,const char* input,double chip_x,double chip_y,int cells_x,int cells_y,double grid,int snap,int append) {
    if(id!=0 && id!=1 && id!=2 && id!=3 && id!=4 && id!=5 && id!=10 && id!=14 && id!=16 && id!=17 && id!=18 && id!=22)return "{\"error\":\"Unknown original creator\"}";
    if(!metal_count || (id==14 && !via_count))return "{\"error\":\"Technology lacks required layers\"}";
    if(!__builtin_isfinite(chip_x) || !__builtin_isfinite(chip_y) || chip_x<=0 || chip_y<=0 || cells_x<=0 || cells_y<=0 || !__builtin_isfinite(grid) || grid<=0)return "{\"error\":\"Invalid chip or grid settings\"}";
    if(keywords && *keywords && !safe_keywords(keywords))return "{\"error\":\"Invalid or oversized creator arguments\"}";
    if(!append)clear();else { reasitic::release_simulation_result();reasitic::clear_diagnostics(); }
    original_head=head;creator_state={};creator_arguments={};answers=input;
    auto& state=creator_state;state.head=head;state.geometry=geometry;
    state.technology={metals,vias,heights,metal_count};
    CommandRuntime parser{character_tables(),sscanf,strtok,bounded_format,message,{warn,nullptr},head,nullptr,1e9,1e-6,grid,snap!=0};
    state.prompt={parser,state.technology,via_count,chip_x,chip_y,invalid,nullptr};
    state.creation={state.prompt,read_answer,nullptr,cells_x,cells_y};
    state.commands={{geometry,via_count,{warn,nullptr},bounded_format,message,invalid,nullptr},nullptr,geometry.memory,random,nullptr};
    // Unwritten native mesh words have no process-independent value. Start at
    // zero, as in original pre-analysis SAVE fixtures, until the solver meshes.
    state.invalid=invalid;pending=nullptr;pending_bytes=pending_count=0;
    creator_arguments.command.value=keywords && *keywords?1:0;
    if(creator_arguments.command.value)__builtin_strcpy(creator_arguments.arguments[0].text,keywords);
    creator_active=true;
    if(setjmp(creator_jump)) {
        creator_active=false;while(pending) { auto* next=pending->next;free(pending);pending=next; }head=original_head;
        char* out=result;out+=sprintf(out,"{\"error\":");out=reasitic::json_string(out,warnings);sprintf(out,"}");return result;
    }
    bool suppressed=false;create_native_geometry(id,creator_arguments,state,suppressed);
    head=state.head;creator_active=false;
    while(pending) { pending->tracked=false;pending=pending->next; }
    if(head==original_head)return "{\"error\":\"Original creator did not produce geometry\"}";
    return serialize_geometry();
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
        out+=sprintf(out,"%s{\"name\":",index?",":"");out=reasitic::json_string(out,reinterpret_cast<const char*>(shape->filaments.prefix));out+=sprintf(out,",\"inductance_nh\":%.17g,\"resistance_hf\":%.17g}",l,r);
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
    return serialize_geometry();
}
}
