#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
// 0x08062230. Despite its old decompiler name, this is the projected
// parallel-segment case table, not the general three-dimensional integral.
int parallel_segment_mutual(const double* a, const double* b, double* out, double sep) {
    using x87::abs;
    using x87::binary64;
    constexpr long double eps = 0x1.b7cdfd9d7bdbbp-34L;
    double alo = a[0], ahi = a[3], bhi = b[3];
    long double ady = static_cast<long double>(a[4])-a[1];
    long double bdy = static_cast<long double>(b[4])-b[1];
    long double alen = static_cast<long double>(ahi)-alo;
    long double blo = b[0];
    long double bdx = static_cast<long double>(bhi)-blo;
    double blen = binary64(bdx);
    const double abs_adx = binary64(abs(alen));
    if (binary64(x87::sqrt(static_cast<long double>(abs_adx)*abs_adx+abs(ady)*abs(ady))) < eps
        || x87::sqrt(abs(bdy)*abs(bdy)+abs(bdx)*abs(bdx)) < eps) return -1;
    if (abs(ady*binary64(bdy)+alen*blen) < eps) { *out = 0.0; return 0; }
    if (ahi < alo) {
        alen = static_cast<long double>(alo)-ahi;
        double tmp = ahi; ahi = alo; alo = tmp;
    }
    if (bhi < blo) {
        blo = bhi;
        blen = binary64(static_cast<long double>(b[0])-blo);
        bhi = b[0];
    }
    const auto classify = [&](double x) {
        return blo <= x ? (x <= bhi ? 2 : 1) : 0;
    };
    const int kind = classify(ahi)+3*classify(alo);
    long double ysep = abs(static_cast<long double>(a[1])-b[1]);
    if (eps < sep) {
        if (abs(ysep-sep) < eps) ysep = 0;
        else ysep = x87::sqrt(static_cast<long double>(sep)*sep+ysep*ysep);
    }
    const double radius = binary64(effective_separation(a[6],b[6],a[7],b[7],binary64(ysep),sep));
    const auto self = [&](long double length) {
        return segment_self_inductance(binary64(length),radius);
    };
    const auto rounded_self = [&](long double length) {
        return static_cast<long double>(binary64(self(length)));
    };
    const auto adjacent = [&]() {
        const long double total = alen+blen;
        return x87::log(total/blen)*blen+alen*x87::log(total/alen);
    };
    const auto collinear = [&](double gap) {
        const long double total = (alen+blen)+gap;
        const long double p = static_cast<long double>(gap)+alen;
        const long double q = static_cast<long double>(blen)+gap;
        return x87::log(static_cast<long double>(gap))*gap
             + ((x87::log(total)*binary64(total)-p*x87::log(p))-q*x87::log(q));
    };
    const auto three = [&](double gap) {
        return 0.5L*((rounded_self(alen)+rounded_self(blen))-self(gap));
    };
    long double value = 0;
    switch (kind) {
    case 0:
    case 4: {
        const long double raw_gap = kind == 0 ? blo-ahi : static_cast<long double>(alo)-bhi;
        const double gap = binary64(raw_gap);
        if (ysep < eps && raw_gap < eps) value = adjacent();
        else if (ysep < eps) value = collinear(gap);
        else if (gap >= eps) {
            const long double s0 = rounded_self((alen+blen)+gap);
            const long double s1 = rounded_self(gap);
            const long double s2 = rounded_self(alen+gap);
            const long double s3 = self(static_cast<long double>(blen)+gap);
            value = kind == 0 ? ((s0+s1)-(s2+binary64(s3)))*0.5L
                              : (((s0+s1)-s2)-s3)*0.5L;
        } else value = 0.5L*((rounded_self(alen+blen)-rounded_self(alen))-self(blen));
        break;
    }
    case 2:
    case 7: {
        const long double raw_overlap = kind == 2 ? static_cast<long double>(ahi)-blo
                                                 : static_cast<long double>(bhi)-alo;
        const double overlap = binary64(raw_overlap);
        if (raw_overlap < eps && ysep < eps) value = adjacent();
        else if (overlap >= eps) {
            if (ysep < eps && (kind == 2 || sep < eps)) return -1;
            value = (((rounded_self((alen+blen)-overlap)+rounded_self(overlap))
                       -rounded_self(alen-overlap))-self(static_cast<long double>(blen)-overlap))*0.5L;
        } else value = 0.5L*((rounded_self(alen+blen)-rounded_self(alen))-self(blen));
        break;
    }
    case 1:
    case 8: {
        const long double raw_left = kind == 1 ? blo-alo : static_cast<long double>(alo)-blo;
        const long double raw_right = kind == 1 ? static_cast<long double>(ahi)-bhi
                                               : static_cast<long double>(bhi)-ahi;
        const double left = binary64(raw_left), right = binary64(raw_right);
        if (raw_left < eps && raw_right < eps) value = self(kind == 1 ? alen : blen);
        else if (left >= eps && right >= eps) {
            const long double inside = kind == 1 ? static_cast<long double>(blen) : alen;
            value = (((rounded_self(inside+left)+rounded_self(inside+right))
                       -rounded_self(left))-self(right))*0.5L;
        } else value = three(left >= eps ? left : right);
        break;
    }
    case 3: case 5: case 6: return -1;
    }
    *out = binary64(value);
    return 0;
}
}
