#include "asitic/extended_node.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& x) { long double r;asm volatile("fldl %1":"=t"(r):"m"(x));return r; }
long double add_memory(const double& a,const double& b) { long double r;asm volatile("fldl %1; faddl %2":"=t"(r):"m"(a),"m"(b));return r; }
void raw(ComplexValue& out,const ComplexValue& in) { __builtin_memcpy(&out,&in,sizeof(out)); }
ComplexMatrix zeros(int rows,int columns,SolverMemory memory,WarningSink warnings,const char* error) {
    const int count=static_cast<int>(static_cast<unsigned>(rows)*static_cast<unsigned>(columns));
    auto* data=static_cast<ComplexValue*>(memory.arrays.get(static_cast<unsigned>(count)*16u));
    for(int i=0;i<count;++i)data[i]={0.,0.};
    if(!data)warnings.emit(error);
    return {data,count,0,rows,columns,rows,0};
}
ComplexMatrix slice(ComplexMatrix& m,int first_row,int first_column,int rows,int columns) {
    return {m.data+first_column*m.stride+first_row,m.stride*columns,1,rows,columns,m.stride,1};
}
void assign(ComplexMatrix& out,const ComplexMatrix& in,WarningSink warnings) {
    if(in.rows==0 && in.columns==1) {
        warnings.emit("MV_VecIndex: (0:-1) too big for matrix (0:-1)");
        warnings.emit("MV_VecIndex: (0:-1) too big for matrix (0:1)");
        warnings.emit("MV_VectorRef::operator=  non-conformant assignment.");
        return;
    }
    for(int j=0;j<in.columns;++j)for(int i=0;i<in.rows;++i)raw(out.view().at(i,j),in.view().at(i,j));
}
constexpr const char* vector_error="Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ";
}
void stamp_extended_node_columns(const ComplexMatrix& in,ComplexMatrix& out) {
    const int n=in.rows;
    for(int i=0;i<n;++i) {
        auto& first=out.view().at(i,0);const auto& a=in.data[i];
        first={x87::binary64(load(a.real)*-0.5L),x87::binary64(load(a.imag)*-0.5L)};
        for(int j=1;j<n;++j) {
            const auto& before=in.data[(j-1)*in.stride+i];const auto& current=in.data[j*in.stride+i];
            const long double real=add_memory(before.real,current.real),imag=add_memory(before.imag,current.imag);
            volatile double dead_real=real,dead_imag=imag;(void)dead_real;(void)dead_imag;
            out.view().at(i,j)={x87::binary64(real*-0.5L),x87::binary64(imag*-0.5L)};
        }
        const auto& last=in.data[(n-1)*in.stride+i];out.view().at(i,n)={x87::binary64(load(last.real)*-0.5L),x87::binary64(load(last.imag)*-0.5L)};
    }
}
void solve_extended_node_equations(const ComplexMatrix& cap,const ComplexMatrix& ind,TwoPortAdmittance& output,SolverMemory memory,const ExtendedNodeOptions& o) {
    const int n=cap.rows,m=static_cast<int>(static_cast<unsigned>(n)*2u+2u),width=static_cast<int>(static_cast<unsigned>(n)+1u);
    auto system=zeros(m,m,memory,o.warnings,vector_error);
    const auto index_assert=[&]() {
        if(o.assertion)o.assertion("i1 <= i2","Index.h",59,"MV_VecIndex::MV_VecIndex(unsigned int, unsigned int)",o.user);
        __builtin_trap();
    };
    if(static_cast<unsigned>(n)>static_cast<unsigned>(static_cast<unsigned>(n)*2u-1u))index_assert();
    if(static_cast<unsigned>(width)>static_cast<unsigned>(static_cast<unsigned>(n)*2u+1u))index_assert();
    auto a=slice(system,0,0,n,width);stamp_extended_node_columns(cap,a);
    a=slice(system,n,n+1,n,width);stamp_extended_node_columns(ind,a);
    auto d=zeros(n,width,memory,o.warnings,vector_error);
    for(int i=0;i<n;++i) { d.view().at(i,i)={1.,0.};d.view().at(i,i+1)={-1.,0.}; }
    auto boundary=zeros(2,width,memory,o.warnings,vector_error);
    boundary.view().at(0,0)={1.,0.};boundary.view().at(1,n)={1.,0.};
    a=slice(system,0,n+1,n,width);assign(a,d,o.warnings);
    a=slice(system,n,0,n,width);assign(a,d,o.warnings);
    a=slice(system,2*n,0,2,width);assign(a,boundary,o.warnings);
    auto zero=zeros(2,width,memory,o.warnings,vector_error);
    a=slice(system,2*n,n+1,2,width);assign(a,zero,o.warnings);memory.put(zero.data);
    auto* pivots=static_cast<int*>(memory.arrays.get(static_cast<unsigned>(m)*4u));
    if(!pivots)o.warnings.emit(vector_error);
    factor_network_matrix(system.data,pivots,m,o.warnings);
    auto rhs=zeros(m,2,memory,o.warnings,vector_error);
    rhs.view().at(2*n,0)={1.,0.};rhs.view().at(2*n+1,1)={1.,0.};
    solve_network_matrix(system.data,rhs.data,pivots,m,2,o.warnings);
    raw(output.y22,rhs.view().at(n+1,0));const auto& b=rhs.data[2*n+1];output.y12={x87::binary64(-load(b.real)),x87::binary64(-load(b.imag))};
    raw(output.y21,rhs.view().at(n+1,1));const auto& c=rhs.data[rhs.stride+2*n+1];output.y11={x87::binary64(-load(c.real)),x87::binary64(-load(c.imag))};
    if(o.dump) {
        char buffer[128];char* name=o.message?o.message:buffer;
        const double frequency=x87::binary64(load(o.frequency)*0x1.12e0be826d695p-30L);
        o.format(name,"current_%5.3lf.out",frequency);a=slice(rhs,n+1,0,n+1,1);if(o.snapshot)o.snapshot(name,a,o.user);
        o.format(name,"voltages_%5.3lf.out",frequency);a=slice(rhs,0,0,n+1,1);if(o.snapshot)o.snapshot(name,a,o.user);
    }
    memory.put(rhs.data);memory.put(pivots);memory.put(boundary.data);memory.put(d.data);memory.put(system.data);
}
}
