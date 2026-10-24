#pragma once
namespace reasitic {
struct PiShunts { double first_real,first_imag,second_real,second_imag; };
// Column-major physical capacitance/branch admittance matrices. Compute the
// two port row sums by a common-mode drive, without subtracting large Y cells.
bool common_mode_shunts(const double* cap,const double* ind,int n,int cap_stride,int ind_stride,
                       const double* pads,int pad_stride,PiShunts& out);
// PIX averages adjacent voltages and currents. Use its unfolded capacitance
// and branch impedance inputs, eliminating currents before the common drive.
bool extended_common_mode_shunts(const double* cap,const double* impedance,int n,
                                int cap_stride,int ind_stride,PiShunts& out);
// PIX port solve after eliminating chain currents. Ports are column-major;
// currents/voltages contain the n+1 states for V(0)=1, V(n)=0. All buffers
// store interleaved real/imaginary binary64 values.
bool extended_port_network(const double* cap,const double* impedance,int n,
                           int cap_stride,int ind_stride,double* ports,
                           double* currents,double* voltages);
}
