#include "engine.hpp"
#include "filament_policy.hpp"
#include "eddy_runtime.hpp"
#include "json.hpp"
#include "asitic/shape_construction.hpp"
#include "asitic/numerics.hpp"
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
char* simulation_result=nullptr;
struct Allocation { void* pointer;Allocation* next; };
struct Workspace {
    Allocation* allocations=nullptr;
    ~Workspace() { while(allocations) { auto* p=allocations;allocations=p->next;free(p->pointer);free(p); } }
    static void* get(unsigned bytes,void* user) {
        auto& work=*static_cast<Workspace*>(user);
        auto* pointer=calloc(1,bytes+16);auto* node=static_cast<Allocation*>(calloc(1,sizeof(Allocation)));
        if(!pointer || !node)abort();
        node->pointer=pointer;node->next=work.allocations;work.allocations=node;return pointer;
    }
    static void put(void* pointer,void* user) {
        auto& work=*static_cast<Workspace*>(user);auto** link=&work.allocations;
        while(*link && (*link)->pointer!=pointer)link=&(*link)->next;
        if(*link) { auto* node=*link;*link=node->next;free(node->pointer);free(node); }
    }
    SolverMemory memory() { return {{get,this},put}; }
};
const ConductorSegment* next(const ConductorSegment* s) { return reinterpret_cast<const ConductorSegment*>(s->next_address); }
Real length(const ConductorSegment& s) {
    const Real x=Real(s.endpoints[3])-s.endpoints[0],y=Real(s.endpoints[4])-s.endpoints[1],z=Real(s.endpoints[5])-s.endpoints[2];
    return x87::sqrt((x*x+y*y)+z*z);
}
void terminate(int,void*) { abort(); }
// Bound cloning before entering the original allocation/loop sequence. The
// mathematical subdivision and cross-section counts are still produced by the
// original routines; no automatic mesh coarsening is performed.
bool bounded_clone(const GeometryShape& shape,double cell,double trigger,int maximum) {
    const double limit=trigger>cell?cell:trigger;int total=0;
    for(auto* s=shape.filaments.first;s;s=next(s)) {
        double largest=x87::binary64(length(*s));
        for(auto* b=reinterpret_cast<const ConductorSegment*>(s->group_next_address);b;b=reinterpret_cast<const ConductorSegment*>(b->group_next_address)) {
            const Real size=length(*b);if(size>largest)largest=x87::binary64(size);
        }
        int pieces=1;
        if(limit<largest) {
            const Real ratio=Real(largest)/cell;if(ratio>maximum)return false;
            pieces=x87::truncate_int(ratio);if(Real(pieces)<ratio)++pieces;
        }
        total+=pieces;if(total>maximum)return false;
    }
    return true;
}
}
namespace reasitic {
bool bounded_solver_clone(const GeometryShape& shape,double cell,double trigger,int maximum) { return bounded_clone(shape,cell,trigger,maximum); }
void release_simulation_result() { free(simulation_result);simulation_result=nullptr; }
}
static const char* simulate_impl(int winding,double frequency,double length_cell,double width_cell,
                                 double thickness_cell,double subdivision_trigger,int filament_limit,reasitic::EddySession* eddy) {
    reasitic::release_simulation_result();reasitic::clear_diagnostics();
    const auto engine=reasitic::engine_view();
    if(winding<0 || !__builtin_isfinite(frequency) || frequency<=0 ||
       !__builtin_isfinite(length_cell) || length_cell<=0 || !__builtin_isfinite(width_cell) || width_cell<=0 ||
       !__builtin_isfinite(thickness_cell) || thickness_cell<=0 || !__builtin_isfinite(subdivision_trigger) || subdivision_trigger<=0 ||
       filament_limit<1 || filament_limit>1024)
        return "{\"error\":\"Use a positive frequency and mesh cells, and a filament limit from 1 to 1024\"}";
    auto* source=engine.shapes;for(int i=0;source && i<winding;++i)source=source->next;
    if(!source)return "{\"error\":\"Generate a layout and select a valid winding\"}";
    if(!bounded_clone(*source,length_cell,subdivision_trigger,filament_limit))
        return "{\"error\":\"Longitudinal mesh exceeds the selected filament limit; increase the length cell or the limit\"}";
    Workspace work;const auto memory=work.memory();
    GeometryRuntime runtime{memory,engine.warnings,terminate,nullptr};GeometryShape* clones=nullptr;
    auto* shape=clone_geometry_shape(clones,*source,{length_cell,subdivision_trigger},runtime);
    // Generated conductors carry explicit incoming mesh history. Zero selects
    // native cell-derived counts; -1/-1 would force the original 3x3 override.
    // Via NX/NY remain physical cut counts and must never be changed here.
    int conductors=0;
    for(auto* s=const_cast<ConductorSegment*>(shape->filaments.first);s;s=const_cast<ConductorSegment*>(next(s)))if(s->metal<engine.metal_count) {
        s->width_count=s->thickness_count=0;++conductors;
        for(auto* b=reinterpret_cast<ConductorSegment*>(s->group_next_address);b;b=reinterpret_cast<ConductorSegment*>(b->group_next_address))b->width_count=b->thickness_count=0;
        // Prevent the native integer count conversion from overflowing before
        // the original context can publish its exact total.
        if(Real(s->thickness)/thickness_cell+Real(.5)>filament_limit)
            return "{\"error\":\"Thickness mesh exceeds the selected filament limit\"}";
        if(Real(s->width)/width_cell+Real(.5)>2147483647.L)
            return "{\"error\":\"Width mesh exceeds the native integer range\"}";
    }
    if(!conductors)return "{\"error\":\"LMAT requires at least one metal conductor\"}";
    InductanceContext context{};context.shape=&shape->filaments;
    if(!setup_filament_groups(context,engine.metal_count,memory.arrays,engine.warnings) ||
       !index_conductor_segments(context,engine.metal_count,memory.arrays,engine.warnings) ||
       !set_filament_counts(context,width_cell,thickness_cell,engine.max_width_cells,memory.arrays,engine.warnings))
        return "{\"error\":\"Could not prepare the original filament context\"}";
    if(context.filament_count>filament_limit)
        return "{\"error\":\"Cross-section mesh exceeds the selected filament limit; increase width/thickness cells or the limit\"}";
    if(!reasitic::valid_width_grading<Real>(context,width_cell,engine.max_width_cells))return "{\"error\":\"MAXNW edge grading produces a nonfinite width; use at least three width cells or a larger width cell size\"}";
    ComplexMatrix matrix{};InductanceRunOptions options{{width_cell,engine.max_width_cells,engine.metals},thickness_cell,engine.metal_count,engine.vias,true};
    const bool solved=eddy?solve_inductance(shape->filaments,matrix,frequency,options,reasitic::eddy_options(*eddy,engine),memory,engine.warnings):solve_inductance_without_eddy(shape->filaments,matrix,frequency,options,memory,engine.warnings);
    if(eddy && eddy->exceeded)return "{\"error\":\"Eddy evaluation budget exceeded; reduce the mesh size or increase the Green budget\"}";
    if(!solved)
        return "{\"error\":\"Original filament solver could not produce an impedance matrix\"}";
    double real=0,imag=0;
    for(int i=0;i<matrix.rows;++i)for(int j=0;j<matrix.columns;++j) {
        const auto& z=matrix.view().at(i,j);
        if(!__builtin_isfinite(z.real) || !__builtin_isfinite(z.imag))return "{\"error\":\"Original filament solver produced a nonfinite matrix\"}";
        real=x87::binary64(Real(real)+z.real);imag=x87::binary64(Real(imag)+z.imag);
    }
    const Real omega=Real(0x1.921fb54442d18p+2L)*frequency;
    const double inductance=x87::binary64(Real(imag)/omega);
    if(!__builtin_isfinite(inductance) || !__builtin_isfinite(real) || !__builtin_isfinite(imag))
        return "{\"error\":\"Original matrix reduction produced nonfinite totals\"}";
    const unsigned capacity=64u*matrix.rows*matrix.columns+16384u;
    simulation_result=static_cast<char*>(calloc(1,capacity));if(!simulation_result)return "{\"error\":\"Could not allocate matrix output\"}";
    char* out=simulation_result;
    out+=sprintf(out,"{\"name\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(shape->filaments.prefix));
    if(engine.max_width_cells!=500)out+=sprintf(out,",\"max_width_cells\":%d",engine.max_width_cells);
    out+=sprintf(out,",\"frequency_hz\":%.17g,\"conductors\":%d,\"filaments\":%d,\"resistance\":%.17g,\"reactance\":%.17g,\"inductance_h\":%.17g,\"series_q\":",frequency,context.group_count,context.filament_count,real,imag,inductance);
    const double q=x87::binary64(Real(imag)/real);
    out+=__builtin_isfinite(q)?sprintf(out,"%.17g",q):sprintf(out,"null");
    if(eddy){out+=sprintf(out,",\"eddy\":");out=reasitic::eddy_metadata(out,*eddy);}
    out+=sprintf(out,",\"mesh\":{\"length\":%.17g,\"width\":%.17g,\"thickness\":%.17g,\"subdivision_trigger\":%.17g},\"matrix\":[",length_cell,width_cell,thickness_cell,subdivision_trigger);
    for(int i=0;i<matrix.rows;++i) {
        out+=sprintf(out,"%s[",i?",":"");
        for(int j=0;j<matrix.columns;++j) { const auto& z=matrix.view().at(i,j);out+=sprintf(out,"%s[%.17g,%.17g]",j?",":"",z.real,z.imag); }
        out+=sprintf(out,"]");
    }
    sprintf(out,"]}");return simulation_result;
}

extern "C" const char* simulate(int winding,double frequency,double length_cell,double width_cell,double thickness_cell,double subdivision_trigger,int filament_limit) {
    return simulate_impl(winding,frequency,length_cell,width_cell,thickness_cell,subdivision_trigger,filament_limit,nullptr);
}
extern "C" const char* simulate_eddy(int winding,double frequency,double length_cell,double width_cell,double thickness_cell,double subdivision_trigger,int filament_limit,int mode,int count,int first,int second,int budget) {
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate Eddy workspace\"}";
    reasitic::EddyScope scope(session);reasitic::clear_diagnostics();
    if(!reasitic::configure_eddy(*session,reasitic::engine_view(),mode,count,first,second,budget))return "{\"error\":\"Invalid eddy mode, layers or evaluation budget\"}";
    return simulate_impl(winding,frequency,length_cell,width_cell,thickness_cell,subdivision_trigger,filament_limit,session);
}
