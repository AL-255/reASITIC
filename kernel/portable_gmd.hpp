#pragma once
namespace reasitic {
bool gmd_farfield_exact(double,double,double,double,double,double);
double geometry_radius_big(double,double);
double error_power_stable(double);
double error_power_big(double);
struct GeometricMeanDistance { double distance;int cancellation_bits;bool reliable;int precision_bits; };
// Same rectangular cross-section integral, evaluated with software binary128
// and adaptive arbitrary precision, with a single binary64 result store.
// No host libm or x87 calls.
GeometricMeanDistance geometric_mean_distance_big(double,double,double,double,double,double);
GeometricMeanDistance geometric_mean_distance(double,double,double,double,double,double);
}
