#pragma once
#include "asitic/impedance.hpp"

namespace asitic {
struct GreenLayers {
    double layer1,layer2,depth;
};
ComplexValue green_propagation_constant(double wave_number,double frequency,double layer);
ComplexValue green_complex_tanh(ComplexValue z);
long double green_reflection_imag(double wave_number,double frequency,double layer1);
long double green_oscillating_integrand(double wave_number,double frequency,const GreenLayers& layers);
// The original callback accepts an additional wave-number argument but does
// not read it; this limiting propagation integrand depends only on frequency.
long double green_propagation_integrand(double frequency,const GreenLayers& layers);
long double green_oscillating_kernel(double wave_number,double frequency,const GreenLayers& layers,
                                     double height,double separation);
// Original 0x08094958 discards the reflection and cosine values and returns
// only the extended-precision exponential (or zero at the cutoff).
long double green_reflection_kernel(double wave_number,double height);
}
