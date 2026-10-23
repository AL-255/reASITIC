#include "asitic/potential_cache.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
int add(int a,int b) { return static_cast<int>(static_cast<unsigned>(a)+static_cast<unsigned>(b)); }
int mul(int a,int b) { return static_cast<int>(static_cast<unsigned>(a)*static_cast<unsigned>(b)); }
int pair_index(int i,int j,int m) {
    const double half=binary64(static_cast<long double>(add(i,-1))*0.5L);
    return x87::truncate_int((static_cast<long double>(m)-half)*i+add(j,-i));
}
ComplexValue* zero_array(int n,SolverMemory memory) {
    auto* data=static_cast<ComplexValue*>(memory.arrays.get(static_cast<unsigned>(n)*16u));
    for(int i=0;i<n;++i)data[i]={0,0};
    return data;
}
bool same_text(const char* a,const char* b) { while(*a && *a==*b) { ++a;++b; }return *a==*b; }
template<class T>T field(const void* p,unsigned offset) {
    T value;__builtin_memcpy(&value,static_cast<const unsigned char*>(p)+offset,sizeof(T));return value;
}
void path(char* out,double frequency,const PotentialCacheRuntime& runtime) {
    runtime.files.format(out,"%s/%s_%2.2lf.dat",runtime.directory,runtime.technology_name,
        binary64(static_cast<long double>(frequency)/1e9));
}
}
bool allocate_potential_cache(PotentialCache& c,int nx,int ny,int m,SolverMemory memory,WarningSink warnings) {
    const int count=add(mul(m,m),m)/2;
    c.xy=static_cast<ComplexValue**>(memory.arrays.get(static_cast<unsigned>(count)*4u));
    c.x=static_cast<ComplexValue**>(memory.arrays.get(static_cast<unsigned>(count)*4u));
    c.y=static_cast<ComplexValue**>(memory.arrays.get(static_cast<unsigned>(count)*4u));
    if(!c.xy || !c.x || !c.y) {
        memory.put(c.xy);memory.put(c.x);memory.put(c.y);
        warnings.emit("Error:  Not enough memory.");return false;
    }
    const int cells=mul(add(nx,1),add(ny,1));
    for(int i=0;i<m;++i)for(int j=i;j<m;++j) {
        const int k=pair_index(i,j,m);
        c.xy[k]=zero_array(cells,memory);c.x[k]=zero_array(mul(nx,2),memory);c.y[k]=zero_array(mul(ny,2),memory);
        if(!c.xy[k] || !c.x[k] || !c.y[k]) { warnings.emit("Error:  Not enough memory.");return false; }
    }
    return true;
}
void release_potential_cache(const PotentialCache& c,int m,SolverMemory memory) {
    for(int i=m-1;i>=0;--i)for(int j=m-1;j>=i;--j) {
        const int k=pair_index(i,j,m);memory.put(c.xy[k]);memory.put(c.x[k]);memory.put(c.y[k]);
    }
    memory.put(c.xy);memory.put(c.x);memory.put(c.y);
}
bool validate_potential_cache_header(void* file,int nx,int ny,const PotentialCacheTechnology& t,
    SolverMemory memory,const PotentialCacheRuntime& r,PotentialCacheHeaderHistory h) {
    auto* substrate=static_cast<SubstrateLayer*>(memory.arrays.get(static_cast<unsigned>(t.substrate_count)*40u));
    auto* metals=static_cast<MetalLayer*>(memory.arrays.get(static_cast<unsigned>(t.metal_count)*236u));
    r.files.read(&h.nx,4,1,file,r.files.user);r.files.read(&h.ny,4,1,file,r.files.user);
    r.files.read(&h.frequency,8,1,file,r.files.user);
    r.files.read(substrate,40,static_cast<unsigned>(t.substrate_count),file,r.files.user);
    r.files.read(metals,236,static_cast<unsigned>(t.metal_count),file,r.files.user);
    bool result=true;char message[512];
    if(h.nx!=nx || h.ny!=ny) { r.warnings.emit("Error:  Data file FFT sizes do not match tek file.");result=false; }
    for(int i=0;i<t.substrate_count;++i) {
        const auto* a=t.substrate+i;const auto* b=substrate+i;
        const int layer=field<int>(a,0);
        if(layer!=field<int>(b,0)) {
            r.files.format(message,"Error:  Layer %d does not match in data file.",layer);r.warnings.emit(message);result=false;
        }
        const char* formats[]={"Error:  Layer eps=%lf does not match the data file.",
            "Error:  Layer rho=%lf does not match the data file.","Error:  Layer dz=%lf does not match the data file."};
        const unsigned offsets[]={4,12,20};
        for(unsigned k=0;k<3;++k)if(!(field<double>(b,offsets[k])==field<double>(a,offsets[k]))) {
            r.files.format(message,formats[k],binary64(field<double>(a,offsets[k])));r.warnings.emit(message);result=false;
        }
    }
    for(int i=0;i<t.metal_count;++i) {
        const auto* a=t.metals+i;const auto* b=metals+i;
        const auto* name=reinterpret_cast<const char*>(a);const auto* color=name+80;
        if(!same_text(name,reinterpret_cast<const char*>(b))) {
            r.files.format(message,"Warning:  Metal name <%s> does not match the data file.",name);r.warnings.emit(message);
        }
        if(!same_text(color,reinterpret_cast<const char*>(b)+80)) {
            r.files.format(message,"Warning:  Metal color <%s> does not match the data file.",color);r.warnings.emit(message);
        }
        if(field<int>(a,160)!=field<int>(b,160)) {
            r.files.format(message,"Warning:  Metal color <%s> does not match the data file.",color);r.warnings.emit(message);result=false;
        }
        if(field<int>(a,164)!=field<int>(b,164)) {
            r.files.format(message,"Error:  Metal layer %d does not match the data file.",field<int>(a,164));r.warnings.emit(message);result=false;
        }
        const char* formats[]={"Error:  Metal coordinate mz=%lf does not match the data file.",
            "Error:  Metal thickness %lf does not match the data file.","Error:  Metal sheet resistance %lf does not match the data file."};
        const unsigned offsets[]={168,176,184};
        for(unsigned k=0;k<3;++k)if(!(field<double>(b,offsets[k])==field<double>(a,offsets[k]))) {
            r.files.format(message,formats[k],binary64(field<double>(a,offsets[k])));r.warnings.emit(message);result=false;
        }
    }
    memory.put(substrate);memory.put(metals);return result;
}
bool generate_potential_cache(PotentialCache& c,double frequency,bool verbose,const PotentialTableGeometry& g,
    const PotentialCacheTechnology& t,SolverMemory memory,const PotentialCacheRuntime& r) {
    char message[512];
    if(verbose) { r.files.format(message,"\n\nGenerating data at %2.2lf GHz...",binary64(static_cast<long double>(frequency)/1e9));r.warnings.emit(message); }
    r.warnings.emit("");
    for(int i=0;i<t.metal_count;++i)for(int j=i;j<t.metal_count;++j) {
        const int k=pair_index(i,j,t.metal_count);
        generate_potential_tables(i,j,g,{c.x[k],c.y[k],c.xy[k]},memory,r.status);
    }
    if(!c.enabled)return true;
    path(message,frequency,r);void* file=r.files.open(message,"w",r.files.user);
    if(!file) { r.warnings.emit("Error:  Unable to save data file!");return false; }
    r.files.write(&g.nx,4,1,file,r.files.user);r.files.write(&g.ny,4,1,file,r.files.user);
    r.files.write(&frequency,8,1,file,r.files.user);
    r.files.write(t.substrate,40,static_cast<unsigned>(t.substrate_count),file,r.files.user);
    r.files.write(t.metals,236,static_cast<unsigned>(t.metal_count),file,r.files.user);
    if(verbose)r.warnings.emit("Writing data...");
    for(int i=0;i<t.metal_count;++i)for(int j=i;j<t.metal_count;++j) {
        const int k=pair_index(i,j,t.metal_count);
        r.files.write(c.xy[k],16,static_cast<unsigned>(mul(add(g.nx,1),add(g.ny,1))),file,r.files.user);
        r.files.write(c.x[k],16,static_cast<unsigned>(mul(g.nx,2)),file,r.files.user);
        r.files.write(c.y[k],16,static_cast<unsigned>(mul(g.ny,2)),file,r.files.user);
    }
    r.files.close(file,r.files.user);c.frequency=binary64(frequency);return true;
}
bool load_potential_cache(PotentialCache& c,double frequency,int nx,int ny,const PotentialCacheTechnology& t,
    SolverMemory memory,const PotentialCacheRuntime& r,PotentialCacheHeaderHistory history) {
    if(!c.enabled)return false;
    if(frequency==c.frequency && !c.dirty)return true;
    c.dirty=false;char message[512];path(message,frequency,r);
    void* file=r.files.open(message,"r",r.files.user);
    if(!file) {
        r.files.format(message,"Warning:  Could not open data file at frequency = %3.2lf.",binary64(static_cast<long double>(frequency)/1e9));r.warnings.emit(message);
        r.files.format(message,"Tried using %s as path.",r.directory);r.warnings.emit(message);return false;
    }
    if(!validate_potential_cache_header(file,nx,ny,t,memory,r,history)) {
        r.files.format(message,"Error:  Data file at frequency = %3.2lf is not consistent with tek file.",binary64(static_cast<long double>(frequency)/1e9));r.warnings.emit(message);return false;
    }
    for(int i=0;i<t.metal_count;++i)for(int j=i;j<t.metal_count;++j) {
        const int k=pair_index(i,j,t.metal_count);
        r.files.read(c.xy[k],16,static_cast<unsigned>(mul(add(nx,1),add(ny,1))),file,r.files.user);
        r.files.read(c.x[k],16,static_cast<unsigned>(mul(nx,2)),file,r.files.user);
        r.files.read(c.y[k],16,static_cast<unsigned>(mul(ny,2)),file,r.files.user);
    }
    r.files.close(file,r.files.user);c.frequency=binary64(frequency);return true;
}
}
