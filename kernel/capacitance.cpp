#include "engine.hpp"
#include "json.hpp"
#include "asitic/potential_solver.hpp"
#include "asitic/legacy_complex.hpp"
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
char* output=nullptr;double total_capacitance=0,total_resistance=0;
struct Allocation { void* pointer;Allocation* next; };
struct Workspace {
    Allocation* allocations=nullptr;int nx,ny;
    Workspace(int x,int y):nx(x),ny(y) {}
    ~Workspace() { while(allocations) { auto* p=allocations;allocations=p->next;free(p->pointer);free(p); } }
    static void* get(unsigned n,void* user) {
        auto& w=*static_cast<Workspace*>(user);unsigned backing=n;
        // Keep the original nonsquare grid strides and scratch request. Supply
        // backing for every original access, including native over-allocation
        // requirements documented in potential_tables.hpp.
        const unsigned largest=w.nx>w.ny?w.nx:w.ny;
        if(n==static_cast<unsigned>((w.nx+1)*(w.ny+1))*16u)backing=(largest+1)*(largest+1)*16u;
        if(n==static_cast<unsigned>(w.nx)*32u)backing=largest*32u;
        auto* p=calloc(1,backing+16);auto* node=static_cast<Allocation*>(calloc(1,sizeof(Allocation)));
        if(!p || !node)abort();node->pointer=p;node->next=w.allocations;w.allocations=node;return p;
    }
    static void put(void* p,void* user) {
        auto& w=*static_cast<Workspace*>(user);auto** link=&w.allocations;
        while(*link && (*link)->pointer!=p)link=&(*link)->next;
        if(*link) { auto* node=*link;*link=node->next;free(node->pointer);free(node); }
    }
    SolverMemory memory(){return {{get,this},put};}
};
const ConductorSegment* next(const ConductorSegment* s) { return reinterpret_cast<const ConductorSegment*>(s->next_address); }
Real segment_length(const ConductorSegment& s) {
    const Real x=Real(s.endpoints[3])-s.endpoints[0],y=Real(s.endpoints[4])-s.endpoints[1],z=Real(s.endpoints[5])-s.endpoints[2];
    return x87::sqrt((x*x+y*y)+z*z);
}
bool same_name(const GeometryShape& a,const GeometryShape& b) { const char* p=reinterpret_cast<const char*>(&a);const char* q=reinterpret_cast<const char*>(&b);while(*p && *p==*q){++p;++q;}return *p==*q; }
GeometryShape* clone_for_ground(const GeometryShape& source,SolverMemory memory) {
    auto* shape=static_cast<GeometryShape*>(memory.arrays.get(sizeof(GeometryShape)));*shape=source;shape->next=nullptr;
    shape->filaments.first=nullptr;ConductorSegment* tail=nullptr;
    for(auto* s=source.filaments.first;s;s=next(s)) {
        auto* copy=static_cast<ConductorSegment*>(memory.arrays.get(sizeof(ConductorSegment)));*copy=*s;if(tail)tail->next_address=reinterpret_cast<unsigned>(copy);else shape->filaments.first=copy;
        copy->width_count=copy->thickness_count=-1;copy->group_next_address=0;
        auto* parent=copy;
        for(auto* child=reinterpret_cast<const ConductorSegment*>(s->group_next_address);child;child=reinterpret_cast<const ConductorSegment*>(child->group_next_address)) {
            auto* duplicate=static_cast<ConductorSegment*>(memory.arrays.get(sizeof(ConductorSegment)));*duplicate=*child;duplicate->group_next_address=0;parent->group_next_address=reinterpret_cast<unsigned>(duplicate);parent=duplicate;
        }
        copy->next_address=0;tail=copy;
    }
    return shape;
}
struct Snapshots {
    SolverMemory memory;ComplexMatrix potential{};int contacts=0;
    static void record(const char* name,const ComplexMatrix& matrix,void* user) {
        auto& s=*static_cast<Snapshots*>(user);
        if(name[0]!='P')return;
        s.contacts=matrix.rows;s.potential=matrix;s.potential.vector_reference=s.potential.matrix_reference=0;
        s.potential.data=static_cast<ComplexValue*>(s.memory.arrays.get(matrix.rows*matrix.columns*16u));s.potential.stride=matrix.rows;
        for(int j=0;j<matrix.columns;++j)for(int i=0;i<matrix.rows;++i)s.potential.view().at(i,j)=matrix.view().at(i,j);
    }
};
// The original file cache is disabled, so these callbacks are never consulted.
void* no_open(const char*,const char*,void*) { return nullptr; }
unsigned no_read(void*,unsigned,unsigned,void*,void*) { return 0; }
unsigned no_write(const void*,unsigned,unsigned,void*,void*) { return 0; }
int no_close(void*,void*) { return 0; }
// Run the native count selection before native contact creation. Budget
// rejection does not change counts, geometry, FFT resolution or cell sizes.
bool admissible(const GeometryShape& shape,const PotentialContactOptions& options,int limit,int& count,int& groups,SolverMemory memory,WarningSink warnings) {
    int conductors=0;
    for(auto* s=shape.filaments.first;s;s=next(s)) {
        if(s->metal>=options.metal_count)continue;++conductors;
        for(auto* segment=s;segment;segment=reinterpret_cast<const ConductorSegment*>(segment->group_next_address)) {
            const Real length=segment_length(*segment);
            if(segment->metal<0 || segment->metal>=options.metal_count || !(length>0) || !(segment->width>0) ||
               length/options.length_step>65536 || Real(segment->width)/options.width_step>65536)return false;
            PotentialContactGroup* head=nullptr;PotentialContactGroup* current=nullptr;
            const bool ok=prepare_potential_contact_group(*segment,shape.filaments,current,false,head,options,memory,warnings);
            if(!ok)return false;
            const int n=current->length_cells*current->width_cells;memory.put(head);
            if(n<1 || n>limit-count)return false;count+=n;
        }
        ++groups;
    }
    return conductors>0;
}
unsigned long long table_hash(const PotentialCache& cache,int nx,int ny,int count) {
    unsigned long long hash=14695981039346656037ULL;
    for(int pair=0;pair<count;++pair)for(int bank=0;bank<3;++bank) {
        const auto* data=reinterpret_cast<const unsigned char*>(bank==0?cache.xy[pair]:bank==1?cache.x[pair]:cache.y[pair]);
        const unsigned bytes=16u*(bank==0?(nx+1)*(ny+1):bank==1?2*nx:2*ny);
        for(unsigned i=0;i<bytes;++i){hash^=data[i];hash*=1099511628211ULL;}
    }
    return hash;
}
char* matrix_json(char* out,const ComplexMatrix& matrix) {
    out+=sprintf(out,"[");
    for(int i=0;i<matrix.rows;++i) { out+=sprintf(out,"%s[",i?",":"");for(int j=0;j<matrix.columns;++j) { const auto z=matrix.view().at(i,j);out+=sprintf(out,"%s[%.17g,%.17g]",j?",":"",z.real,z.imag); }out+=sprintf(out,"]"); }
    out+=sprintf(out,"]");return out;
}
bool finite_matrix(const ComplexMatrix& m) { for(int j=0;j<m.columns;++j)for(int i=0;i<m.rows;++i){auto z=m.view().at(i,j);if(!__builtin_isfinite(z.real) || !__builtin_isfinite(z.imag))return false;}return true; }
}
namespace asitic {
// Observation of the original rounded totals immediately before native CAP's
// formatted output. No expression or arithmetic in the solve is replaced.
void record_capacitance_totals(double capacitance,double resistance) { total_capacitance=capacitance;total_resistance=resistance; }
}
namespace reasitic { void release_capacitance_result(){free(output);output=nullptr;} }
extern "C" const char* simulate_capacitance(int winding,int ground,double frequency,double length_cell,double width_cell,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int unfolded) {
    reasitic::release_capacitance_result();reasitic::clear_diagnostics();const auto engine=reasitic::engine_view();
    if(winding<0 || ground< -1 || !__builtin_isfinite(frequency) || frequency<=0 ||
       !__builtin_isfinite(length_cell) || length_cell<=0 || !__builtin_isfinite(width_cell) || width_cell<=0 ||
       !__builtin_isfinite(chip_x) || chip_x<=0 || !__builtin_isfinite(chip_y) || chip_y<=0 ||
       nx<1 || nx>256 || ny<1 || ny>256 || (nx&(nx-1)) || (ny&(ny-1)) || contact_limit<1 || contact_limit>512)
        return "{\"error\":\"Use positive frequency, cells and chip dimensions, power-of-two FFT sizes from 1 to 256, and 1–512 contacts\"}";
    auto* source=engine.shapes;for(int i=0;source && i<winding;++i)source=source->next;
    auto* reference=engine.shapes;for(int i=0;reference && i<ground;++i)reference=reference->next;if(ground<0)reference=source;
    if(!source || !reference)return "{\"error\":\"Select a valid device and ground winding\"}";
    if(engine.substrate_count<2)return "{\"error\":\"The layered potential solve requires a substrate stack\"}";
    for(int i=0;i<engine.substrate_count;++i) { double eps;__builtin_memcpy(&eps,engine.substrate[i].prefix+4,8);
        if(!(eps>0) || !(engine.substrate[i].resistivity>0))return "{\"error\":\"Define positive layer permittivity and resistivity in the technology\"}"; }
    for(int i=0;i<engine.metal_count;++i) { int layer;__builtin_memcpy(&layer,engine.metals[i].prefix+0xa0,4);if(layer<=0 || layer>=engine.substrate_count)return "{\"error\":\"Every metal must lie above the first substrate layer\"}"; }
    const unsigned largest=nx>ny?nx:ny,pairs=engine.metal_count*(engine.metal_count+1)/2;
    if(pairs*((largest+1)*(largest+1)+4*largest)*16u>128u*1024u*1024u)return "{\"error\":\"The requested Green tables exceed the 128 MiB cache budget\"}";
    const Real x_pitch=Real(chip_x)/nx,y_pitch=Real(chip_y)/ny;
    if(length_cell<x_pitch || length_cell<y_pitch) { length_cell=x87::binary64(y_pitch<x_pitch?x_pitch:y_pitch);engine.warnings.emit("CCELL length raised to the native chip/FFT pitch"); }
    if(width_cell<x_pitch || width_cell<y_pitch) { width_cell=x87::binary64(y_pitch<x_pitch?x_pitch:y_pitch);engine.warnings.emit("CCELL width raised to the native chip/FFT pitch"); }
    Workspace work(nx,ny);const auto memory=work.memory();
    // CAP's third argument uses native parser type 8, which resets the ground's
    // main mesh counts to -1/-1. Apply that exact acquisition step to a private
    // clone, so repeated solves leave the user's source geometry intact.
    if(ground>=0) { const bool identical=source==reference;reference=clone_for_ground(*reference,memory);if(identical)source=reference; }
    const PotentialContactOptions contacts{nx,ny,engine.metal_count,chip_x,chip_y,length_cell,width_cell};int count=0,groups=0;
    if(!admissible(*source,contacts,contact_limit,count,groups,memory,engine.warnings) ||
       (!same_name(*source,*reference) && !admissible(*reference,contacts,contact_limit,count,groups,memory,engine.warnings)))
        return "{\"error\":\"Contact mesh is outside the chip, degenerate, or exceeds the selected contact budget\"}";
    int ground_segments=0;if(!same_name(*source,*reference))for(auto* s=reference->filaments.first;s;s=next(s))++ground_segments;
    if(groups<=ground_segments)return "{\"error\":\"Native terminal folding has no device groups after subtracting the ground records\"}";
    PotentialCapacitanceState state{};state.coefficients=static_cast<ComplexValue*>(memory.arrays.get(engine.substrate_count*16u));
    auto* depths=static_cast<double*>(memory.arrays.get((engine.substrate_count+1)*8u));for(int i=0;i<engine.substrate_count;++i)depths[i]=engine.substrate[i].height;depths[engine.substrate_count]=0;
    // Validate native contact indices before any Green-table indexing. Keep all
    // native meshing operations and the source's explicit incoming mesh words.
    if(!prepare_potential_contacts(source->filaments,reference->filaments,state.groups,contacts,memory,{},engine.warnings))return "{\"error\":\"Original contact builder rejected the geometry\"}";
    for(auto* g=state.groups;g;g=g->next)for(auto* c=g->first;c;c=reinterpret_cast<PotentialContact*>(c->next_address))
        if(c->x0<0 || c->x1>nx || c->y0<0 || c->y1>ny || c->x0>=c->x1 || c->y0>=c->y1)return "{\"error\":\"A contact collapses on the FFT grid or crosses its boundary; use a finer FFT grid or move/widen the conductor\"}";
    release_potential_contacts(state.groups,memory);
    if(!allocate_potential_cache(state.cache,nx,ny,engine.metal_count,memory,engine.warnings))return "{\"error\":\"Could not allocate original Green tables\"}";
    Snapshots snapshots{memory};const PotentialCacheRuntime runtime{{no_open,no_read,no_write,no_close,sprintf,nullptr},"","",engine.warnings,engine.warnings};
    PotentialCapacitanceOptions options{contacts,{engine.substrate,engine.substrate_count,engine.metals,engine.metal_count},depths,runtime,true,true,Snapshots::record,&snapshots,{}};
    ComplexMatrix matrix{};total_capacitance=total_resistance=__builtin_nan("");
    const bool ok=unfolded?solve_unfolded_capacitance(source->filaments,reference->filaments,matrix,frequency,state,options,memory):solve_capacitance(source->filaments,reference->filaments,matrix,frequency,state,options,memory);
    if(!ok || matrix.rows<1 || !finite_matrix(matrix) || !finite_matrix(snapshots.potential))return "{\"error\":\"Original capacitance solver did not produce a finite matrix\"}";
    const unsigned capacity=64u*(matrix.rows*matrix.columns+snapshots.potential.rows*snapshots.potential.columns)+16384u;
    output=static_cast<char*>(calloc(1,capacity));if(!output)return "{\"error\":\"Unable to allocate capacitance output\"}";char* out=output;
    out+=sprintf(out,"{\"name\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(source));out+=sprintf(out,",\"ground\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(reference));
    out+=sprintf(out,",\"frequency_hz\":%.17g,\"contacts\":%d,\"groups\":%d,\"unfolded\":%s,\"mesh\":{\"length\":%.17g,\"width\":%.17g,\"chip_x\":%.17g,\"chip_y\":%.17g,\"fft_x\":%d,\"fft_y\":%d},\"capacitance_f\":",frequency,snapshots.contacts,groups,unfolded?"true":"false",length_cell,width_cell,chip_x,chip_y,nx,ny);
    out+=__builtin_isfinite(total_capacitance)?sprintf(out,"%.17g",total_capacitance):sprintf(out,"null");out+=sprintf(out,",\"resistance\":");out+=__builtin_isfinite(total_resistance)?sprintf(out,"%.17g",total_resistance):sprintf(out,"null");
    out+=sprintf(out,",\"green_table_hash\":\"%016llx\",\"matrix\":",table_hash(state.cache,nx,ny,pairs));out=matrix_json(out,matrix);out+=sprintf(out,",\"potential_matrix\":");out=matrix_json(out,snapshots.potential);sprintf(out,"}");return output;
}
