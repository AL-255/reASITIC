#pragma once
#include "asitic/impedance.hpp"

namespace asitic {
struct TwoPortAdmittance { ComplexValue y11,y12,y21,y22; };
// Originals 0x0804e8b0 and 0x0804e7c0. Differential mode assumes reciprocity
// and uses y21 squared, ignoring y12. Single-ended port 1 selects 1/y22.
ComplexValue two_port_impedance(const TwoPortAdmittance& y,bool differential,int port);
long double two_port_imaginary_impedance(const TwoPortAdmittance& y,int differential);
// Original 0x0804e9b0. The load is impedance; both off-diagonal cells are used.
ComplexValue terminated_two_port_impedance(const TwoPortAdmittance& y,const ComplexValue& load,int port);
// Contiguous original model state at 0x080d8820, in address order. Raw units
// are Hz, H, ohms and F. Quality factors and shunts preserve the legacy formulas.
struct PiEquivalent {
    double frequency,inductance,resistance,capacitance_second,capacitance_first,
        resistance_second,resistance_first,quality_first,quality_second,quality_differential,
        shunt_single,shunt_differential;
};
static_assert(sizeof(PiEquivalent)==96);
// Originals 0x0804e354 and 0x0804ec50.
long double pi_shunt_resistance(const PiEquivalent& pi,int differential);
long double pi_resonant_frequency(const PiEquivalent& pi);
// Original 0x08089e40, including both shunt estimates and differential quality.
PiEquivalent extract_pi_equivalent(const TwoPortAdmittance& y,double frequency_hz);
}
