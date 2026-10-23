#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
double projection_load(const double& value) {
    double out;
    asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(value) : "st");
    return out;
}
double projection_delta(const double& first,const double& second) {
    double out;
    asm volatile("fldl %1; fsubl %2; fstpl %0" : "=m"(out) : "m"(first),"m"(second) : "st");
    return out;
}
}
// 0x08063654. This is an intersection predicate, not an integrator.
int segments_intersect_parametric(const double* p, const double* u,
                                  const double* q, const double* v,
                                  double length_a, double length_b, IntersectionHistory& history) {
    using x87::abs;
    using x87::binary64;
    constexpr long double eps = 0x1.b7cdfd9d7bdbbp-34L;
    const double xy = binary64(static_cast<long double>(binary64(static_cast<long double>(v[0])*u[1]))
                              -static_cast<long double>(u[0])*v[1]);
    const double xz = binary64(static_cast<long double>(binary64(static_cast<long double>(v[0])*u[2]))
                              -static_cast<long double>(u[0])*v[2]);
    const double yz = binary64(static_cast<long double>(v[1])*u[2]-static_cast<long double>(u[1])*v[2]);
    if (abs(xy) < eps && abs(xz) < eps && abs(yz) < eps) {
        const bool x = abs(u[0]) > eps, y = abs(u[1]) > eps, z = abs(u[2]) > eps;
        const long double tx = x ? (static_cast<long double>(q[0])-p[0])/u[0] : 0;
        const long double ty = y ? (static_cast<long double>(q[1])-p[1])/u[1] : 0;
        const double tz = z ? binary64((static_cast<long double>(q[2])-p[2])/u[2]) : 0;
        bool collinear = false;
        if (x && y && z) collinear = abs(tx-ty) < eps && abs(tx-tz) < eps;
        else if (x && y) collinear = abs(tx-ty) < eps;
        else if (x && z) collinear = abs(tx-tz) < eps;
        else if (y && z) collinear = abs(ty-tz) < eps;
        else if (x) collinear = abs(static_cast<long double>(p[1])-q[1]) < eps
                             && abs(static_cast<long double>(p[2])-q[2]) < eps;
        else if (y) collinear = abs(static_cast<long double>(p[0])-q[0]) < eps
                             && abs(static_cast<long double>(p[2])-q[2]) < eps;
        else if (z) collinear = abs(static_cast<long double>(p[0])-q[0]) < eps
                             && abs(static_cast<long double>(p[1])-q[1]) < eps;
        if (!collinear) return 0;
        unsigned k = 0;
        if (u[k] == 0) { k = 1; if (u[k] == 0) k = 2; }
        const long double delta = static_cast<long double>(q[k])-p[k];
        const long double first = delta/u[k];
        const long double last = (static_cast<long double>(length_b)*v[k]+delta)/u[k];
        // Preserve the original asymmetric bounds, including last < length_b.
        return eps < first && first+eps < length_a && eps < last && last+eps < length_b;
    }
    unsigned i = 0, j = 1, k = 2;
    double det = xy;
    bool selected=abs(xy)>eps;
    if (!selected) {
        if (abs(xz) > eps) { j = 2; k = 1; det = xz; selected=true; }
        else if (abs(yz)>eps) { i = 1; j = 2; k = 0; det = yz; selected=true; }
        // Otherwise native retains the incoming projection slots and the XY
        // denominator, including when that denominator is NaN.
    }
    if(selected) {
        const unsigned axes[]={i,j,k};
        double first[3],second[3];
        for(unsigned axis=0;axis<3;++axis) {
            first[axis]=projection_load(u[axis]);
            second[axis]=projection_load(v[axis]);
        }
        for(unsigned axis=0;axis<3;++axis)history.u[axis]=first[axes[axis]];
        for(unsigned axis=0;axis<3;++axis)history.v[axis]=second[axes[axis]];
        for(unsigned axis=0;axis<3;++axis)history.delta[axis]=projection_delta(q[axes[axis]],p[axes[axis]]);
    }
    const double di=history.delta[0],dj=history.delta[1],dk=history.delta[2];
    const long double t = (static_cast<long double>(dj)*history.v[0]-static_cast<long double>(di)*history.v[1])/det;
    const long double s = (static_cast<long double>(dj)*history.u[0]-static_cast<long double>(di)*history.u[1])/det;
    if (abs((t*history.u[2]-s*history.v[2])-dk) > eps) return 0;
    return eps < t && t+eps < length_a && eps < s && s+eps < length_b;
}
int segments_intersect_parametric(const double* p,const double* u,const double* q,const double* v,
                                  double length_a,double length_b) {
    IntersectionHistory history{};
    return segments_intersect_parametric(p,u,q,v,length_a,length_b,history);
}
}
