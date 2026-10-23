#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
int segment_mutual(const double* a, const double* b, double* out, const char** warning,
                   IntersectionHistory& history) {
    using x87::binary64;
    if (warning) *warning=nullptr;
    double da[3], db[3];
    for (unsigned i=0;i<3;++i) {
        da[i]=binary64(static_cast<long double>(a[i])-a[i+3]);
        db[i]=binary64(static_cast<long double>(b[i])-b[i+3]);
    }
    constexpr long double eps=0x1.b7cdfd9d7bdbbp-34L;
    const long double orientation=dot(da,db);
    double sign=1;
    if (!(orientation > eps)) {
        if (!(orientation < -eps)) { *out=0.0; return 0; }
        sign=-1;
    }
    constexpr long double scale=0x1.a36e2eb1c432dp-14L;
    const double separation=binary64(x87::abs(static_cast<long double>(a[2])*scale
                                           -static_cast<long double>(b[2])*scale));
    double sa[8],sb[8];
    for (unsigned i=0;i<8;++i) {
        sa[i]=binary64(static_cast<long double>(a[i])*scale);
        sb[i]=binary64(static_cast<long double>(b[i])*scale);
    }
    double value=0;
    switch (canonicalize_segment_pair(sa,sb,history)) {
    case 0: parallel_segment_mutual(sa,sb,&value,separation); break;
    case 1: planar_segment_mutual(sa,sb,&value); break;
    case 2: spatial_segment_mutual(sa,sb,&value); break;
    case 3: if (warning) *warning="Warning:  Segments intersect."; break;
    case 4: case 5: case 6: break;
    default: if (warning) *warning="Warning:  An unknown error has occurred."; break;
    }
    *out=binary64(x87::abs(value)*sign);
    return 0;
}
int segment_mutual(const double* a,const double* b,double* out,const char** warning) {
    IntersectionHistory history{};
    return segment_mutual(a,b,out,warning,history);
}
}
