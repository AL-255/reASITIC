#include "engine.hpp"
#include "filament_policy.hpp"
#include "potential_grid.hpp"
#include "json.hpp"
#include "eddy_runtime.hpp"
#include "three_history.hpp"
#include "numerical_policy.hpp"
#include "two_port_records.hpp"
#include "asitic/combined_network.hpp"
#include "asitic/extended_twoport.hpp"
#include "asitic/extended_analysis.hpp"
namespace asitic { GeometryShape* prepare_pad_geometry(const GeometryShape&,const GeometryShape&,const GeometryShape&,CombinedNetworkState&,const PadDeembeddingOptions&,GeometryShape*&); }
#include "asitic/later_analysis.hpp"
#include "asitic/three_port_analysis.hpp"
#include "asitic/port_output.hpp"
namespace asitic { GeometryShape* prepare_three_port_geometry(const GeometryShape&,const GeometryShape&,const GeometryShape&,GeometryShape&,ThreePortAnalysisState&,const ThreePortAnalysisOptions&,int&,int&,unsigned&); }
namespace asitic { GeometryShape* prepare_transformer_geometry(const GeometryShape&,const GeometryShape&,LaterAnalysisState&,const LaterAnalysisOptions&,int&); }
#include "asitic/potential_solver.hpp"
#include "asitic/legacy_complex.hpp"
#ifdef __EMSCRIPTEN__
#include "x87_port.hpp"
#include "pi_shunts.hpp"
namespace asitic { PiEquivalent extract_pi_equivalent_with_shunts(const TwoPortAdmittance&,double,ComplexValue,ComplexValue); }
using Real=SoftReal;
#else
#include "x87.hpp"
using Real=long double;
#endif
extern "C" void* calloc(unsigned,unsigned);
extern "C" void free(void*);
extern "C" int sprintf(char*,const char*,...);
extern "C" int vsnprintf(char*,unsigned,const char*,__builtin_va_list);
extern "C" void abort();
using namespace asitic;
namespace {
char* output=nullptr;double total_capacitance=0,total_resistance=0;
// The solver stores dense n-by-n complex matrices. Bound their storage,
// independently of processor arithmetic or the native contact-selection rules.
constexpr unsigned contact_matrix_bytes=16u*1024u*1024u;
constexpr int contact_count_limit(){int n=0;while(static_cast<unsigned long long>(n+1)*(n+1)<=contact_matrix_bytes/sizeof(ComplexValue))++n;return n;}
constexpr int max_contacts=contact_count_limit();
bool contact_budget(int n){return n>=1&&static_cast<unsigned long long>(n)*n<=contact_matrix_bytes/sizeof(ComplexValue);}
struct Allocation { void* pointer;Allocation* next; };
struct Workspace {
    Allocation* allocations=nullptr;int nx,ny;unsigned matrix_request=0,matrix_backing=0;
    Workspace(int x,int y):nx(x),ny(y) {}
    ~Workspace() { while(allocations) { auto* p=allocations;allocations=p->next;free(p->pointer);free(p); } }
    static void* get(unsigned n,void* user) {
        auto& w=*static_cast<Workspace*>(user);unsigned backing=n;
        // Historical nonsquare diagnostics need backing for inconsistent
        // strides/scratch requests. Production uses a contiguous rectangular
        // table and scratch sized for both axes by the source adapter.
        if(reasitic::reference_numerics()){
            const unsigned largest=w.nx>w.ny?w.nx:w.ny;
            if(n==static_cast<unsigned>((w.nx+1)*(w.ny+1))*16u)backing=(largest+1)*(largest+1)*16u;
            if(n==static_cast<unsigned>(w.nx)*32u)backing=largest*32u;
        }
        if(n==w.matrix_request && backing<w.matrix_backing)backing=w.matrix_backing;
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
// Ordinary requests disable file IO; RAM-cache requests supply other callbacks.
void* no_open(const char*,const char*,void*) { return nullptr; }
unsigned no_read(void*,unsigned,unsigned,void*,void*) { return 0; }
unsigned no_write(const void*,unsigned,unsigned,void*,void*) { return 0; }
int no_close(void*,void*) { return 0; }
#include "potential_memo.inc"
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
            const auto n=static_cast<long long>(current->length_cells)*current->width_cells;memory.put(head);
            if(n<1 || n>limit-count)return false;count+=static_cast<int>(n);
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
struct NetworkRequest { double length,width,thickness;int filament_limit;reasitic::EddySession* eddy;int secondary=-1;double termination=0;int third=-1;const ComplexValue* cap_history=nullptr;const ComplexValue* ind_history=nullptr;int history_cells=0;const unsigned* history_addresses=nullptr;bool extended=false;int first_pad=-1,second_pad=-1; };
int private_name_random(void*) { return 65; }
void network_terminate(int,void*) { abort(); }
struct NetworkSnapshots {
    SolverMemory memory;ComplexMatrix capacitance{},inductance{},node_capacitance{},system_before{},system_after{},currents{},voltages{},pads{},with_pads{};
    static void record(const char* name,const ComplexMatrix& matrix,void* user) {
        auto& s=*static_cast<NetworkSnapshots*>(user);
        if(!((name[0]=='Z' && name[1]=='C') || (name[0]=='Y' && name[1]=='M') || name[0]=='c' || name[0]=='v' || name[0]=='P' || name[0]=='W'))return;
        auto& copy=name[0]=='P'?s.pads:name[0]=='W'?s.with_pads:name[0]=='c'?s.currents:name[0]=='v'?s.voltages:name[0]=='Z'?s.capacitance:s.inductance;
        copy=matrix;copy.stride=copy.rows;copy.vector_reference=copy.matrix_reference=0;
        copy.data=static_cast<ComplexValue*>(s.memory.arrays.get(matrix.rows*matrix.columns*16u));
        for(int j=0;j<matrix.columns;++j)for(int i=0;i<matrix.rows;++i)copy.view().at(i,j)=matrix.view().at(i,j);
    }
    static void record_nodes(const char* name,ComplexMatrixView matrix,int n,void* user) {
        auto& s=*static_cast<NetworkSnapshots*>(user);auto& copy=name[0]=='A'?(name[4]=='p'?s.system_before:s.system_after):s.node_capacitance;
        copy={static_cast<ComplexValue*>(s.memory.arrays.get(n*n*16u)),n*n,0,n,n,n,0};
        for(int j=0;j<n;++j)for(int i=0;i<n;++i)copy.view().at(i,j)=matrix.at(i,j);
    }
};
char* number_json(char* out,double value) { return out+(__builtin_isfinite(value)?sprintf(out,"%.17g",value):sprintf(out,"null")); }
char* complex_json(char* out,ComplexValue z) { out+=sprintf(out,"[");out=number_json(out,z.real);out+=sprintf(out,",");out=number_json(out,z.imag);out+=sprintf(out,"]");return out; }

bool three_name_admissible(const GeometryShape& source,const GeometryShape* head) {
    char name[80];unsigned i=0;while(source.filaments.prefix[i] && i<76){name[i]=source.filaments.prefix[i];++i;}if(source.filaments.prefix[i])return false;
    name[i++]='_';name[i++]='_';name[i]=0;
    for(auto* p=head;p;p=p->next){const char* q=reinterpret_cast<const char*>(p);unsigned j=0;while(name[j] && name[j]==q[j])++j;if(name[j]==q[j])return false;}return true;
}
char* cells_json(char* out,const ComplexValue* values,int n) {
    out+=sprintf(out,"[");for(int i=0;i<n;++i){out+=sprintf(out,"%s[",i?",":"");for(int j=0;j<n;++j){if(j)out+=sprintf(out,",");out=complex_json(out,values[i*n+j]);}out+=sprintf(out,"]");}return out+sprintf(out,"]");
}
struct RecordBuffer { char* data;unsigned used,capacity; };
int record_print(void* user,const char* format,...) {
    auto& b=*static_cast<RecordBuffer*>(user);__builtin_va_list args;__builtin_va_start(args,format);const int n=vsnprintf(b.data+b.used,b.capacity-b.used,format,args);__builtin_va_end(args);
    if(n>0){const unsigned remaining=b.capacity-b.used;b.used+=static_cast<unsigned>(n)<remaining?static_cast<unsigned>(n):remaining-1;}return n;
}
char* three_records_json(char* out,const ThreePortImpedance& impedance,double frequency,SolverMemory memory,WarningSink warnings,bool polar) {
    out+=sprintf(out,"{");const char* keys[]={"s","y","z"};
    for(int parameter=1;parameter<=3;++parameter){char record[4096]{};RecordBuffer buffer{record,0,sizeof(record)};ThreePortOutputState state{};state.impedance=impedance;
        const PortOutputRuntime runtime{record_print,memory,{warnings},warnings};write_three_port_record(&buffer,frequency,parameter,polar?1:0,state,runtime);
        out+=sprintf(out,"%s\"%s\":",parameter>1?",":"",keys[parameter-1]);out=reasitic::json_string(out,record);
    }return out+sprintf(out,"}");
}
char* three_headers_json(char* out,bool polar) {
    out+=sprintf(out,"{");const char* keys[]={"s","y","z"};
    for(int parameter=1;parameter<=3;++parameter){char text[4096]{};RecordBuffer buffer{text,0,sizeof(text)};
        const PortOutputRuntime runtime{record_print,{},{},{}};write_three_port_header(&buffer,parameter,polar?1:0,runtime);
        out+=sprintf(out,"%s\"%s\":",parameter>1?",":"",keys[parameter-1]);out=reasitic::json_string(out,text);
    }return out+sprintf(out,"}");
}

}
namespace asitic {
// Observation of the original rounded totals immediately before native CAP's
// formatted output. No expression or arithmetic in the solve is replaced.
void record_capacitance_totals(double capacitance,double resistance) { total_capacitance=capacitance;total_resistance=resistance; }
}
namespace reasitic { void release_capacitance_result(){free(output);output=nullptr;} }
static const char* simulate_capacitance_impl(int winding,int ground,double frequency,double length_cell,double width_cell,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int unfolded,const NetworkRequest* network) {
    reasitic::release_capacitance_result();reasitic::clear_diagnostics();const auto engine=reasitic::engine_view();
    if(winding<0 || ground< -1 || !__builtin_isfinite(frequency) || frequency<=0 ||
       !__builtin_isfinite(length_cell) || length_cell<=0 || !__builtin_isfinite(width_cell) || width_cell<=0 ||
       !__builtin_isfinite(chip_x) || chip_x<=0 || !__builtin_isfinite(chip_y) || chip_y<=0 ||
       nx<1 || ny<1 || (nx&(nx-1)) || (ny&(ny-1)) || !contact_budget(contact_limit)){
        static char error[384];sprintf(error,"{\"error\":\"Use positive frequency, cells and chip dimensions, positive power-of-two FFT sizes, and 1–%d contacts (16 MiB dense-matrix budget)\"}",max_contacts);return error;
    }
    auto* source=engine.shapes;for(int i=0;source && i<winding;++i)source=source->next;
    auto* reference=engine.shapes;for(int i=0;reference && i<ground;++i)reference=reference->next;if(ground<0)reference=source;
    GeometryShape* secondary=nullptr;GeometryShape* third=nullptr;const bool three=network && network->third>=0,transformer=network && network->secondary>=0 && !three;
    if(transformer || three) {
        secondary=engine.shapes;for(int i=0;secondary && i<network->secondary;++i)secondary=secondary->next;
        if(!secondary || secondary==source || !__builtin_isfinite(network->termination))return "{\"error\":\"Select two distinct windings and a finite transformer termination\"}";
    }
    if(three){
        third=engine.shapes;for(int i=0;third && i<network->third;++i)third=third->next;
        if(!third || third==source || third==secondary)return "{\"error\":\"Select three distinct winding records\"}";
        if(source && (!three_name_admissible(*source,engine.shapes) || !three_name_admissible(*secondary,engine.shapes) || !three_name_admissible(*third,engine.shapes)))return "{\"error\":\"Original three-port temporary names collide with the layout\"}";
    }
    const bool padbed=network && network->first_pad>=0;GeometryShape* pad1=nullptr;GeometryShape* pad2=nullptr;
    if(padbed){
        pad1=engine.shapes;for(int i=0;pad1 && i<network->first_pad;++i)pad1=pad1->next;
        pad2=engine.shapes;for(int i=0;pad2 && i<network->second_pad;++i)pad2=pad2->next;
        if(!pad1 || !pad2)return "{\"error\":\"Select valid first and second pad records\"}";
        GeometryShape* pad_shapes[]={pad1,pad2};for(auto* shape:pad_shapes){int n=0;while(reinterpret_cast<const char*>(shape)[n] && n<78)++n;if(n>=78)return "{\"error\":\"Native PI4 pad temporary names exceed their safe capacity\"}";}
    }
    if(!source || !reference)return "{\"error\":\"Select a valid device and ground winding\"}";
    if(three && reasitic::reference_numerics() && !same_name(*source,*reference) && (!network->cap_history || !network->ind_history || network->history_cells<1))return "{\"error\":\"Native separate-reference three-port results depend on out-of-bounds matrix backing; captured backing is required for exact comparison\"}";
    if(three && (network->history_cells<0 || network->history_cells>512*512))return "{\"error\":\"Invalid three-port backing size\"}";
    if(engine.substrate_count<2)return "{\"error\":\"The layered potential solve requires a substrate stack\"}";
    for(int i=0;i<engine.substrate_count;++i) { double eps;__builtin_memcpy(&eps,engine.substrate[i].prefix+4,8);
        if(!(eps>0) || !(engine.substrate[i].resistivity>0))return "{\"error\":\"Define positive layer permittivity and resistivity in the technology\"}"; }
    for(int i=0;i<engine.metal_count;++i) { int layer;__builtin_memcpy(&layer,engine.metals[i].prefix+0xa0,4);if(layer<=0 || layer>=engine.substrate_count)return "{\"error\":\"Every metal must lie above the first substrate layer\"}"; }
    const unsigned pairs=engine.metal_count*(engine.metal_count+1)/2;
    if(!reasitic::potential_grid_fits(nx,ny,engine.metal_count,reasitic::reference_numerics()))return "{\"error\":\"The requested Green tables exceed the 128 MiB cache budget\"}";
    const Real x_pitch=Real(chip_x)/nx,y_pitch=Real(chip_y)/ny;
    if(length_cell<x_pitch || length_cell<y_pitch) { length_cell=x87::binary64(y_pitch<x_pitch?x_pitch:y_pitch);engine.warnings.emit("CCELL length raised to the native chip/FFT pitch"); }
    if(width_cell<x_pitch || width_cell<y_pitch) { width_cell=x87::binary64(y_pitch<x_pitch?x_pitch:y_pitch);engine.warnings.emit("CCELL width raised to the native chip/FFT pitch"); }
    Workspace work(nx,ny);const auto memory=work.memory();
    // CAP's third argument uses native parser type 8, which resets the ground's
    // main mesh counts to -1/-1. Apply that exact acquisition step to a private
    // clone, so repeated solves leave the user's source geometry intact.
    if(ground>=0) { const bool identical=source==reference,second_identical=secondary==reference,third_identical=third==reference;reference=clone_for_ground(*reference,memory);if(identical)source=reference;if(second_identical)secondary=reference;if(third_identical)third=reference; }
    if(padbed){
        GeometryShape* pads[]={pad1,pad2};
        for(auto* pad:pads){auto* copy=clone_for_ground(*pad,memory);if(source==pad)source=copy;if(reference==pad)reference=copy;if(pad1==pad)pad1=copy;if(pad2==pad)pad2=copy;}
    }
    const auto* original_source=source;const auto* original_secondary=secondary;const auto* original_third=third;const auto* original_reference=reference;GeometryShape* preview_head=nullptr;int network_filaments=0,boundary=0,second_boundary=0;unsigned node_count=0;GeometryShape* pads_preview=nullptr;int pad_contacts=0,pad_groups=0;
    char composition_message[1024];const GeometryRuntime private_geometry{memory,engine.warnings,network_terminate,nullptr};
    const GeometryTechnology composition_technology{engine.metals,engine.vias,engine.metal_heights,engine.metal_count};
    const GeometryCommandRuntime commands{{private_geometry,0,engine.warnings,sprintf,composition_message,nullptr,nullptr},nullptr,memory,private_name_random,nullptr};
    if(network) {
        if(!__builtin_isfinite(network->length) || network->length<=0 || !__builtin_isfinite(network->width) || network->width<=0 ||
           !__builtin_isfinite(network->thickness) || network->thickness<=0 || network->filament_limit<1 || network->filament_limit>1024)
            return "{\"error\":\"Use positive network cells and a filament limit from 1 to 1024\"}";
        if(!source->filaments.first || !reasitic::bounded_solver_clone(*source,network->length,length_cell,network->filament_limit))
            return "{\"error\":\"Network longitudinal mesh exceeds the selected filament budget\"}";
        const GeometryRuntime geometry{memory,engine.warnings,network_terminate,nullptr};
        if(padbed){
            const GeometryShape* pad_shapes[]={pad1,pad2};for(auto* pad:pad_shapes)if(!pad->filaments.first || !reasitic::bounded_solver_clone(*pad,network->length,length_cell,network->filament_limit))return "{\"error\":\"Pad longitudinal mesh exceeds the selected filament budget\"}";
            CombinedNetworkState preview{};preview.shapes=engine.shapes;PadDeembeddingOptions preparation{};preparation.technology=composition_technology;preparation.commands=commands;preparation.combined.discretization={network->length,length_cell};
            auto* joined=prepare_pad_geometry(*source,*pad1,*pad2,preview,preparation,pads_preview);
            if(!reasitic::bounded_solver_clone(*joined,network->length,length_cell,network->filament_limit))return "{\"error\":\"Device with pads exceeds the longitudinal filament budget\"}";
            source=clone_geometry_shape(preview.shapes,*joined,{network->length,length_cell},geometry);
        }else if(three) {
            const GeometryShape* other_shapes[]={secondary,third};for(auto* shape:other_shapes)if(!shape->filaments.first || !reasitic::bounded_solver_clone(*shape,network->length,length_cell,network->filament_limit))return "{\"error\":\"Three-port longitudinal mesh exceeds the selected filament budget\"}";
            ThreePortAnalysisState preview{};ThreePortAnalysisOptions preparation{};preparation.discretization={network->length,length_cell};preparation.technology=composition_technology;preparation.commands=commands;
            source=prepare_three_port_geometry(*source,*secondary,*third,*reference,preview,preparation,boundary,second_boundary,node_count);
            if(same_name(*original_source,*original_reference))reference=source;
            if(boundary<1 || second_boundary<1 || static_cast<unsigned>(boundary+second_boundary)>=node_count || node_count>static_cast<unsigned>(contact_limit))return "{\"error\":\"Three-port winding boundaries or reference-inclusive node budget are invalid\"}";
            if(network->history_cells && static_cast<unsigned>(network->history_cells)!=node_count*node_count)return "{\"error\":\"Captured three-port backing size does not match reference-inclusive node count\"}";
            work.matrix_request=count_geometry_metals(*source,engine.metal_count);work.matrix_request=work.matrix_request*work.matrix_request*16u;work.matrix_backing=node_count*node_count*16u;
        }else if(transformer) {
            if(!secondary->filaments.first || !reasitic::bounded_solver_clone(*secondary,network->length,length_cell,network->filament_limit))return "{\"error\":\"Secondary longitudinal mesh exceeds the selected filament budget\"}";
            LaterAnalysisState preview{};LaterAnalysisOptions preparation{};preparation.discretization={network->length,length_cell};preparation.technology=composition_technology;preparation.commands=commands;
            source=prepare_transformer_geometry(*source,*secondary,preview,preparation,boundary);
            if(same_name(*original_source,*original_reference))reference=source;
        }else source=clone_geometry_shape(preview_head,*source,{network->length,length_cell},geometry);
        InductanceContext context{};context.shape=&source->filaments;
        for(auto* s=source->filaments.first;s;s=next(s))for(auto* b=s;b;b=reinterpret_cast<const ConductorSegment*>(b->group_next_address))
            if(b->metal<engine.metal_count && (Real(b->thickness)/network->thickness+Real(.5)>network->filament_limit || Real(b->width)/network->width+Real(.5)>2147483647.L))
                return "{\"error\":\"Network cross-section mesh exceeds the selected filament budget\"}";
        if(!setup_filament_groups(context,engine.metal_count,memory.arrays,engine.warnings) || context.group_count<1 ||
           !index_conductor_segments(context,engine.metal_count,memory.arrays,engine.warnings) ||
           !set_filament_counts(context,network->width,network->thickness,engine.max_width_cells,memory.arrays,engine.warnings) || context.filament_count>network->filament_limit)
            return "{\"error\":\"Network cross-section mesh exceeds the selected filament budget\"}";
        if(!reasitic::valid_width_grading<Real>(context,network->width,engine.max_width_cells))return "{\"error\":\"MAXNW edge grading produces a nonfinite width; use at least three width cells or a larger width cell size\"}";
        network_filaments=context.filament_count;
    }
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
    if(padbed){
        if(!admissible(*pads_preview,contacts,contact_limit,pad_contacts,pad_groups,memory,engine.warnings) ||
           (!same_name(*pads_preview,*reference) && !admissible(*reference,contacts,contact_limit,pad_contacts,pad_groups,memory,engine.warnings)))return "{\"error\":\"Pads-only contact mesh exceeds its budget or is degenerate\"}";
        int reference_groups=0;if(!same_name(*pads_preview,*reference))for(auto* s=reference->filaments.first;s;s=next(s))++reference_groups;
        if(pad_groups-reference_groups<2)return "{\"error\":\"Original PI4 pad subtraction requires at least two folded pad groups\"}";
        if(!prepare_potential_contacts(pads_preview->filaments,reference->filaments,state.groups,contacts,memory,{},engine.warnings))return "{\"error\":\"Original pads-only contact builder rejected the geometry\"}";
        for(auto* g=state.groups;g;g=g->next)for(auto* c=g->first;c;c=reinterpret_cast<PotentialContact*>(c->next_address))if(c->x0<0 || c->x1>nx || c->y0<0 || c->y1>ny || c->x0>=c->x1 || c->y0>=c->y1)return "{\"error\":\"A pads-only contact collapses on the FFT grid or crosses its boundary\"}";
        release_potential_contacts(state.groups,memory);
    }
    if(!allocate_potential_cache(state.cache,nx,ny,engine.metal_count,memory,engine.warnings))return "{\"error\":\"Could not allocate original Green tables\"}";
    PotentialMemoFile memo(engine,frequency,chip_x,chip_y,nx,ny);state.cache.enabled=memo.enabled;
    Snapshots snapshots{memory};const PotentialCacheRuntime runtime{memo.enabled?memo.files():PotentialCacheFiles{no_open,no_read,no_write,no_close,sprintf,nullptr},"","",engine.warnings,engine.warnings};
    PotentialCapacitanceOptions options{contacts,{engine.substrate,engine.substrate_count,engine.metals,engine.metal_count},depths,runtime,true,true,Snapshots::record,&snapshots,{}};
    if(network) {
        const auto expected_groups=count_geometry_metals(*source,engine.metal_count);
        if(groups-ground_segments!=expected_groups)return "{\"error\":\"Native network capacitance and inductance dimensions do not agree\"}";
        CombinedNetworkState combined{};combined.capacitance=state;LaterAnalysisState coupled{};coupled.capacitance=state;
        NetworkSnapshots intermediate{memory};
        const InductanceRunOptions inductance{{network->width,engine.max_width_cells,engine.metals},network->thickness,engine.metal_count,engine.vias,false};
        const CombinedNetworkOptions network_options{{network->length,length_cell},options,inductance,reasitic::eddy_options(*network->eddy,engine),
            {true,sprintf,engine.warnings,NetworkSnapshots::record_nodes,&intermediate},engine.warnings,false,false,NetworkSnapshots::record,&intermediate,{}};
        const GeometryRuntime geometry{memory,engine.warnings,network_terminate,nullptr};
        bool ok;
        if(padbed){
            combined.shapes=engine.shapes;const PadDeembeddingOptions pad_options{composition_technology,commands,network_options,false,{}};
            ok=solve_pad_deembedded_network(*original_source,*pad1,*pad2,*original_reference,frequency,combined,pad_options);
        }else if(network->extended) {
            ExtendedTwoPortState extended{};extended.network.capacitance=state;
            const ExtendedTwoPortOptions extended_options{network_options,{frequency,true,sprintf,nullptr,engine.warnings,NetworkSnapshots::record,&intermediate,nullptr},geometry,nullptr,sprintf,nullptr,nullptr};
            ok=solve_extended_twoport_network(*original_source,*reference,frequency,extended,extended_options);combined=extended.network;
        }else if(three) {
            if(ground>=0)reset_geometry_mesh_counts(*const_cast<GeometryShape*>(original_reference));
            ThreePortAnalysisState state3{};state3.capacitance=state;
            auto ind3=inductance;ind3.snapshot=NetworkSnapshots::record;ind3.snapshot_user=&intermediate;
            const ThreePortAnalysisOptions options3{{network->length,length_cell},composition_technology,options,ind3,network_options.eddy,network_options.nodes,commands,false,nullptr,nullptr};
            reasitic::ThreePortHistory history{memory,network->cap_history,network->ind_history,network->history_addresses,network->history_cells};
            reasitic::ThreePortHistoryScope history_scope(network->history_cells?&history:nullptr);
            ok=solve_three_port_network(*original_source,*original_secondary,*original_third,*const_cast<GeometryShape*>(original_reference),frequency,state3,options3);
            if(!history.prefix_matches)return "{\"error\":\"Captured backing matrix prefix differs from the original computed matrices\"}";
            if(!history.contract_matches)return "{\"error\":\"Captured native three-port address layout or allocation contract is invalid\"}";
            if(network->eddy->exceeded)return "{\"error\":\"Three-port eddy evaluation budget exceeded\"}";
            const auto* z=reinterpret_cast<const ComplexValue*>(&state3.impedance);
            if(!ok || !finite_matrix(intermediate.capacitance) || !finite_matrix(intermediate.inductance))return "{\"error\":\"Original three-port solver did not produce finite matrices\"}";
            for(int i=0;i<9;++i)if(!__builtin_isfinite(z[i].real) || !__builtin_isfinite(z[i].imag))return "{\"error\":\"Original three-port node equations produced nonfinite impedance\"}";
            ThreePortAdmittance admittance{};ThreePortScattering scattering{};convert_three_port_admittance(state3.impedance,admittance,memory,{engine.warnings});convert_three_port_scattering(state3.impedance,scattering,memory,{engine.warnings});
            const unsigned capacity=64u*(intermediate.capacitance.elements+intermediate.inductance.elements+intermediate.system_before.elements+intermediate.system_after.elements)+65536u;
            output=static_cast<char*>(calloc(1,capacity));if(!output)return "{\"error\":\"Unable to allocate three-port output\"}";char* out=output;
            out+=sprintf(out,"{\"names\":[");out=reasitic::json_string(out,reinterpret_cast<const char*>(original_source));out+=sprintf(out,",");out=reasitic::json_string(out,reinterpret_cast<const char*>(original_secondary));out+=sprintf(out,",");out=reasitic::json_string(out,reinterpret_cast<const char*>(original_third));out+=sprintf(out,"],\"ground\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(original_reference));
            if(engine.max_width_cells!=500)out+=sprintf(out,",\"max_width_cells\":%d",engine.max_width_cells);
            out+=sprintf(out,",\"frequency_hz\":%.17g,\"contacts\":%d,\"groups\":%d,\"filaments\":%d,\"node_count\":%u,\"winding_groups\":[%d,%d,%d],\"mesh\":{\"length\":%.17g,\"width\":%.17g,\"thickness\":%.17g,\"contact_length\":%.17g,\"contact_width\":%.17g,\"chip_x\":%.17g,\"chip_y\":%.17g,\"fft_x\":%d,\"fft_y\":%d},\"impedance_matrix\":",frequency,snapshots.contacts,expected_groups,network_filaments,node_count,boundary,second_boundary,expected_groups-boundary-second_boundary,network->length,network->width,network->thickness,length_cell,width_cell,chip_x,chip_y,nx,ny);

            out=cells_json(out,z,3);out+=sprintf(out,",\"admittance_matrix\":");out=cells_json(out,reinterpret_cast<const ComplexValue*>(&admittance),3);out+=sprintf(out,",\"scattering_matrix\":");out=cells_json(out,reinterpret_cast<const ComplexValue*>(&scattering),3);
            out+=sprintf(out,",\"model\":{");const char* model_names[]={"inductance_h","resistance_ohm","mutual_inductance_h","cross_resistance_ohm","coupling"};const double* values[]={state3.model.inductance,state3.model.resistance,state3.model.mutual,state3.model.cross_resistance,state3.model.coupling};
            for(int i=0;i<5;++i){out+=sprintf(out,"%s\"%s\":[",i?",":"",model_names[i]);for(int j=0;j<3;++j){if(j)out+=sprintf(out,",");out=number_json(out,values[i][j]);}out+=sprintf(out,"]");}out+=sprintf(out,"},\"eddy\":");out=reasitic::eddy_metadata(out,*network->eddy);
            out+=sprintf(out,",\"capacitance_matrix\":");out=matrix_json(out,intermediate.capacitance);out+=sprintf(out,",\"inductance_impedance_matrix\":");out=matrix_json(out,intermediate.inductance);out+=sprintf(out,",\"system_before\":");out=matrix_json(out,intermediate.system_before);out+=sprintf(out,",\"system_matrix\":");out=matrix_json(out,intermediate.system_after);
            if(network->history_cells)out+=sprintf(out,",\"backing\":{\"captured\":true,\"cells\":%d,\"prefix_matches\":true}",network->history_cells);
            out+=sprintf(out,",\"records\":");out=three_records_json(out,state3.impedance,frequency,memory,engine.warnings,false);out+=sprintf(out,",\"polar_records\":");out=three_records_json(out,state3.impedance,frequency,memory,engine.warnings,true);
            out+=sprintf(out,",\"headers\":");out=three_headers_json(out,false);out+=sprintf(out,",\"polar_headers\":");out=three_headers_json(out,true);
            sprintf(out,",\"green_table_hash\":\"%016llx\"}",table_hash(state3.capacitance.cache,nx,ny,pairs));return output;
        }else if(transformer) {
            if(boundary<1 || boundary>=expected_groups)return "{\"error\":\"Transformer ports require metal conductor groups in both windings\"}";
            auto transformer_inductance=inductance;transformer_inductance.snapshot=NetworkSnapshots::record;transformer_inductance.snapshot_user=&intermediate;
            const LaterAnalysisOptions coupled_options{{network->length,length_cell},composition_technology,options,transformer_inductance,network_options.eddy,network_options.nodes,commands,engine.warnings,false,nullptr,nullptr};
            ok=solve_transformer_network(*original_source,*original_secondary,*original_reference,frequency,network->termination,coupled,coupled_options);
        }else ok=solve_combined_network(*original_source,*reference,frequency,combined,network_options,geometry);
        if(network->eddy->exceeded)return "{\"error\":\"Network eddy evaluation budget exceeded\"}";
        if(padbed && (intermediate.pads.rows<2 || !finite_matrix(intermediate.pads) || !finite_matrix(intermediate.with_pads) || !finite_matrix(intermediate.capacitance) || !finite_matrix(intermediate.inductance)))return "{\"error\":\"Original pad deembedding did not produce finite stage matrices\"}";
#ifdef __EMSCRIPTEN__
        if(ok&&padbed&&!reasitic::reference_numerics()){
            double ports[8];
            if(!reasitic::folded_port_network(reinterpret_cast<const double*>(intermediate.capacitance.data),reinterpret_cast<const double*>(intermediate.inductance.data),intermediate.capacitance.rows,intermediate.capacitance.stride,intermediate.inductance.stride,reinterpret_cast<const double*>(intermediate.pads.data),intermediate.pads.stride,ports,nullptr))return "{\"error\":\"Guarded pad deembedding did not produce finite stage matrices\"}";
            combined.ports={{ports[0],ports[1]},{ports[4],ports[5]},{ports[2],ports[3]},{ports[6],ports[7]}};
        }
#endif
        if(network->extended && (!finite_matrix(intermediate.currents) || !finite_matrix(intermediate.voltages)))return "{\"error\":\"Original extended node equations produced nonfinite vectors\"}";
        const auto& y=transformer?coupled.ports:combined.ports;const ComplexValue ports[]={y.y11,y.y12,y.y21,y.y22};
        if(!ok || !finite_matrix(intermediate.capacitance) || !finite_matrix(intermediate.inductance))return "{\"error\":\"Original combined solver did not produce finite matrices\"}";
        for(const auto z:ports)if(!__builtin_isfinite(z.real) || !__builtin_isfinite(z.imag))return "{\"error\":\"Original node equations produced nonfinite admittance\"}";
#ifdef __EMSCRIPTEN__
        reasitic::PiShunts shunts{};double branch_impedances[6];const bool stable_shunts=!transformer&&!reasitic::reference_numerics();
        if(stable_shunts){
            const bool calculated=network->extended?reasitic::extended_common_mode_shunts(reinterpret_cast<const double*>(intermediate.capacitance.data),reinterpret_cast<const double*>(intermediate.inductance.data),intermediate.capacitance.rows,intermediate.capacitance.stride,intermediate.inductance.stride,shunts):reasitic::common_mode_shunts(reinterpret_cast<const double*>(intermediate.capacitance.data),reinterpret_cast<const double*>(intermediate.inductance.data),intermediate.capacitance.rows,intermediate.capacitance.stride,intermediate.inductance.stride,padbed?reinterpret_cast<const double*>(intermediate.pads.data):nullptr,intermediate.pads.stride,shunts);
            if(!calculated)return "{\"error\":\"Common-mode shunt solve failed\"}";
            combined.pi=extract_pi_equivalent_with_shunts(y,frequency,{shunts.first_real,shunts.first_imag},{shunts.second_real,shunts.second_imag});
            const double series[]={-y.y21.real,-y.y21.imag};
            if(!reasitic::pi_branch_impedances(series,shunts,branch_impedances,combined.pi.quality_differential))return "{\"error\":\"Pi branch impedance evaluation failed\"}";
        }
#endif
        const unsigned capacity=64u*(intermediate.capacitance.elements+intermediate.inductance.elements+intermediate.node_capacitance.elements+intermediate.currents.rows*intermediate.currents.columns+intermediate.voltages.rows*intermediate.voltages.columns+intermediate.pads.elements+intermediate.with_pads.elements)+16384u;
        output=static_cast<char*>(calloc(1,capacity));if(!output)return "{\"error\":\"Unable to allocate network output\"}";char* out=output;
        out+=sprintf(out,"{\"name\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(original_source));out+=sprintf(out,",\"ground\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(original_reference));
        if(padbed){out+=sprintf(out,",\"model\":\"PI4\",\"pads\":[");out=reasitic::json_string(out,reinterpret_cast<const char*>(pad1));out+=sprintf(out,",");out=reasitic::json_string(out,reinterpret_cast<const char*>(pad2));out+=sprintf(out,"],\"pad_contacts\":%d",pad_contacts);}
        if(transformer){out+=sprintf(out,",\"secondary\":");out=reasitic::json_string(out,reinterpret_cast<const char*>(original_secondary));out+=sprintf(out,",\"termination_ohm\":%.17g,\"boundary\":%d",network->termination,boundary);}
        if(engine.max_width_cells!=500)out+=sprintf(out,",\"max_width_cells\":%d",engine.max_width_cells);
        out+=sprintf(out,",\"frequency_hz\":%.17g,\"contacts\":%d,\"groups\":%d,\"filaments\":%d,\"mesh\":{\"length\":%.17g,\"width\":%.17g,\"thickness\":%.17g,\"contact_length\":%.17g,\"contact_width\":%.17g,\"chip_x\":%.17g,\"chip_y\":%.17g,\"fft_x\":%d,\"fft_y\":%d},\"matrix\":[[",frequency,padbed?count:snapshots.contacts,expected_groups,network_filaments,network->length,network->width,network->thickness,length_cell,width_cell,chip_x,chip_y,nx,ny);
        out=complex_json(out,y.y11);out+=sprintf(out,",");out=complex_json(out,y.y12);out+=sprintf(out,"],[");out=complex_json(out,y.y21);out+=sprintf(out,",");out=complex_json(out,y.y22);out+=sprintf(out,"]]");
        if(transformer){
            out+=sprintf(out,",\"transformer\":{");
            const char* names[]={"first_inductance_h","second_inductance_h","first_resistance_ohm","second_resistance_ohm","ground_resistance_ohm","mutual_inductance_h","coupling"};
            const double values[]={coupled.model.first_inductance,coupled.model.second_inductance,coupled.model.first_resistance,coupled.model.second_resistance,coupled.model.ground_resistance,coupled.model.mutual,coupled.model.coupling};
            for(int i=0;i<7;++i){out+=sprintf(out,"%s\"%s\":",i?",":"",names[i]);out=number_json(out,values[i]);}
            out+=sprintf(out,"},\"impedance_matrix\":[[");out=complex_json(out,coupled.impedance.z11);out+=sprintf(out,",");out=complex_json(out,coupled.impedance.z12);out+=sprintf(out,"],[");out=complex_json(out,coupled.impedance.z21);out+=sprintf(out,",");out=complex_json(out,coupled.impedance.z22);out+=sprintf(out,"]]");
        }else {
        out+=sprintf(out,",\"pi\":{");
        const char* names[]={"frequency_hz","inductance_h","resistance","capacitance_second_f","capacitance_first_f","resistance_second","resistance_first","quality_first","quality_second","quality_differential","shunt_single","shunt_differential"};
        const auto* values=reinterpret_cast<const double*>(&combined.pi);
        for(int i=0;i<12;++i){out+=sprintf(out,"%s\"%s\":",i?",":"",names[i]);out=number_json(out,values[i]);}
        out+=sprintf(out,"},\"resonance_hz\":");out=number_json(out,x87::binary64(pi_resonant_frequency(combined.pi)));
#ifdef __EMSCRIPTEN__
        if(stable_shunts)out+=sprintf(out,",\"pi_shunts\":{\"first\":[%.17g,%.17g],\"second\":[%.17g,%.17g],\"precision_bits\":128}",shunts.first_real,shunts.first_imag,shunts.second_real,shunts.second_imag);
#endif
        }
        out+=sprintf(out,",\"input_impedance\":[");
        for(int i=0;i<3;++i){if(i)out+=sprintf(out,",");
#ifdef __EMSCRIPTEN__
            if(stable_shunts)out=complex_json(out,{branch_impedances[2*i],branch_impedances[2*i+1]});else
#endif
            out=complex_json(out,two_port_impedance(y,i==2,i+1));
        }
        out+=sprintf(out,"],\"eddy\":");out=reasitic::eddy_metadata(out,*network->eddy);
        out+=sprintf(out,",\"capacitance_matrix\":");out=matrix_json(out,intermediate.capacitance);out+=sprintf(out,network->extended?",\"inductance_impedance_matrix\":":",\"inductance_admittance_matrix\":");out=matrix_json(out,intermediate.inductance);
        if(network->extended){out+=sprintf(out,",\"model\":\"PIX\",\"currents\":");out=matrix_json(out,intermediate.currents);out+=sprintf(out,",\"voltages\":");out=matrix_json(out,intermediate.voltages);}else {out+=sprintf(out,",\"node_capacitance_matrix\":");out=matrix_json(out,intermediate.node_capacitance);}
        if(padbed){out+=sprintf(out,",\"pad_capacitance_matrix\":");out=matrix_json(out,intermediate.pads);out+=sprintf(out,",\"with_pad_admittance_matrix\":");out=matrix_json(out,intermediate.with_pads);}
        double bank[9]{};
        if(transformer){const auto& m=coupled.model;bank[0]=m.first_inductance;bank[1]=m.second_inductance;bank[2]=m.coupling;bank[3]=m.ground_resistance;bank[4]=m.first_resistance;bank[5]=m.second_resistance;bank[8]=m.mutual;}
        else __builtin_memcpy(bank,&combined.pi.inductance,sizeof(bank));
        out+=sprintf(out,",\"two_port\":");out=reasitic::two_port_json(out,y,bank,transformer,frequency);
        sprintf(out,",\"green_table_hash\":\"%016llx\"}",table_hash(transformer?coupled.capacitance.cache:combined.capacitance.cache,nx,ny,pairs));return output;
    }
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

extern "C" int max_contact_count(){return max_contacts;}
extern "C" int potential_grid_fits(int nx,int ny){return reasitic::potential_grid_fits(nx,ny,reasitic::engine_view().metal_count,reasitic::reference_numerics())?1:0;}
extern "C" const char* simulate_capacitance(int winding,int ground,double frequency,double length_cell,double width_cell,double chip_x,double chip_y,int nx,int ny,int contact_limit,int unfolded) {
    return simulate_capacitance_impl(winding,ground,frequency,length_cell,width_cell,chip_x,chip_y,nx,ny,contact_limit,unfolded,nullptr);
}
extern "C" const char* simulate_network(int winding,int ground,double frequency,double length_cell,double width_cell,double thickness_cell,double contact_length,double contact_width,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int filament_limit,int mode,int count,int first,int second,int budget) {
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate network eddy workspace\"}";
    reasitic::EddyScope scope(session);reasitic::clear_diagnostics();
    if(!reasitic::configure_eddy(*session,reasitic::engine_view(),mode,count,first,second,budget))return "{\"error\":\"Invalid network eddy mode, layers or evaluation budget\"}";
    const NetworkRequest request{length_cell,width_cell,thickness_cell,filament_limit,session};
    return simulate_capacitance_impl(winding,ground,frequency,contact_length,contact_width,chip_x,chip_y,nx,ny,contact_limit,0,&request);
}

extern "C" const char* simulate_extended_network(int winding,int ground,double frequency,double length_cell,double width_cell,double thickness_cell,double contact_length,double contact_width,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int filament_limit,int mode,int count,int first,int second,int budget) {
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate network eddy workspace\"}";
    reasitic::EddyScope scope(session);reasitic::clear_diagnostics();
    if(!reasitic::configure_eddy(*session,reasitic::engine_view(),mode,count,first,second,budget))return "{\"error\":\"Invalid network eddy mode, layers or evaluation budget\"}";
    NetworkRequest request{length_cell,width_cell,thickness_cell,filament_limit,session};request.extended=true;
    return simulate_capacitance_impl(winding,ground,frequency,contact_length,contact_width,chip_x,chip_y,nx,ny,contact_limit,0,&request);
}

extern "C" const char* simulate_pad_network(int winding,int first_pad,int second_pad,int ground,double frequency,double length_cell,double width_cell,double thickness_cell,double contact_length,double contact_width,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int filament_limit,int mode,int count,int first,int second,int budget) {
    if(first_pad<0 || second_pad<0)return "{\"error\":\"Select valid pad records\"}";
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate network eddy workspace\"}";
    reasitic::EddyScope scope(session);reasitic::clear_diagnostics();
    if(!reasitic::configure_eddy(*session,reasitic::engine_view(),mode,count,first,second,budget))return "{\"error\":\"Invalid network eddy mode, layers or evaluation budget\"}";
    NetworkRequest request{length_cell,width_cell,thickness_cell,filament_limit,session};request.first_pad=first_pad;request.second_pad=second_pad;
    return simulate_capacitance_impl(winding,ground,frequency,contact_length,contact_width,chip_x,chip_y,nx,ny,contact_limit,0,&request);
}

extern "C" const char* simulate_transformer(int primary,int secondary,int ground,double frequency,double length_cell,double width_cell,double thickness_cell,double contact_length,double contact_width,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int filament_limit,int mode,int count,int first,int second,double termination,int budget) {
    if(secondary<0)return "{\"error\":\"Select a valid secondary winding\"}";
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate transformer eddy workspace\"}";
    reasitic::EddyScope scope(session);reasitic::clear_diagnostics();
    if(!reasitic::configure_eddy(*session,reasitic::engine_view(),mode,count,first,second,budget))return "{\"error\":\"Invalid transformer eddy mode, layers or evaluation budget\"}";
    const NetworkRequest request{length_cell,width_cell,thickness_cell,filament_limit,session,secondary,termination};
    return simulate_capacitance_impl(primary,ground,frequency,contact_length,contact_width,chip_x,chip_y,nx,ny,contact_limit,0,&request);
}

extern "C" const char* simulate_three_port(int first_winding,int second_winding,int third_winding,int ground,double frequency,double length_cell,double width_cell,double thickness_cell,double contact_length,double contact_width,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int filament_limit,int mode,int count,int first,int second,int budget) {
    if(second_winding<0 || third_winding<0)return "{\"error\":\"Select valid second and third windings\"}";
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate three-port eddy workspace\"}";
    reasitic::EddyScope scope(session);reasitic::clear_diagnostics();
    if(!reasitic::configure_eddy(*session,reasitic::engine_view(),mode,count,first,second,budget))return "{\"error\":\"Invalid three-port eddy mode, layers or evaluation budget\"}";
    const NetworkRequest request{length_cell,width_cell,thickness_cell,filament_limit,session,second_winding,0,third_winding};
    return simulate_capacitance_impl(first_winding,ground,frequency,contact_length,contact_width,chip_x,chip_y,nx,ny,contact_limit,0,&request);
}

extern "C" const char* probe_three_port(int first_winding,int second_winding,int third_winding,int ground,double frequency,double length_cell,double width_cell,double thickness_cell,double contact_length,double contact_width,
    double chip_x,double chip_y,int nx,int ny,int contact_limit,int filament_limit,int mode,int count,int first,int second,int budget,const ComplexValue* cap_history,const ComplexValue* ind_history,int history_cells,const unsigned* history_addresses) {
    reasitic::ReferenceNumericsScope reference_arithmetic;
    if(second_winding<0 || third_winding<0 || !cap_history || !ind_history || !history_addresses || history_cells<1 || history_cells>512*512)return "{\"error\":\"Select valid windings and captured three-port backing arrays\"}";
    auto* session=static_cast<reasitic::EddySession*>(calloc(1,sizeof(reasitic::EddySession)));if(!session)return "{\"error\":\"Unable to allocate three-port eddy workspace\"}";
    reasitic::EddyScope scope(session);reasitic::clear_diagnostics();if(!reasitic::configure_eddy(*session,reasitic::engine_view(),mode,count,first,second,budget))return "{\"error\":\"Invalid three-port eddy configuration\"}";
    const NetworkRequest request{length_cell,width_cell,thickness_cell,filament_limit,session,second_winding,0,third_winding,cap_history,ind_history,history_cells,history_addresses};
    return simulate_capacitance_impl(first_winding,ground,frequency,contact_length,contact_width,chip_x,chip_y,nx,ny,contact_limit,0,&request);
}

extern "C" void set_potential_memo(int enabled){
    potential_memo_enabled=enabled!=0;
    if(!potential_memo_enabled){for(auto& entry:potential_memos){free(entry.data);entry={};}potential_memo_hits=potential_memo_misses=potential_memo_writes=0;}
}
extern "C" const char* potential_memo_stats(){
    static char data[256];unsigned bytes=0;for(const auto& entry:potential_memos)bytes+=entry.size;
    sprintf(data,"{\"enabled\":%s,\"hits\":%u,\"misses\":%u,\"writes\":%u,\"bytes\":%u}",potential_memo_enabled?"true":"false",potential_memo_hits,potential_memo_misses,potential_memo_writes,bytes);return data;
}
