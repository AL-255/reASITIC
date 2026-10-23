#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
// 0x080613bc. General spatial segment mutual-inductance formula.
void spatial_segment_mutual(const double* a, const double* b, double* out) {
    using x87::binary64;
    using x87::abs;
    using x87::atanh;
    constexpr long double eps=0x1.b7cdfd9d7bdbbp-34L;
    const double d0=binary64(distance(a+3,b+3));
    const double d1=binary64(distance(a+3,b));
    const double d2=binary64(distance(a,b));
    const double d3=binary64(distance(a,b+3));
    const double la=binary64(distance(a,a+3));
    const double lb=binary64(distance(b,b+3));
    const double p2=binary64(static_cast<long double>(d3)*d3);
    const double q2=binary64(static_cast<long double>(d2)*d2);
    const double diff=binary64(static_cast<long double>(p2)-static_cast<long double>(d2)*d2);
    const long double r2=static_cast<long double>(d1)*d1;
    const long double s2=static_cast<long double>(d0)*d0;
    const long double delta=(static_cast<long double>(diff)+r2)-s2;
    const double delta64=binary64(delta);
    const long double magnitude=abs(delta);
    const long double cosine=magnitude/((static_cast<long double>(la)+la)*lb);
    const double angle=binary64(x87::atan2(x87::sqrt(1.0L-cosine*cosine),cosine));
    const double c=binary64(x87::cos(angle)), s=binary64(x87::sin(angle));
    const long double a2=static_cast<long double>(la)*la, b2=static_cast<long double>(lb)*lb;
    long double v,w;
    if (delta64 > eps) {
        w=static_cast<long double>(diff)-b2;
        v=(static_cast<long double>(binary64(r2))-q2)-a2;
    } else {
        v=(s2-p2)-a2;
        w=(static_cast<long double>(q2)-p2)-b2;
    }
    const long double den=a2*4.0L*b2-static_cast<long double>(delta64)*delta64;
    const long double x=(la*(magnitude*w+(b2+b2)*v))/den;
    const long double y=(lb*(w*(a2+a2)+magnitude*v))/binary64(den);
    const long double h2=(x+x)*y*c+(((delta64 > eps ? q2 : p2)-x*x)-y*y);
    const double h=binary64(x87::sqrt(h2));
    const long double n=c*h2;
    const long double xp=x+la;
    const double yp=binary64(y+lb);
    const double a0=binary64(x87::atan2(xp*(y+lb)*s*s+n,
                                       static_cast<long double>(h)*(delta64 > eps ? d0 : d1)*s));
    const double a1=binary64(x87::atan2(n+xp*y*s*s,
                                       static_cast<long double>(h)*(delta64 > eps ? d1 : d0)*s));
    const double a2angle=binary64(x87::atan2(n+x*y*s*s,
                                           static_cast<long double>(h)*(delta64 > eps ? d2 : d3)*s));
    const double a3=binary64(x87::atan2(x*yp*s*s+n,
                                       static_cast<long double>(h)*(delta64 > eps ? d3 : d2)*s));
    const long double angular=((static_cast<long double>(a0)-a1)+a2angle)-a3;
    const double hb0=binary64(atanh(lb/(static_cast<long double>(d0)+d1)));
    const double hb1=binary64(atanh(lb/(static_cast<long double>(d2)+d3)));
    const double ha0=binary64(atanh(la/(delta64>eps ? static_cast<long double>(d1)+d2
                                                   : static_cast<long double>(d0)+d3)));
    const double ha1=binary64(atanh(la/(delta64>eps ? static_cast<long double>(d0)+d3
                                                   : static_cast<long double>(d1)+d2)));
    const long double sum=static_cast<long double>(ha1)*yp
                         +((static_cast<long double>(hb0)*xp-static_cast<long double>(hb1)*x)-y*ha0);
    *out=binary64(((sum+sum)-(angular*h)/s)*c);
}
}
