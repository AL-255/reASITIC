#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
// 0x080b3c94. Two-dimensional logarithmic antiderivative used in
// the geometric mean distance of rectangular wire cross sections.
long double cross_section_log_integral(double xd, double yd) {
    const long double x = xd, y = yd, x2 = x*x, y2 = y*y;
    constexpr long double eps = 0x1.b7cdfd9d7bdbbp-34L;
    long double logarithmic, angular = 0.0L;
    if (x87::abs(x) > eps && x87::abs(y) > eps) {
        logarithmic = (x87::log(x2+y2)*((x2*x2-x2*6.0L*y2)+y2*y2))/24.0L;
        const long double a = x87::atan2(y/x,1.0L);
        const long double b = x87::atan2(x/y,1.0L);
        angular = (-x*y*(x2*a+y2*b))/3.0L;
    } else {
        const long double square = x87::abs(x) > eps ? x2 : y2;
        if (x87::abs(x) > eps || x87::abs(y) > eps)
            logarithmic = (x87::log(square)*(square*square))/24.0L;
        else logarithmic = 0.0L;
    }
    return logarithmic+angular;
}

// 0x08063ca0. This returns a distance, not an inductance.
long double effective_separation(double w1, double w2, double t1, double t2,
                                 double dx, double dy) {
    using x87::binary64;
    const long double x = dx, y = dy;
    const long double r = x87::sqrt(x*x+y*y);
    const long double ts = static_cast<long double>(t1)+t2;
    if (ts*1.5L < r && (static_cast<long double>(w1)+w2)*1.5L < r) return r;
    const long double ws = static_cast<long double>(w1)+w2;
    const long double wd = static_cast<long double>(w1)-w2;
    const long double td = static_cast<long double>(t1)-t2;
    const double xs[] = {binary64(wd*0.5L-x), binary64(-ws*0.5L-x),
                         binary64(ws*0.5L-x), binary64(-wd*0.5L-x)};
    const double ys[] = {binary64(td*0.5L-y), binary64(ts*0.5L-y),
                         binary64(-ts*0.5L-y), binary64(-td*0.5L-y)};
    double rows[3] = {};
    long double last = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const double v0 = binary64(cross_section_log_integral(xs[i],ys[0]));
        const double v1 = binary64(cross_section_log_integral(xs[i],ys[1]));
        const double v2 = binary64(cross_section_log_integral(xs[i],ys[2]));
        const long double v3 = cross_section_log_integral(xs[i],ys[3]);
        if (i == 0) rows[0] = binary64((static_cast<long double>(v0)-v1-v2)+v3);
        if (i == 1) rows[1] = binary64(((-static_cast<long double>(v0)+v1)+v2)-binary64(v3));
        if (i == 2) rows[2] = binary64(((-static_cast<long double>(v0)+v1)+v2)-v3);
        if (i == 3) last = ((static_cast<long double>(v0)-v1)-v2)+binary64(v3);
    }
    const long double total = ((static_cast<long double>(rows[0])+rows[1])+rows[2])+last;
    const long double volume = ((static_cast<long double>(w1)*w2)*t1)*t2;
    constexpr long double offset = 0x1.0aaaaaaaaaaabp+1L; // original binary64 25/12
    return binary64(x87::exp((total*-0.5L)/volume-offset));
}
}
