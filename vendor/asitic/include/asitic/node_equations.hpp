#pragma once
#include "asitic/network.hpp"
#include "asitic/solver.hpp"

namespace asitic {
// Dense arithmetic helpers used by the original combined-network solver.
// The source/export names describe node stamping, but these are raw arithmetic.
// Original 0x0806cc30 interprets rows contiguously: with a column-major view it
// computes transpose(matrix)*x. It executes all products, including zero x.
void network_matrix_vector(const ComplexValue* matrix,const ComplexValue* x,ComplexValue* out,int n);
// Originals 0x0806df14/0x0806de64: addition/subtraction in increasing order.
// Inputs are loaded before each output write, preserving in-place use.
void add_network_vectors(const ComplexValue* a,const ComplexValue* b,ComplexValue* out,int n);
void subtract_network_vectors(const ComplexValue* a,const ComplexValue* b,ComplexValue* out,int n);
// Original 0x0806dfc4: column-major n*n addition, with rows in the outer loop.
void add_network_matrices(const ComplexValue* a,const ComplexValue* b,ComplexValue* out,int n);
// Original 0x08054a08: backward column differences, column-zero copying, then
// backward row differences. Implements D*matrix*D^T and permits out==matrix.
void difference_network_matrix(const ComplexValue* matrix,ComplexValue* out,int n);
// Originals 0x08056148/0x08056198. The first/last vector cell is left intact.
void backward_network_difference(ComplexValue* values,int n);
void forward_network_difference(ComplexValue* values,int n);
// Original raw wrappers 0x0806dc80/0x0806dc2c, using dense n-stride storage.
// The solve wrapper returns zero even on success, matching native ZGETRS.
bool factor_network_matrix(ComplexValue* matrix,int* pivots,int n,WarningSink warnings = {});
int solve_network_matrix(ComplexValue* lu,ComplexValue* rhs,const int* pivots,int n,int right_hand_sides,WarningSink warnings = {});
struct NodeEquationOptions {
    bool progress = false;
    int (*format)(char*,const char*,...) = nullptr;
    WarningSink warnings;
    void (*snapshot)(const char*,ComplexMatrixView,int,void*) = nullptr;
    void* snapshot_user = nullptr;
};
// Original 0x08053e00: overwrite the first dense matrix with row-sum diagonals
// and negative off-diagonals, assemble/factor the network system, solve both
// port excitations and write the two-port result after ordered work release.
// Factor/solve failures are reported and ignored, as in the original.
bool solve_node_equations(ComplexValue* capacitance,const ComplexValue* inductance,int n,
    TwoPortAdmittance& output,SolverMemory memory,const NodeEquationOptions& options = {});
}
