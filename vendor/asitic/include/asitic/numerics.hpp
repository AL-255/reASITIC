#pragma once

// Original i386 ASITIC returns these scalars in x87 ST(0), without a
// narrowing store to double. Keep this ABI and the 64-bit significand.
static_assert(__LDBL_MANT_DIG__ == 64, "ASITIC requires x87 extended precision");
static_assert(__DBL_MANT_DIG__ == 53, "ASITIC requires binary64 doubles");

namespace asitic {
long double norm(const double* v);
long double distance(const double* a, const double* b);
long double dot(const double* a, const double* b);
long double sqrt_dot(const double* a, const double* b);
void cross(const double* a, const double* b, double* out);
long double clipped_divide(double numerator, double denominator);
long double coth(double x);
long double segment_self_inductance(double length, double radius);
long double rectangular_self_inductance(double width, double height, double length);
long double cross_section_log_integral(double x, double y);
long double effective_separation(double w1, double w2, double t1, double t2,
                                 double dx, double dy);
// Original 0x08062230: endpoints at [0..2], [3..5], width/thickness
// at [6], [7]. This kernel operates on the x/y projections.
// Returns -1 without touching *out for rejected geometry, otherwise 0.
int parallel_segment_mutual(const double* a, const double* b, double* out, double separation);
// Original 0x08063654 leaves these projection slots untouched when no
// determinant exceeds its tolerance. Explicit history preserves that path;
// the existing convenience overload starts with zero projection slots.
struct IntersectionHistory {
    double delta[3];
    double v[3];
    double u[3];
};
int segments_intersect_parametric(const double* origin_a, const double* direction_a,
                                  const double* origin_b, const double* direction_b,
                                  double length_a, double length_b);
int segments_intersect_parametric(const double* origin_a, const double* direction_a,
                                  const double* origin_b, const double* direction_b,
                                  double length_a, double length_b, IntersectionHistory& history);
// Mutates endpoints into the original canonical basis and selects the
// following mutual-inductance kernel (0 parallel, 1 planar, 2 spatial).
// Other return codes preserve the original geometry rejection behavior.
int canonicalize_segment_pair(double* a, double* b);
int canonicalize_segment_pair(double* a, double* b, IntersectionHistory& history);
void planar_segment_mutual(const double* a, const double* b, double* out);
void spatial_segment_mutual(const double* a, const double* b, double* out);
// 0x08061110: complete signed segment-pair path, including micron
// scaling. Optional warning receives the original message or nullptr.
int segment_mutual(const double* a, const double* b, double* out, const char** warning = nullptr);
int segment_mutual(const double* a, const double* b, double* out, const char** warning,
                   IntersectionHistory& history);
}
