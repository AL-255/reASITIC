#include "asitic/legacy_complex.hpp"
#include "x87.hpp"

namespace asitic::legacy {
namespace {
using x87::binary64;
void scalar_divide_store(double& out,long double value) {
    // The scalar complex-division operator stores directly into its result.
    // Reloading the rounded double first adds DE for subnormal outputs.
    asm volatile("fstpl %0" : "=m"(out) : "t"(value) : "st");
}
long double sinh_real(long double x) {
    const long double a=x87::abs(x),exponent=a*x87::log2e(),integer=x87::round(exponent);
    const long double part=x87::scale(x87::pow2m1(exponent-integer),integer);
    const long double whole=x87::scale(1.0L,integer);
    const long double q=(whole-1.0L)+part;
    const long double value=(q+q/(q+1.0L))*0.5L,sign=x<0?-1.0L:1.0L;
    // FMUL preserves the sign of an indefinite NaN. A compiler-generated
    // conditional FCHS would flip it when the input is negative and enormous.
    long double result;
    asm("fmulp" : "=t"(result) : "0"(value),"u"(sign) : "st(1)");
    return result;
}
long double cosh_real(long double x) {
    const long double e=x87::exp(x);
    return (e+1.0L/e)*0.5L;
}
}
ComplexValue square_root(const ComplexValue& z) {
    const long double real=z.real,imag=z.imag;
    const double m=binary64(x87::sqrt(real*real+imag*imag));
    if(m==0)return {0,0};
    if(real>0) {
        const long double t=x87::sqrt(((long double)m+real)*0.5L);
        return {binary64(t),binary64((imag/t)*0.5L)};
    }
    long double t=x87::sqrt(((long double)m-real)*0.5L);
    if(imag<0)t=-t;
    return {binary64((imag/t)*0.5L),binary64(t)};
}
ComplexValue divide(const ComplexValue& a,const ComplexValue& b) {
    const long double c=b.real,d=b.imag;
    if(x87::abs(d)>=x87::abs(c)) {
        const long double r=c/d,denom=r*r*d+d;
        return {binary64(((long double)a.real*r+a.imag)/denom),
                binary64(((long double)a.imag*r-a.real)/denom)};
    }
    const long double r=d/c,denom=r*r*c+c;
    return {binary64(((long double)a.imag*r+a.real)/denom),
            binary64(((long double)a.imag-(long double)a.real*r)/denom)};
}
ComplexValue divide(double numerator,const ComplexValue& b) {
    const long double c=b.real,d=b.imag,n=numerator;ComplexValue result;
    if(x87::abs(c)<=x87::abs(d)) {
        const long double ratio=c/d,denominator=(ratio*ratio)*d+d;
        scalar_divide_store(result.real,(n*ratio)/denominator);scalar_divide_store(result.imag,-n/denominator);
    } else {
        const long double ratio=d/c,denominator=(ratio*ratio)*c+c;
        scalar_divide_store(result.real,n/denominator);scalar_divide_store(result.imag,(-n*ratio)/denominator);
    }
    return result;
}

ComplexValue exp(const ComplexValue& z) {
    const double e=binary64(x87::exp(z.real));
    return {binary64((long double)e*x87::cos(z.imag)),binary64((long double)e*x87::sin(z.imag))};
}
ComplexValue sinh(const ComplexValue& z) {
    const double s=binary64(sinh_real(z.real)),c=binary64(cosh_real(z.real));
    return {binary64((long double)s*x87::cos(z.imag)),binary64((long double)c*x87::sin(z.imag))};
}
ComplexValue cosh(const ComplexValue& z) {
    const double c=binary64(cosh_real(z.real)),s=binary64(sinh_real(z.real));
    return {binary64((long double)c*x87::cos(z.imag)),binary64((long double)s*x87::sin(z.imag))};
}
}
