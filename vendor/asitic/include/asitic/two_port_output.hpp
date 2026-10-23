#pragma once
#include "asitic/network.hpp"
namespace asitic {
struct TwoPortScattering { ComplexValue s11,s12,s21,s22; };
// Original 0x08087d14, including the historical diagonal formulas and
// binary64 spills around each legacy complex division.
TwoPortScattering convert_two_port_scattering(const TwoPortAdmittance&);
struct TwoPortOutputState {
    TwoPortAdmittance admittance;
    TwoPortScattering scattering;
    // Native scalar bank 0x080d8828 through 0x080d8868, in address order.
    double model[9];
};
// Uses the native i386 cdecl fprintf ABI. Saved floating payloads are
// marshalled as their two 32-bit stack words to avoid extra floating loads.
using PortOutputPrint = int (*)(void*,const char*,...);
// Original 0x08088c9c. Parameters 1/2 emit S/Y; 4/5 emit Pi/transformer
// scalar records. Native polar phase arithmetic is retained verbatim.
void write_two_port_record(void* file,double frequency,int parameter,unsigned char polar,
    TwoPortOutputState&,PortOutputPrint);
}
