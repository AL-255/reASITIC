#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
long double dot(const double* a, const double* b) {
    return (static_cast<long double>(a[0]) * b[0]
          + static_cast<long double>(a[1]) * b[1])
          + static_cast<long double>(a[2]) * b[2];
}

long double norm(const double* v) { return x87::sqrt(dot(v, v)); }
long double sqrt_dot(const double* a, const double* b) {
    return x87::sqrt(dot(a, b));
}

long double distance(const double* a, const double* b) {
    const long double dx = static_cast<long double>(a[0]) - b[0];
    const long double dy = static_cast<long double>(a[1]) - b[1];
    const long double dz = static_cast<long double>(a[2]) - b[2];
    return x87::sqrt((dx * dx + dy * dy) + dz * dz);
}

void cross(const double* a, const double* b, double* out) {
    // Store in original order: callers may alias out with either input.
    out[0] = x87::binary64(static_cast<long double>(a[1]) * b[2]
                       - static_cast<long double>(a[2]) * b[1]);
    out[1] = x87::binary64(static_cast<long double>(a[2]) * b[0]
                       - static_cast<long double>(a[0]) * b[2]);
    out[2] = x87::binary64(static_cast<long double>(a[0]) * b[1]
                       - static_cast<long double>(a[1]) * b[0]);
}

long double clipped_divide(double numerator, double denominator) {
    const long double n = numerator, d = denominator;
    // The binary loads binary64 constants here, not decimal long doubles.
    const long double eps = 0x1.b7cdfd9d7bdbbp-34L;
    if (!(x87::abs(d) < eps)) return n / d;
    if ((d < eps && n > eps) || (d < -eps && n < -eps)) return 1e15L;
    if ((d < -eps && n > eps) || (d < eps && n > -eps)) return -1e15L;
    if (d < eps && n < eps) return 1.0L;
    return 1e15L;
}

long double coth(double x) {
    const long double v = x;
    const long double exponent = -x87::abs(v + v) * x87::log2e();
    const long double integral = x87::round(exponent);
    const long double fractional = x87::pow2m1(exponent - integral);
    const long double small = x87::scale(fractional, integral);
    const long double base = x87::scale(1.0L, integral);
    const long double em1 = (base - 1.0L) + small;
    const long double sign = !(x <= 0) ? -1.0L : 1.0L;
    const double rounded = x87::binary64((em1 / (em1 + 2.0L)) * sign);
    return 1.0L / static_cast<long double>(rounded);
}

long double segment_self_inductance(double length, double radius) {
    const long double l = length;
    long double term = 0;
    if (!(l < 0x1.b7cdfd9d7bdbbp-34L)) {
        const long double a = l / static_cast<long double>(radius);
        const long double inv = 1.0L / a;
        term = (x87::log(x87::sqrt(a * a + 1.0L) + a)
                - x87::sqrt(inv * inv + 1.0L)) + inv;
    }
    return (l + l) * term;
}
}
