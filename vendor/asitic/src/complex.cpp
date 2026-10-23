#include "asitic/lapack.hpp"
#include "x87.hpp"

namespace asitic::lapack {
[[noreturn]] __attribute__((weak)) void fatal_error(const char*) { __builtin_trap(); }
[[noreturn]] __attribute__((weak)) void application_fatal_error(const char* message) { fatal_error(message); }
__attribute__((weak)) void argument_error(const char*,int) {}
namespace {
using x87::binary64;
void store_binary64(double& destination,long double value) {
    // The native final FSTP does not reload subnormal results. Returning a
    // rounded double through a C++ temporary would add an FLD and set DE.
    asm volatile("fstpl %0" : "=m"(destination) : "t"(value) : "st");
}
ComplexValue product(const ComplexValue& a,const ComplexValue& b) {
    return {binary64((long double)a.real*b.real-(long double)a.imag*b.imag),
            binary64((long double)a.imag*b.real+(long double)a.real*b.imag)};
}
}
long double abs1(const ComplexValue& z) {
    return x87::abs(z.real)+x87::abs(z.imag);
}
long double magnitude(const ComplexValue& z) {
    long double x=z.real,y=z.imag;
    if(x<0) x=-x;
    if(y<0) y=-y;
    if(x<y) { const long double t=x;x=y;y=t; }
    if(x+y==x) return x;
    const long double r=y/x;
    return x87::sqrt(1.0L+r*r)*x;
}
void divide(ComplexValue& out,const ComplexValue& a,const ComplexValue& b) {
    const long double c=b.real,d=b.imag;
    const long double ac=c<0?-c:c,ad=d<0?-d:d;
    long double real,imag;
    if(ac<=ad) {
        if(ad==0)fatal_error("complex division by zero");
        const long double r=c/d,denom=r*r*d+d;
        real=((long double)a.real*r+a.imag)/denom;
        imag=((long double)a.imag*r-a.real)/denom;
    } else {
        const long double r=d/c,denom=r*r*c+c;
        real=((long double)a.imag*r+a.real)/denom;
        imag=((long double)a.imag-(long double)a.real*r)/denom;
    }
    store_binary64(out.imag,imag);store_binary64(out.real,real);
}
void square_root(ComplexValue& out,const ComplexValue& z) {
    const long double real=z.real,imag=z.imag,m=magnitude(z);
    if(m==0) { out={0,0};return; }
    // The native test selects the first branch only for ordered real > 0.
    // A NaN real part takes this branch, including the imaginary sign change.
    if(!(real>0)) {
        const long double t=x87::sqrt(0.5L*(m-real));
        const double im=binary64(imag<0?-t:t);
        out.imag=im;store_binary64(out.real,(imag/im)/2.0L);
    } else {
        const long double t=x87::sqrt(0.5L*(real+m));
        store_binary64(out.real,t);store_binary64(out.imag,(imag/t)/2.0L);
    }
}
void integer_power(ComplexValue& out,const ComplexValue& z,int exponent) {
    ComplexValue acc{1,0},base=z;
    unsigned p=static_cast<unsigned>(exponent);
    if(exponent<0) { p=0u-p;divide(base,acc,z); }
    while(p) {
        if(p&1) acc=product(acc,base);
        p>>=1;
        if(!p)break;
        const long double real=(long double)base.real*base.real-(long double)base.imag*base.imag;
        const long double imag=((long double)base.real+base.real)*base.imag;
        base.imag=binary64(imag);base.real=binary64(real);
    }
    out=acc;
}
void eigen2(const ComplexValue& a,const ComplexValue& b,const ComplexValue& c,
            ComplexValue& rt1,ComplexValue& rt2,ComplexValue& evscal,
            ComplexValue& cs1,ComplexValue& sn1) {
    if(magnitude(b)==0) {
        rt1=a;rt2=c;
        const double m=binary64(magnitude(rt1));
        if(magnitude(rt2)<=m) { cs1={1,0};sn1={0,0}; }
        else { const auto t=rt1;rt1=rt2;rt2=t;cs1={0,0};sn1={1,0}; }
        return;
    }
    const ComplexValue s{binary64(((long double)a.real+c.real)*0.5L),
                         binary64(((long double)a.imag+c.imag)*0.5L)};
    const ComplexValue diff{binary64((long double)a.real-c.real),binary64((long double)a.imag-c.imag)};
    ComplexValue t{binary64((long double)diff.real*0.5L),binary64((long double)diff.imag*0.5L)};
    const double bn=binary64(magnitude(b));
    long double z=magnitude(t);
    if(z<=bn)z=bn;
    const double zd=binary64(z);
    if(z>0) {
        ComplexValue st{binary64(t.real/z),binary64(t.imag/z)},sb{binary64((long double)b.real/zd),binary64((long double)b.imag/zd)};
        ComplexValue t2,b2,root;
        integer_power(t2,st,2);integer_power(b2,sb,2);
        const ComplexValue sum{binary64((long double)t2.real+b2.real),binary64((long double)t2.imag+b2.imag)};
        square_root(root,sum);
        t={binary64((long double)zd*root.real),binary64((long double)zd*root.imag)};
    }
    rt1={binary64((long double)s.real+t.real),binary64((long double)s.imag+t.imag)};
    rt2={binary64((long double)s.real-t.real),binary64((long double)s.imag-t.imag)};
    const double m1=binary64(magnitude(rt1));
    if(m1<magnitude(rt2)) { const auto tmp=rt1;rt1=rt2;rt2=tmp; }
    const ComplexValue delta{binary64((long double)rt1.real-a.real),binary64((long double)rt1.imag-a.imag)};
    divide(sn1,delta,b);
    const long double n=magnitude(sn1);
    const double nd=binary64(n);
    if(n>1.0L) {
        const ComplexValue scaled{binary64(sn1.real/n),binary64(sn1.imag/n)};
        ComplexValue sq,root;
        integer_power(sq,scaled,2);
        const long double inv=1.0L/n;
        const ComplexValue sum{binary64(inv*inv+sq.real),sq.imag};
        square_root(root,sum);
        t={binary64((long double)nd*root.real),binary64((long double)nd*root.imag)};
    } else {
        const long double re=(long double)sn1.real*sn1.real-(long double)sn1.imag*sn1.imag;
        const double im=binary64((long double)sn1.real*sn1.imag+(long double)sn1.real*sn1.imag);
        const ComplexValue sum{binary64(re+1.0L),binary64((long double)im+0.0L)};
        square_root(t,sum);
    }
    if(magnitude(t)>=0x1.999999999999ap-4L) {
        const ComplexValue one{1,0};divide(evscal,one,t);cs1=evscal;
        sn1=product(sn1,evscal);
    } else evscal={0,0};
}
}
