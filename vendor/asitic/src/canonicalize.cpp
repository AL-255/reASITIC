#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
// 0x08062ebc. Set up the local basis and select a mutual-inductance path.
// It also rejects intersections; it does not itself compute inductance.
int canonicalize_segment_pair(double* a, double* b, IntersectionHistory& history) {
    using x87::abs;
    using x87::binary64;
    constexpr long double eps = 0x1.b7cdfd9d7bdbbp-34L;
    double da[3], db[3], p[3], q[3];
    for (unsigned i=0;i<3;++i) {
        p[i]=a[i]; q[i]=b[i];
        da[i]=binary64(static_cast<long double>(a[i+3])-a[i]);
        db[i]=binary64(static_cast<long double>(b[i+3])-b[i]);
    }
    if (abs(dot(da,db)) < eps) return 5;
    const long double na=norm(da);
    if (na > eps) for (double& v : da) v=binary64(v/na);
    if (na < eps) return 6;
    const long double nb=norm(db);
    if (nb > eps) for (double& v : db) v=binary64(v/nb);
    if (nb < eps) return 6;
    const auto length = [&](const double* s, const double* dir) {
        for (unsigned i=0;i<3;++i) {
            const long double delta=static_cast<long double>(s[i+3])-s[i];
            if (abs(delta) > eps && abs(dir[i]) > eps) return binary64(delta/dir[i]);
        }
        return 0.0;
    };
    const double la=length(a,da), lb=length(b,db);
    if (la == 0 || lb == 0) return 4;
    if (segments_intersect_parametric(p,da,q,db,la,lb,history)) return 3;

    double delta[3], start[3], end[3];
    for (unsigned i=0;i<3;++i) {
        const long double shifted_start=static_cast<long double>(a[i])+5000.0L;
        const long double shifted_end=static_cast<long double>(a[i+3])+5000.0L;
        // The original retains the z start in ST, but reloads x/y starts
        // from QWORD stores. Removing the translation changes low bits.
        delta[i]=binary64(shifted_end-(i == 2 ? shifted_start : binary64(shifted_start)));
        start[i]=binary64(static_cast<long double>(b[i])-a[i]);
        end[i]=binary64(static_cast<long double>(b[i+3])-a[i]);
    }
    const long double nd=norm(delta);
    if (nd < eps) return 4;
    double e1[3], first[3], last[3], e3[3], e2[3];
    for (unsigned i=0;i<3;++i) e1[i]=binary64(delta[i]/nd);
    const long double ns=norm(start), ne=norm(end);
    for (unsigned i=0;i<3;++i) {
        first[i]=ns > eps ? binary64(start[i]/ns) : 0.0;
        last[i]=ne > eps ? binary64(end[i]/ne) : 0.0;
    }
    cross(e1,first,e3);
    long double n3=norm(e3);
    if (n3 < eps) {
        cross(e1,last,e3); n3=norm(e3);
        if (n3 < eps) return 0;
    }
    for (double& v : e3) v=binary64(v/n3);
    cross(e3,e1,e2);
    const long double n2=norm(e2);
    for (double& v : e2) v=binary64(v/n2);
    a[0]=a[1]=a[2]=a[4]=a[5]=0.0;
    a[3]=binary64(norm(delta));
    b[0]=binary64(dot(start,e1)); b[1]=binary64(dot(start,e2)); b[2]=binary64(dot(start,e3));
    b[3]=binary64(dot(end,e1)); b[4]=binary64(dot(end,e2));
    const long double last_z=dot(end,e3);
    b[5]=binary64(last_z);
    const long double projection=abs(((static_cast<long double>(a[0])-a[3])*(static_cast<long double>(b[0])-b[3])
                                   +(static_cast<long double>(a[1])-a[4])*(static_cast<long double>(b[1])-b[4]))
                                   +(static_cast<long double>(a[2])-a[5])*(static_cast<long double>(b[2])-last_z));
    if (abs(projection-distance(b,b+3)*binary64(distance(a,a+3))) < eps) return 0;
    if (abs(b[5]) < eps && abs(b[2]) < eps) return 1;
    return 2;
}
int canonicalize_segment_pair(double* a,double* b) {
    IntersectionHistory history{};
    return canonicalize_segment_pair(a,b,history);
}
}
