#pragma once
namespace reasitic {
struct PiShunts { double first_real,first_imag,second_real,second_imag; };
// Column-major physical capacitance/branch admittance matrices. Compute the
// two port row sums by a common-mode drive, without subtracting large Y cells.
bool common_mode_shunts(const double* cap,const double* ind,int n,int cap_stride,int ind_stride,
                       const double* pads,int pad_stride,PiShunts& out);
}
