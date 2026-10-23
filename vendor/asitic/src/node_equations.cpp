#include "asitic/node_equations.hpp"
#include "asitic/legacy_complex.hpp"
#include "x87.hpp"

namespace asitic::lapack {
void swap_general_rows(int columns,ComplexMatrixView a,int first,int last,const int* pivots,int inc) {
    if(inc==0)return;
    int index=inc<0?1+(1-last)*inc:first;
    for(int i=inc<0?last:first;inc<0?i>=first:i<=last;i+=inc<0?-1:1,index+=inc) {
        const int p=pivots[index-1];
        if(p!=i)swap(columns,&a.at(i-1,0),a.stride,&a.at(p-1,0),a.stride);
    }
}
int solve_general(int n,int nrhs,ComplexMatrixView lu,const int* pivots,ComplexMatrixView rhs) {
    const int error=n<0?2:nrhs<0?3:lu.stride<(n>1?n:1)?5:rhs.stride<(n>1?n:1)?8:0;
    if(error) { argument_error("ZGETRS",error);return -error; }
    if(n==0 || nrhs==0)return 0;
    swap_general_rows(nrhs,rhs,1,n,pivots,1);
    const ComplexValue unit{1,0};
    solve_triangular(n,nrhs,unit,lu,false,false,true,rhs);
    solve_triangular(n,nrhs,unit,lu,false,true,false,rhs);
    return 0;
}
}
namespace asitic {
namespace {
using x87::binary64;
ComplexValue difference(ComplexValue a,ComplexValue b) {
    return {binary64(static_cast<long double>(a.real)-b.real),binary64(static_cast<long double>(a.imag)-b.imag)};
}
ComplexValue sum(ComplexValue a,ComplexValue b) {
    return {binary64(static_cast<long double>(a.real)+b.real),binary64(static_cast<long double>(a.imag)+b.imag)};
}
ComplexValue* zero_array(int n,SolverMemory memory) {
    auto* p=static_cast<ComplexValue*>(memory.arrays.get(static_cast<unsigned>(n)*16u));
    for(int i=0;i<n;++i)p[i]={0,0};
    return p;
}
}
void network_matrix_vector(const ComplexValue* matrix,const ComplexValue* x,ComplexValue* out,int n) {
    for(int i=0;i<n;++i) {
        out[i]={0,0};
        for(int j=0;j<n;++j) {
            const auto a=matrix[i*n+j],b=x[j];
            const long double real=static_cast<long double>(a.real)*b.real-static_cast<long double>(a.imag)*b.imag;
            const long double imag=static_cast<long double>(b.real)*a.imag+static_cast<long double>(a.real)*b.imag;
            out[i]={binary64(real+out[i].real),binary64(imag+out[i].imag)};
        }
    }
}
void add_network_vectors(const ComplexValue* a,const ComplexValue* b,ComplexValue* out,int n) {
    for(int i=0;i<n;++i)out[i]=sum(a[i],b[i]);
}
void subtract_network_vectors(const ComplexValue* a,const ComplexValue* b,ComplexValue* out,int n) {
    for(int i=0;i<n;++i)out[i]=difference(a[i],b[i]);
}
void add_network_matrices(const ComplexValue* a,const ComplexValue* b,ComplexValue* out,int n) {
    for(int i=0;i<n;++i)for(int j=0;j<n;++j)out[j*n+i]=sum(a[j*n+i],b[j*n+i]);
}
void difference_network_matrix(const ComplexValue* a,ComplexValue* out,int n) {
    for(int j=n-1;j>0;--j)for(int i=0;i<n;++i)out[j*n+i]=difference(a[j*n+i],a[(j-1)*n+i]);
    for(int i=0;i<n;++i)out[i]=a[i];
    for(int i=n-1;i>0;--i)for(int j=0;j<n;++j)out[j*n+i]=difference(out[j*n+i],out[j*n+i-1]);
}
void backward_network_difference(ComplexValue* a,int n) {
    for(int i=n-1;i>0;--i)a[i]=difference(a[i],a[i-1]);
}
void forward_network_difference(ComplexValue* a,int n) {
    for(int i=0;i<n-1;++i)a[i]=difference(a[i],a[i+1]);
}
bool factor_network_matrix(ComplexValue* a,int* pivots,int n,WarningSink warnings) {
    const int info=lapack::factor_general(n,n,{a,n},pivots);
    if(info)warnings.emit("Error:  Error in factoring matrix.");
    return info==0;
}
int solve_network_matrix(ComplexValue* a,ComplexValue* b,const int* pivots,int n,int nrhs,WarningSink warnings) {
    if(lapack::solve_general(n,nrhs,{a,n},pivots,{b,n}))warnings.emit("Error:  Error in back-solve routine.");
    return 0;
}
bool solve_node_equations(ComplexValue* cap,const ComplexValue* ind,int n,TwoPortAdmittance& output,
    SolverMemory memory,const NodeEquationOptions& o) {
    if(o.progress) { char message[128];o.format(message,"Solving node equations...(%dx%d)",n,n);o.warnings.emit(message); }
    auto* pivots=static_cast<int*>(memory.arrays.get(static_cast<unsigned>(n)*4u));
    auto* lu=zero_array(n*n,memory);auto* drive=zero_array(n,memory);auto* bias=zero_array(n,memory);
    auto* current=zero_array(n,memory);auto* rhs=zero_array(n,memory);
    const auto release=[&]() { memory.put(lu);memory.put(drive);memory.put(bias);memory.put(current);memory.put(rhs);memory.put(pivots); };
    if(!lu || !drive || !bias || !current || !rhs || !pivots) { o.warnings.emit("Error 401:  Out of memory.");release();return false; }
    for(int i=0;i<n;++i) {
        ComplexValue acc{0,0};
        for(int j=0;j<n;++j) { auto& v=cap[j*n+i];acc=sum(acc,v);if(i!=j)v={binary64(-static_cast<long double>(v.real)),binary64(-static_cast<long double>(v.imag))}; }
        cap[i*n+i]=acc;
    }
    if(o.snapshot)o.snapshot("Ycap.out",{cap,n},n,o.snapshot_user);
    difference_network_matrix(ind,lu,n);add_network_matrices(cap,lu,lu,n);factor_network_matrix(lu,pivots,n,o.warnings);
    for(int i=0;i<n;++i)bias[i]=drive[i]={0,0};
    drive[0]={1,0};bias[n-1]={0,0};
    const auto run=[&]() {
        network_matrix_vector(ind,bias,rhs,n);backward_network_difference(rhs,n);add_network_vectors(rhs,drive,rhs,n);
        solve_network_matrix(lu,rhs,pivots,n,1,o.warnings);const auto first=rhs[0];
        forward_network_difference(rhs,n);subtract_network_vectors(rhs,bias,rhs,n);network_matrix_vector(ind,rhs,current,n);
        return first;
    };
    const auto a=run();const ComplexValue c{binary64(-static_cast<long double>(current[n-1].real)),binary64(-static_cast<long double>(current[n-1].imag))};
    drive[0]={0,0};bias[n-1]={1,0};
    const auto b=run();const ComplexValue d{binary64(-static_cast<long double>(current[n-1].real)),binary64(-static_cast<long double>(current[n-1].imag))};
    release();output.y22=legacy::divide(1.,a);
    const long double real=(static_cast<long double>(a.real)*d.real-static_cast<long double>(a.imag)*d.imag)-
        (static_cast<long double>(c.real)*b.real-static_cast<long double>(c.imag)*b.imag);
    const long double imag=(static_cast<long double>(d.real)*a.imag+static_cast<long double>(a.real)*d.imag)-
        (static_cast<long double>(b.real)*c.imag+static_cast<long double>(c.real)*b.imag);
    output.y11=legacy::divide(ComplexValue{binary64(real),binary64(imag)},a);
    output.y12=output.y21=legacy::divide(ComplexValue{binary64(-static_cast<long double>(b.real)),binary64(-static_cast<long double>(b.imag))},a);
    return true;
}
}
