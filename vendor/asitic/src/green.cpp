#include "asitic/green.hpp"
#include "asitic/legacy_complex.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
constexpr double factor=0x1.08ef67ae4dd49p-17;
}
ComplexValue green_propagation_constant(double k,double frequency,double layer) {
    return legacy::square_root({binary64((long double)k*k+0.0L),
                                binary64((long double)factor*layer*frequency)});
}
ComplexValue green_complex_tanh(ComplexValue z) {
    const auto c=legacy::cosh(z),s=legacy::sinh(z);
    return legacy::divide(s,c);
}
long double green_reflection_imag(double k,double frequency,double layer1) {
    const auto g=green_propagation_constant(k,frequency,layer1);
    const ComplexValue a{binary64((long double)k-g.real),-g.imag},
                       b{binary64((long double)g.real+k),g.imag};
    return legacy::divide(a,b).imag;
}
long double green_oscillating_integrand(double k,double frequency,const GreenLayers& layers) {
    const auto a=green_propagation_constant(k,frequency,layers.layer1),
               b=green_propagation_constant(k,frequency,layers.layer2);
    const long double ar=a.real,ai=a.imag;
    const double sr=binary64(ar*ar-ai*ai),si=binary64((ar*ai)+(ar*ai));
    const auto t=green_complex_tanh({binary64((long double)layers.depth*ar),
                                    binary64((long double)layers.depth*ai)});
    const long double difference=(long double)k-b.real,negative_imag=-((long double)b.imag);
    // 0x0809395d rounds the real base; the imaginary base remains live in
    // an x87 register through 0x08093a2f despite an intervening FST QWORD.
    const long double base_i=ar*negative_imag+ai*difference;
    const double base_r=binary64(ar*difference-ai*negative_imag),
                 kr=binary64((long double)k*b.real),ki=binary64((long double)k*b.imag);
    const long double nr=-(long double)sr+kr,ni=-(long double)si+ki;
    const ComplexValue numerator{binary64((nr*t.real-ni*t.imag)+base_r),
        binary64(base_i+(ni*t.real+nr*t.imag))};
    const long double sum=(long double)k+b.real,dr=(long double)sr+kr,di=(long double)si+ki;
    const ComplexValue denominator{binary64((ar*sum-ai*b.imag)+(dr*t.real-di*t.imag)),
        binary64((sum*ai+ar*b.imag)+(di*t.real+dr*t.imag))};
    return legacy::divide(numerator,denominator).imag;
}
long double green_propagation_integrand(double frequency,const GreenLayers& layers) {
    const double omega=binary64((long double)0x1.515370f99f6cbp-20*
        binary64((long double)0x1.921fb54442d18p+2*frequency));
    const auto a=legacy::square_root({binary64(((long double)layers.layer1*0.0L)*omega),
                                     binary64((long double)layers.layer1*omega)}),
               b=legacy::square_root({binary64(((long double)layers.layer2*0.0L)*omega),
                                     binary64((long double)layers.layer2*omega)});
    const long double twice_depth=(long double)layers.depth+layers.depth;
    const auto e=legacy::exp({binary64(twice_depth*a.real),binary64(twice_depth*a.imag)}),
               f=legacy::exp({binary64(twice_depth*b.real),binary64(twice_depth*b.imag)});
    const double plus=binary64((long double)e.real+1.0L);
    // The sum is reloaded from a binary64 spill, while the difference and
    // subsequent complex products stay extended until the final division.
    const long double minus=(long double)e.real-1.0L,
        twice_plus=(long double)plus+plus,twice_ei=(long double)e.imag+e.imag,
        twice_fr=(long double)f.real+f.real,twice_fi=(long double)f.imag+f.imag;
    const double base_r=binary64(twice_plus*a.real-twice_ei*a.imag);
    const long double base_i=twice_plus*a.imag+twice_ei*a.real,
        ur=twice_fr*minus-twice_fi*e.imag,ui=twice_fr*e.imag+twice_fi*minus;
    const ComplexValue numerator{binary64((ur*b.real-ui*b.imag)+base_r),
        binary64(base_i+(ur*b.imag+ui*b.real))};
    const long double ir=(minus*0.0L-(long double)e.imag*1.0L)*layers.layer1,
        ii=(0.0L*e.imag+minus*1.0L)*layers.layer1,
        vr=(long double)f.real*plus-(long double)f.imag*e.imag,
        vi=(long double)f.real*e.imag+(long double)f.imag*plus,
        wr=vr*a.real-vi*a.imag,wi=vr*a.imag+vi*a.real;
    const ComplexValue denominator{binary64(ir*omega+(wr*b.real-wi*b.imag)),
        binary64(ii*omega+(wr*b.imag+wi*b.real))};
    return legacy::divide(numerator,denominator).imag;
}
long double green_oscillating_kernel(double k,double frequency,const GreenLayers& layers,
                                     double height,double separation) {
    const long double kh=(long double)k*height;
    if(kh>=x87::log(0x1.fffffep127L))return 0.0L;
    const double integrand=binary64(green_oscillating_integrand(k,frequency,layers)),
                 decay=binary64(x87::exp(-kh));
    return ((x87::cos((long double)k*separation)*integrand)*decay)/k;
}
long double green_reflection_kernel(double k,double height) {
    const long double kh=(long double)k*height;
    if(kh>=x87::log(0x1.fffffep127L))return 0.0L;
    return x87::exp(-kh);
}
}
