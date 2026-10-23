#include "asitic/potential_solver.hpp"
#include "asitic/legacy_complex.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
bool same_name(const ShapeFilamentList& a,const ShapeFilamentList& b) {
    const auto* x=a.prefix;const auto* y=b.prefix;while(*x && *x==*y) { ++x;++y; }return *x==*y;
}
ComplexValue* vector(int n,SolverMemory memory,WarningSink warnings) {
    auto* p=static_cast<ComplexValue*>(memory.arrays.get(static_cast<unsigned>(n)*16u));
    for(int i=0;i<n;++i)p[i]={0,0};
    if(!p)warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    return p;
}
ComplexMatrix matrix(int n,SolverMemory memory,WarningSink warnings) {
    return {vector(n*n,memory,warnings),n*n,0,n,n,n,0};
}
void cleanup(void* user) {
    auto* pair=static_cast<void**>(user);
    release_potential_contacts(*static_cast<PotentialContactGroup**>(pair[0]),*static_cast<SolverMemory*>(pair[1]));
}
void assign_principal(ComplexMatrix& out,const ComplexMatrix& source,int n,SolverMemory memory,WarningSink warnings) {
    if(out.matrix_reference) {
        if(out.rows!=n || out.columns!=n) { warnings.emit("\nMV_ColMatRef::operator=  non-conformant assignment.");return; }
    } else {
        if(out.vector_reference)warnings.emit("MV_Vector::newsize can't operator on references.");
        else if(out.elements!=n*n) { memory.put(out.data);out.data=vector(n*n,memory,warnings);if(out.data)out.elements=n*n; }
        out.rows=out.columns=out.stride=n;
    }
    for(int j=0;j<n;++j)for(int i=0;i<n;++i)out.view().at(i,j)=source.view().at(i,j);
}
bool solve_capacitance_impl(const ShapeFilamentList& first,const ShapeFilamentList& second,
    ComplexMatrix& output,double frequency,PotentialCapacitanceState& state,
    const PotentialCapacitanceOptions& o,SolverMemory memory,bool unfolded) {
    const auto warnings=o.runtime.warnings;char message[512];
    if(o.progress) { o.runtime.files.format(message,"Performing Analysis at %2.2lf GHz",binary64(static_cast<long double>(frequency)/1e9));warnings.emit(message); }
    if(!prepare_potential_contacts(first,second,state.groups,o.contacts,memory,o.contact_history,warnings)) {
        release_potential_contacts(state.groups,memory);warnings.emit("Error:  Errors while attempting to generate subcontacts.");return false;
    }
    int count=0,groups=0;
    for(auto* g=state.groups;g;g=g->next) { ++groups;for(auto* c=g->first;c;c=reinterpret_cast<PotentialContact*>(c->next_address))++count; }
    if(o.progress) { o.runtime.files.format(message,"Generating capacitance matrix (%dx%d).",count,count);warnings.emit(message); }
    const long double weight=((static_cast<long double>(frequency)*0x1.921fb54442d18p+2)*0x1.8ebfab864ade4p-44)*0x1.a36e2eb1c432dp-14;
    for(int i=0;i<o.technology.substrate_count;++i) {
        double eps;__builtin_memcpy(&eps,o.technology.substrate[i].prefix+4,8);
        state.coefficients[i]=legacy::divide(1.,{binary64(1.L/o.technology.substrate[i].resistivity),binary64(weight*eps)});
    }
    const PotentialTableGeometry geometry{o.contacts.nx,o.contacts.ny,o.contacts.width,o.contacts.height,
        o.technology.metals,{state.coefficients,o.depths},o.technology.substrate_count};
    if(!load_potential_cache(state.cache,frequency,geometry.nx,geometry.ny,o.technology,memory,o.runtime,o.cache_header_history))
        generate_potential_cache(state.cache,frequency,true,geometry,o.technology,memory,o.runtime);
    auto full=matrix(count,memory,warnings);
    // Walk the parallel native banks directly, preserving the allocator call
    // stream while selecting the same packed table for each contact pair.
    const auto cache=state.cache;
    int k=0;
    for(auto* g=state.groups;g;g=g->next)for(auto* c=g->first;c;c=reinterpret_cast<PotentialContact*>(c->next_address))c->index=k++;
    for(auto* g=state.groups;g;g=g->next)for(auto* a=g->first;a;a=reinterpret_cast<PotentialContact*>(a->next_address))
        for(auto* h=state.groups;h;h=h->next)for(auto* b=h->first;b;b=reinterpret_cast<PotentialContact*>(b->next_address)) {
            const int lo=a->metal<b->metal?a->metal:b->metal,hi=a->metal<b->metal?b->metal:a->metal;
            const int pair=x87::truncate_int((static_cast<long double>(o.technology.metal_count)-static_cast<long double>(lo-1)*0.5L)*lo+hi-lo);
            full.view().at(a->index,b->index)=contact_potential(*a,*b,geometry.view(),{cache.x[pair],cache.y[pair],cache.xy[pair]});
        }
    if(o.snapshot)o.snapshot("Pmat.out",full,o.snapshot_user);
    if(o.progress)o.runtime.status.emit(".");
    void* cleanup_state[]={&state.groups,&memory};
    invert_general_allocated(full,memory,{warnings,cleanup,cleanup_state});
    int second_count=0;
    if(!same_name(first,second))for(auto* s=second.first;s;s=reinterpret_cast<const ConductorSegment*>(s->next_address))++second_count;
    if(!unfolded && o.verbose) {
        ComplexValue sum{0,0};
        for(int i=0;i<full.rows-second_count;++i)for(int j=0;j<full.rows-second_count;++j) {
            const auto z=full.view().at(i,j);sum.real=binary64(static_cast<long double>(sum.real)+z.real);sum.imag=binary64(static_cast<long double>(sum.imag)+z.imag);
        }
        const auto z=legacy::divide(1.,sum);
        o.runtime.files.format(message,"At %5.3lf GHz:",binary64(static_cast<long double>(frequency)*0x1.12e0be826d695p-30));warnings.emit(message);
        const double capacitance=binary64(-1.L/((static_cast<long double>(frequency)*0x1.921fb54442d18p+2)*z.imag));
        o.runtime.files.format(message,"Total Capacitance = %5.3lf (fF)",binary64(static_cast<long double>(capacitance)*1e15));warnings.emit(message);
        o.runtime.files.format(message,"Total Resistance = %5.3lf",z.real);warnings.emit(message);
    }
    auto reduced=matrix(groups,memory,warnings);int i=0;
    for(auto* a=state.groups;a;a=a->next,++i) {
        a->reserved=i;int j=0;
        for(auto* b=state.groups;b;b=b->next,++j) {
            ComplexValue sum{0,0};
            for(auto* ca=a->first;ca;ca=reinterpret_cast<PotentialContact*>(ca->next_address))
                for(auto* cb=b->first;cb;cb=reinterpret_cast<PotentialContact*>(cb->next_address)) {
                    const auto z=full.view().at(ca->index,cb->index);sum.real=binary64(static_cast<long double>(sum.real)+z.real);sum.imag=binary64(static_cast<long double>(sum.imag)+z.imag);
                }
            reduced.view().at(i,j)=sum;
        }
    }
    if(unfolded) {
        assign_principal(output,reduced,groups-second_count,memory,warnings);
        if(o.snapshot)o.snapshot("YC.out",output,o.snapshot_user);
        if(o.progress)o.runtime.status.emit(".");
        release_potential_contacts(state.groups,memory);memory.put(reduced.data);memory.put(full.data);return true;
    }
    auto result=matrix(groups,memory,warnings);
    for(int row=0;row<groups;++row)for(int col=0;col<groups;++col) {
        auto& out=result.view().at(row,col);
        if(row==col) {
            out={0,0};for(int k=0;k<groups;++k) { const auto z=reduced.view().at(row,k);out.real=binary64(static_cast<long double>(out.real)+z.real);out.imag=binary64(static_cast<long double>(out.imag)+z.imag); }
        } else { const auto z=reduced.view().at(row,col);out={-z.real,-z.imag}; }
    }
    if(!same_name(first,second))for(int row=0;row<groups-1;++row)for(int k=1;k<=second_count;++k) {
        auto& out=result.view().at(row,row);const auto z=result.view().at(row,groups-k);
        out.real=binary64(static_cast<long double>(out.real)+z.real);out.imag=binary64(static_cast<long double>(out.imag)+z.imag);
    }
    assign_principal(output,result,groups-second_count,memory,warnings);
    if(o.snapshot)o.snapshot("YC.out",output,o.snapshot_user);
    if(o.progress)o.runtime.status.emit(".");
    release_potential_contacts(state.groups,memory);memory.put(result.data);memory.put(reduced.data);memory.put(full.data);return true;
}
}
bool solve_capacitance(const ShapeFilamentList& first,const ShapeFilamentList& second,ComplexMatrix& output,double frequency,
    PotentialCapacitanceState& state,const PotentialCapacitanceOptions& options,SolverMemory memory) {
    return solve_capacitance_impl(first,second,output,frequency,state,options,memory,false);
}
bool solve_unfolded_capacitance(const ShapeFilamentList& first,const ShapeFilamentList& second,ComplexMatrix& output,double frequency,
    PotentialCapacitanceState& state,const PotentialCapacitanceOptions& options,SolverMemory memory) {
    return solve_capacitance_impl(first,second,output,frequency,state,options,memory,true);
}
}
