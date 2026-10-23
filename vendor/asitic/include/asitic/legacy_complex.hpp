#pragma once
#include "asitic/impedance.hpp"

namespace asitic::legacy {
// Arithmetic used by the bundled libstdc++ 2.9 complex<double> templates.
// These preserve its x87 spills, zero signs and non-fatal division by zero.
ComplexValue square_root(const ComplexValue& z);
ComplexValue divide(const ComplexValue& a,const ComplexValue& b);
ComplexValue divide(double numerator,const ComplexValue& denominator);
ComplexValue exp(const ComplexValue& z);
ComplexValue sinh(const ComplexValue& z);
ComplexValue cosh(const ComplexValue& z);
}
