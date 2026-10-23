#include "asitic/network.hpp"
#include "asitic/legacy_complex.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
struct ExtendedComplex { long double real,imag; };
ExtendedComplex multiply(const ComplexValue& a,const ComplexValue& b) {
    return {(long double)a.real*b.real-(long double)a.imag*b.imag,
            (long double)a.imag*b.real+(long double)a.real*b.imag};
}
ComplexValue differential_impedance(const TwoPortAdmittance& y) {
    const auto product=multiply(y.y22,y.y11);
    const long double squared_real=(long double)y.y21.real*y.y21.real-(long double)y.y21.imag*y.y21.imag;
    const double square64=binary64(squared_real);
    const long double square_product=(long double)y.y21.real*y.y21.imag,
        squared_imag=square_product+square_product;
    const ComplexValue determinant{binary64(product.real-square64),binary64(product.imag-squared_imag)};
    const ComplexValue numerator{binary64(((long double)y.y22.real+y.y11.real)+((long double)y.y21.real+y.y21.real)),
                                 binary64(((long double)y.y22.imag+y.y11.imag)+((long double)y.y21.imag+y.y21.imag))};
    return legacy::divide(numerator,determinant);
}
// Native804e9b0 explicitly quiets these loads. Its last cross-product
// operand is instead an FMUL memory operand: loading it separately can raise
// a denormal exception even when the native NaN operation suppresses it.
void terminated_discard(long double value) {
    double slot;
    asm volatile("fstl %0" : "=m"(slot) : "t"(value));
}
long double terminated_load(const double& x) { long double out;asm volatile("fldl %1" : "=t"(out) : "m"(x));return out; }
long double terminated_multiply_memory(long double x,const double& y) { long double out;asm volatile("fmull %2" : "=t"(out) : "0"(x),"m"(y));return out; }
ExtendedComplex terminated_base(const ComplexValue& a,const ComplexValue& b) {
    const long double ar=terminated_load(a.real),br=terminated_load(b.real),first=ar*br,ai=terminated_load(a.imag),bi=terminated_load(b.imag);
    return {first-ai*bi,ar*bi+ai*br};
}
ExtendedComplex terminated_extra(const ComplexValue& a,const ComplexValue& b) {
    const long double ar=terminated_load(a.real),ai=terminated_load(a.imag);
    return {terminated_multiply_memory(ar,b.real)-terminated_multiply_memory(ai,b.imag),
            terminated_multiply_memory(ar,b.imag)+terminated_multiply_memory(ai,b.real)};
}
ExtendedComplex terminated_cross(const ComplexValue& c,const ComplexValue& d) {
    const long double cr=terminated_load(c.real),dr=terminated_load(d.real),first=cr*dr,ci=terminated_load(c.imag);
    return {first-terminated_multiply_memory(ci,d.imag),dr*ci+terminated_multiply_memory(cr,d.imag)};
}
double power_square(long double x) { return x==0?0:binary64((x*x)*1.0L); }
}
ComplexValue two_port_impedance(const TwoPortAdmittance& y,bool differential,int port) {
    return differential?differential_impedance(y):legacy::divide(1.,port==1?y.y22:y.y11);
}
long double two_port_imaginary_impedance(const TwoPortAdmittance& y,int differential) {
    return differential?differential_impedance(y).imag:legacy::divide(1.,y.y22).imag;
}
ComplexValue terminated_two_port_impedance(const TwoPortAdmittance& y,const ComplexValue& load,int port) {
    const auto admittance=legacy::divide(1.,load);
    const auto base=terminated_base(y.y22,y.y11);
    const auto extra=terminated_extra(port==1?y.y22:y.y11,admittance);
    // Discarded native FST copies raise overflow/underflow/inexact flags while
    // the live base, extra and combined values retain extended precision.
    terminated_discard(base.real);terminated_discard(base.imag);
    terminated_discard(extra.real);terminated_discard(extra.imag);
    const long double total_real=base.real+extra.real,total_imag=base.imag+extra.imag;
    terminated_discard(total_real);terminated_discard(total_imag);
    const auto cross=terminated_cross(y.y21,y.y12);
    terminated_discard(cross.real);terminated_discard(cross.imag);
    const ComplexValue& other=port==1?y.y11:y.y22;
    const ComplexValue numerator{binary64(terminated_load(other.real)+admittance.real),binary64(terminated_load(other.imag)+admittance.imag)},
        denominator{binary64(total_real-cross.real),binary64(total_imag-cross.imag)};
    return legacy::divide(numerator,denominator);
}

