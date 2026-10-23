#include "asitic/three_port.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
ComplexValue* zero_array(int n,SolverMemory memory) {
    auto* p=static_cast<ComplexValue*>(memory.arrays.get(static_cast<unsigned>(n)*16u));for(int i=0;i<n;++i)p[i]={0,0};return p;
}
ComplexValue negative(ComplexValue z) { return {binary64(-static_cast<long double>(z.real)),binary64(-static_cast<long double>(z.imag))}; }
void floating_copy(double& out,const double& in) { asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st","memory"); }
long double floating_load(const double& in) { long double out;asm volatile("fldl %1" : "=t"(out) : "m"(in));return out; }
void raw_copy(ComplexValue& out,const ComplexValue& in) { __builtin_memcpy(&out,&in,sizeof(out)); }
}
bool solve_three_port_equations(ComplexValue* cap,const ComplexValue* ind,int first,int second,int n,
    ThreePortImpedance& output,SolverMemory memory,const NodeEquationOptions& o) {
    const int dim=static_cast<int>(static_cast<unsigned>(n)*2u);
    if(o.progress) { char message[128];o.format(message,"Solving 3-port equations...(%dx%d)",n,n);o.warnings.emit(message); }
    auto* pivots=static_cast<int*>(memory.arrays.get(static_cast<unsigned>(n)*8u));
    auto* lu=zero_array(dim*dim,memory);auto* rhs=zero_array(dim*3,memory);
    if(!lu || !pivots || !rhs) { o.warnings.emit("Error:  Out of memory.");memory.put(lu);memory.put(pivots);memory.put(rhs);return false; }
    for(int i=0;i<n;++i) {
        ComplexValue sum{0,0};
        for(int j=0;j<n;++j) {
            auto& z=cap[j*n+i];sum.real=binary64(static_cast<long double>(sum.real)+z.real);sum.imag=binary64(static_cast<long double>(sum.imag)+z.imag);
            if(i!=j)z=negative(z);
        }
        cap[i*n+i]=sum;
    }
    for(int i=0;i<dim;++i)for(int j=0;j<dim;++j) {
        auto& z=lu[j*dim+i];
        if(i<n) { if(j<n)z=i==j?ComplexValue{1,0}:j==i+1?ComplexValue{-1,0}:ComplexValue{0,0};else z=negative(ind[(j-n)*n+i]); }
        else { if(j<n)z=negative(cap[j*n+i-n]);else z=i==j?ComplexValue{-1,0}:j==i-1?ComplexValue{1,0}:ComplexValue{0,0}; }
    }
    if(o.snapshot)o.snapshot("Abigp.out",{lu,dim},dim,o.snapshot_user);
    lu[first*dim+first-1]={0,0};lu[(first+second)*dim+first+second-1]={0,0};
    lu[(first+n-1)*dim+first+n]={0,0};lu[(first+second+n-1)*dim+first+second+n]={0,0};
    if(o.snapshot)o.snapshot("Abig.out",{lu,dim},dim,o.snapshot_user);
    factor_network_matrix(lu,pivots,dim,o.warnings);
    for(int i=0;i<dim;++i)for(int j=0;j<3;++j)rhs[j*dim+i]={0,0};
    rhs[n]={-1,0};rhs[dim+n+first]={-1,0};rhs[2*dim+n+first+second]={-1,0};
    solve_network_matrix(lu,rhs,pivots,dim,3,o.warnings);
    raw_copy(output.z11,rhs[0]);raw_copy(output.z21,rhs[first]);raw_copy(output.z12,rhs[first]);
    raw_copy(output.z31,rhs[first+second]);raw_copy(output.z13,rhs[first+second]);raw_copy(output.z22,rhs[dim+first]);
    raw_copy(output.z32,rhs[dim+first+second]);raw_copy(output.z23,rhs[dim+first+second]);raw_copy(output.z33,rhs[2*dim+first+second]);
    memory.put(lu);memory.put(rhs);memory.put(pivots);return true;
}
ThreePortEquivalent extract_three_port_equivalent(const ThreePortImpedance& z,double frequency) {
    ThreePortEquivalent out;const long double omega=0x1.921fb54442d18p+2L*frequency;
    const long double l1=floating_load(z.z11.imag)/omega,l2=floating_load(z.z22.imag)/omega,l3=floating_load(z.z33.imag)/omega;
    const long double m12=floating_load(z.z12.imag)/omega,m13=floating_load(z.z13.imag)/omega,m23=floating_load(z.z23.imag)/omega;
    floating_copy(out.resistance[0],z.z11.real);floating_copy(out.resistance[1],z.z22.real);floating_copy(out.resistance[2],z.z33.real);
    floating_copy(out.cross_resistance[0],z.z12.real);floating_copy(out.cross_resistance[1],z.z13.real);floating_copy(out.cross_resistance[2],z.z23.real);
    out.inductance[0]=binary64(l1);out.inductance[1]=binary64(l2);out.inductance[2]=binary64(l3);
    out.mutual[0]=binary64(m12);out.mutual[1]=binary64(m13);out.mutual[2]=binary64(m23);
    out.coupling[0]=binary64(m12/x87::sqrt(l1*l2));out.coupling[1]=binary64(m13/x87::sqrt(l1*l3));out.coupling[2]=binary64(m23/x87::sqrt(l2*l3));return out;
}
}
