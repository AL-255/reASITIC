#include "asitic/three_port_transforms.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
long double floating_load(const double& value) { long double out;asm volatile("fldl %1" : "=t"(out) : "m"(value));return out; }
void raw_copy(ComplexValue& out,const ComplexValue& in) { __builtin_memcpy(&out,&in,sizeof(out)); }
ComplexMatrix matrix(SolverMemory memory,WarningSink warnings) {
    auto* data=static_cast<ComplexValue*>(memory.arrays.get(144));for(unsigned i=0;i<9;++i)data[i]={0,0};
    if(!data)warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    return {data,9,0,3,3,3,0};
}
void copy_input(ComplexMatrix& matrix,const ThreePortImpedance& input) {
    const auto* values=reinterpret_cast<const ComplexValue*>(&input);
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)raw_copy(matrix.view().at(i,j),values[3*i+j]);
}
void scatter(const ComplexMatrix& matrix,ComplexValue* output) {
    const int native_order[]={1,0,2};
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)raw_copy(output[3*i+j],matrix.view().at(native_order[i],native_order[j]));
}
void scaled_identity(ComplexMatrix& matrix,const ThreePortImpedance& input,bool plus) {
    constexpr long double scale=0x1.47ae147ae147bp-6L;
    const auto* values=reinterpret_cast<const ComplexValue*>(&input);
    for(int i=0;i<3;++i)for(int j=0;j<3;++j) {
        const auto& z=values[3*i+j];const long double real=floating_load(z.real)*scale,imag=floating_load(z.imag)*scale;
        // Native stores the product in a dead QWORD slot, then keeps its
        // extended register value while adding/subtracting the identity.
        volatile double discarded=binary64(real);(void)discarded;
        matrix.view().at(i,j)={binary64(i==j?(plus?1.L+real:real-1.L):real),binary64(imag)};
    }
}
}
void multiply_network_dense(const ComplexValue* first,const ComplexValue* second,ComplexValue* output,int n) {
    for(int i=0;i<n;++i)for(int j=0;j<n;++j) {
        double sum_real=0,sum_imag=0;
        for(int k=0;k<n;++k) {
            const auto& a=first[i*n+k];const auto& b=second[k*n+j];
            const long double ar=floating_load(a.real),br=floating_load(b.real),ai=floating_load(a.imag),bi=floating_load(b.imag);
            const long double real=ar*br-ai*bi,imag=bi*ar+ai*br;
            volatile double discarded=binary64(real);discarded=binary64(imag);(void)discarded;
            sum_real=binary64(real+sum_real);sum_imag=binary64(imag+sum_imag);
        }
        const ComplexValue result{sum_real,sum_imag};raw_copy(output[i*n+j],result);
    }
}
void convert_three_port_admittance(const ThreePortImpedance& input,ThreePortAdmittance& output,
    SolverMemory memory,lapack::GeneralInverseRuntime runtime) {
    auto inverse=matrix(memory,runtime.warnings);copy_input(inverse,input);invert_general_allocated(inverse,memory,runtime);
    scatter(inverse,reinterpret_cast<ComplexValue*>(&output));memory.put(inverse.data);
}
void convert_three_port_scattering(const ThreePortImpedance& input,ThreePortScattering& output,
    SolverMemory memory,lapack::GeneralInverseRuntime runtime) {
    auto plus=matrix(memory,runtime.warnings);auto minus=matrix(memory,runtime.warnings);auto result=matrix(memory,runtime.warnings);
    scaled_identity(plus,input,true);scaled_identity(minus,input,false);invert_general_allocated(plus,memory,runtime);
    multiply_network_dense(plus.data,minus.data,result.data,3);scatter(result,reinterpret_cast<ComplexValue*>(&output));
    memory.put(result.data);memory.put(minus.data);memory.put(plus.data);
}
}