long double pi_shunt_resistance(const PiEquivalent& p,int differential) {
    constexpr long double two_pi=0x1.921fb54442d18p+2L;
    const long double capacitance=differential?((long double)p.capacitance_second*p.capacitance_first)/
            ((long double)p.capacitance_second+p.capacitance_first):p.capacitance_second,
        resistance=differential?(long double)p.resistance_second+p.resistance_first:p.resistance_second,
        angular=(long double)p.frequency*two_pi;
    const double a=power_square((angular*p.inductance)/p.resistance),b=power_square(1.0L/((angular*capacitance)*resistance));
    const long double series=((long double)a+1.0L)*p.resistance,shunt=(1.0L+b)*resistance;
    return (series*shunt)/(series+shunt);
}
long double pi_resonant_frequency(const PiEquivalent& p) {
    const long double ratio=(long double)p.capacitance_second/p.inductance,
        correction=(1.0L-((long double)p.resistance*p.resistance)*ratio)/
                   (1.0L-((long double)p.resistance_second*p.resistance_second)*ratio);
    long double result=1.0L/((long double)p.inductance*p.capacitance_second);
    if(correction>0)result*=correction;
    return x87::sqrt(result)/0x1.921fb54442d18p+2L;
}
PiEquivalent extract_pi_equivalent(const TwoPortAdmittance& y,double frequency) {
    const ComplexValue second{binary64((long double)y.y21.real+y.y22.real),binary64((long double)y.y21.imag+y.y22.imag)},
        first{binary64((long double)y.y11.real+y.y12.real),binary64((long double)y.y11.imag+y.y12.imag)},
        series{-y.y21.real,-y.y21.imag};
    const double angular=binary64((long double)frequency*0x1.921fb54442d18p+2L);
    const auto impedance=legacy::divide(1.,series);
    PiEquivalent result{};result.frequency=frequency;result.resistance=impedance.real;
    result.inductance=binary64((long double)impedance.imag/angular);
    if(binary64(x87::sqrt((long double)second.real*second.real+(long double)second.imag*second.imag))>0x1.b7cdfd9d7bdbbp-34) {
        const auto z=legacy::divide(1.,second);result.resistance_second=z.real;
        result.capacitance_second=binary64(-1.0L/((long double)angular*z.imag));
    } else { result.resistance_second=1e15;result.capacitance_second=0; }
    if(binary64(x87::sqrt((long double)first.real*first.real+(long double)first.imag*first.imag))>0x1.b7cdfd9d7bdbbp-34) {
        const auto z=legacy::divide(1.,first);result.resistance_first=z.real;
        result.capacitance_first=binary64(-1.0L/((long double)angular*z.imag));
    } else { result.resistance_first=1e15;result.capacitance_first=0; }
    result.quality_first=binary64(-((long double)second.imag+series.imag)/((long double)second.real+series.real));
    result.quality_second=binary64(-((long double)series.imag+first.imag)/((long double)series.real+first.real));
    const auto differential=two_port_impedance(y,true,1);
    result.quality_differential=binary64((long double)differential.imag/differential.real);
    result.shunt_single=binary64(pi_shunt_resistance(result,0));
    result.shunt_differential=binary64(pi_shunt_resistance(result,1));
    return result;
}
}
