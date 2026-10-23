#pragma once
#include "asitic/three_port.hpp"
namespace asitic {
struct ThreePortAdmittance { ComplexValue y11,y12,y13,y21,y22,y23,y31,y32,y33; };
struct ThreePortScattering { ComplexValue s11,s12,s13,s21,s22,s23,s31,s32,s33; };
// Original 0x0806cd88, a row-contiguous dense product despite its exported
// identity-builder label. All products execute, and each accumulated cell
// rounds to binary64 after every term. Inputs/output may overlap as native.
void multiply_network_dense(const ComplexValue* first,const ComplexValue* second,ComplexValue* output,int n);
// Originals 0x080881a8 and 0x080884b8. Their output labels swap the first
// two ports when scattering the computed 3x3 cells into native globals.
// Allocation zeroes storage before checking null. General-inverse failure is
// reported but ignored; callers can supply its native contact-cleanup boundary.
void convert_three_port_admittance(const ThreePortImpedance& input,ThreePortAdmittance& output,
    SolverMemory memory,lapack::GeneralInverseRuntime runtime = {});
void convert_three_port_scattering(const ThreePortImpedance& input,ThreePortScattering& output,
    SolverMemory memory,lapack::GeneralInverseRuntime runtime = {});
}
