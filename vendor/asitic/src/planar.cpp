#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
// 0x08061b84. Coplanar nonparallel case, not the orthogonal case
// suggested by the old symbol name. Orthogonal pairs are rejected earlier.
void planar_segment_mutual(const double* a, const double* b, double* out) {
    using x87::binary64;
    using x87::abs;
    using x87::atanh;
    constexpr long double eps=0x1.b7cdfd9d7bdbbp-34L;
    double d0=binary64(distance(a+3,b+3));
    const double d1=binary64(distance(a+3,b));
    const double d2=binary64(distance(a,b));
    const double d3=binary64(distance(a,b+3));
    const double la=binary64(distance(a,a+3));
    const long double raw_lb=distance(b,b+3);
    const double lb=binary64(raw_lb);
    const double p2=binary64(static_cast<long double>(d3)*d3);
    const double q2=binary64(static_cast<long double>(d2)*d2);
    const double diff=binary64(static_cast<long double>(p2)-static_cast<long double>(d2)*d2);
    const long double r2=static_cast<long double>(d1)*d1;
    const long double s2=static_cast<long double>(d0)*d0;
    const long double delta=(static_cast<long double>(diff)+r2)-s2;
    const double delta64=binary64(delta);
    const long double magnitude=abs(delta);
    const long double a2=static_cast<long double>(la)*la;
    const long double b2=raw_lb*raw_lb;
    const long double cosine=magnitude/((static_cast<long double>(la)+la)*lb);
    const double cosangle=binary64(x87::cos(binary64(x87::atan2(x87::sqrt(1.0L-cosine*cosine),cosine))));
    long double sum;
    if (abs(d0)<eps || abs(d1)<eps || abs(d2)<eps || abs(d3)<eps) {
        if (abs(d0)<eps) d0=d2;
        else if (abs(d1)<eps) d0=d3;
        else if (!(abs(d2)<eps) && abs(d3)<eps) d0=d1;
        const double hb=binary64(atanh(lb/(static_cast<long double>(la)+d0)));
        const double ha=binary64(atanh(la/(static_cast<long double>(lb)+d0)));
        sum=static_cast<long double>(lb)*ha+static_cast<long double>(hb)*la;
    } else {
        long double v,w;
        if (delta64>eps) {
            w=static_cast<long double>(diff)-b2;
            v=(static_cast<long double>(binary64(r2))-q2)-a2;
        } else {
            v=(s2-p2)-a2;
            w=(static_cast<long double>(q2)-p2)-b2;
        }
        const long double numerator=la*((b2+b2)*v+magnitude*w);
        const long double denominator=a2*4.0L*b2-static_cast<long double>(delta64)*delta64;
        const long double x=numerator/denominator;
        const long double y=(lb*(magnitude*v+w*(a2+a2)))/denominator;
        const double hb0=binary64(atanh(lb/(static_cast<long double>(d0)+d1)));
        const double hb1=binary64(atanh(lb/(static_cast<long double>(d2)+d3)));
        const double ha0=binary64(atanh(la/(delta64>eps ? static_cast<long double>(d0)+d3
                                                       : static_cast<long double>(d1)+d2)));
        const double ha1=binary64(atanh(la/(delta64>eps ? static_cast<long double>(d1)+d2
                                                       : static_cast<long double>(d0)+d3)));
        sum=((y+lb)*ha0+(static_cast<long double>(hb0)*(x+la)-static_cast<long double>(hb1)*x))
             -static_cast<long double>(ha1)*y;
    }
    *out=binary64((static_cast<long double>(cosangle)+cosangle)*sum);
}
}
