#pragma once
namespace reasitic {
// Original two-port S formulas and port order, evaluated from exact dyadic
// coefficients with direct IEEE binary64 quotient rounding. Input is
// y11,y12,y21,y22, in the original struct order. Output is
// s11,s12,s21,s22; each complex cell stores real,imaginary components.
bool two_port_scattering(const double* admittance,double* scattering);
// Original 2PZIN port order: port 1 drives y22. Capacitor 1 shunts y22,
// capacitor 2 shunts y11. Evaluate the loaded nodal equations with exact
// dyadic coefficients and one IEEE binary64 division per result component.
// Status: 0 finite, 1 pole, 2 indeterminate, 3 binary64 overflow.
bool loaded_port_impedance(const double* admittance,const double* load,int port,
                           double frequency,double first_cap,double second_cap,
                           double* impedance,int& status);
struct PiShunts { double first_real,first_imag,second_real,second_imag; };
// Reciprocal Pi circuit: grounded endpoint impedances and differential
// impedance, in the original port order. Evaluate from the physical branches
// rather than a determinant of rounded Y cells. Open circuits may publish
// nonfinite impedances/quality; nonfinite inputs and allocation failure reject.
bool pi_branch_impedances(const double* series,const PiShunts& shunts,
                          double* impedances,double& differential_quality);
// Folded chain: stamp C + D^T*Y*D, fix both endpoint voltages and eliminate
// interior nodes. Optional PI4 pads are subtracted before binary64 rounding.
// Inputs and outputs are column-major interleaved complex values; node_cap is
// the n*n capacitance-only nodal block. with_pads may receive the raw ports.
bool folded_port_network(const double* cap,const double* ind,int n,int cap_stride,int ind_stride,
                         const double* pads,int pad_stride,double* ports,double* node_cap,
                         double* with_pads=nullptr);
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
