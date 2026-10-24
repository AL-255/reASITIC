#pragma once
#include "asitic/impedance.hpp"
namespace reasitic {
// Count admission follows the storage of a dense complex matrix. Share this
// policy with the request boundary; JavaScript must reject values before the
// WASM i32 ABI can truncate or wrap them.
constexpr unsigned dense_solver_matrix_bytes=16u*1024u*1024u;
constexpr int dense_solver_count_limit(){int n=0;while(static_cast<unsigned long long>(n+1)*(n+1)<=dense_solver_matrix_bytes/sizeof(asitic::ComplexValue))++n;return n;}
constexpr int maximum_solver_filaments=dense_solver_count_limit();
inline bool dense_solver_count_fits(int n){return n>=1&&static_cast<unsigned long long>(n)*n<=dense_solver_matrix_bytes/sizeof(asitic::ComplexValue);}
constexpr int maximum_green_evaluations=10000000,maximum_port_samples=101;
constexpr int maximum_eddy_mode=2,maximum_eddy_layers=2;
}
