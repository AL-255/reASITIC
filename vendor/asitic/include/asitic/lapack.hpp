#pragma once
#include "asitic/impedance.hpp"

namespace asitic::lapack {
// Embedders may override this weak runtime boundary. The freestanding default
// traps; the original libf2c runtime prints the message and terminates.
[[noreturn]] void fatal_error(const char* message);
// Application allocation errors use ASITIC's fatal dialog/cleanup path, unlike
// libf2c's stderr/SIGABRT path. The weak default delegates to fatal_error so
// freestanding embedders can retain a single fatal capture boundary.
[[noreturn]] void application_fatal_error(const char* message);
// XERBLA runtime boundary for the factor/inverse entry points. The weak default
// is silent; an embedding application supplies the original output format.
void argument_error(const char* routine,int parameter);
// libf2c helpers used by the original complex symmetric solver.
long double magnitude(const ComplexValue& z);
long double abs1(const ComplexValue& z);
void divide(ComplexValue& out,const ComplexValue& numerator,const ComplexValue& denominator);
void square_root(ComplexValue& out,const ComplexValue& z);
void integer_power(ComplexValue& out,const ComplexValue& z,int exponent);
// ZLAESY: EVSCAL is untouched when b == 0; CS1 is untouched when scaling fails.
void eigen2(const ComplexValue& a,const ComplexValue& b,const ComplexValue& c,
            ComplexValue& rt1,ComplexValue& rt2,ComplexValue& evscal,
            ComplexValue& cs1,ComplexValue& sn1);
// BLAS increments refer to elements; negative increments start at (1-n)*inc.
// BLAS matrix routines require valid dimensions/strides and nonzero increments.
int max_index(int n,const ComplexValue* x,int inc);
void axpy(int n,const ComplexValue& alpha,const ComplexValue* x,int incx,ComplexValue* y,int incy);
void scale(int n,const ComplexValue& alpha,ComplexValue* x,int inc);
void swap(int n,ComplexValue* x,int incx,ComplexValue* y,int incy);
void rotate(int n,ComplexValue* x,int incx,ComplexValue* y,int incy,
            const ComplexValue& c,const ComplexValue& s);
void rank1_upper(int n,const ComplexValue& alpha,const ComplexValue* x,int inc,ComplexMatrixView a);
void copy(int n,const ComplexValue* x,int incx,ComplexValue* y,int incy);
ComplexValue dotu(int n,const ComplexValue* x,int incx,const ComplexValue* y,int incy);
void multiply_upper(int n,const ComplexValue& alpha,ComplexMatrixView a,
                    const ComplexValue* x,int incx,const ComplexValue& beta,ComplexValue* y,int incy);
// Original upper ZSYTF2 (0x0809bcf8). Returns INFO, with one-based pivots.
// pivots has n cells. Lower triangle is untouched. Invalid dimensions/stride
// return negative INFO and invoke argument_error, matching XERBLA.
int factor_symmetric_upper(int n,ComplexMatrixView a,int* pivots);
// Original upper ZSYTRI (0x08095bf8), applied to a ZSYTF2/ZSYTRF factor.
// Scratch has n complex cells. Returns a one-based zero diagonal, or zero.
int invert_symmetric_upper(int n,ComplexMatrixView a,const int* pivots,ComplexValue* scratch);
// Original ZGEMV with TRANS='N', and ZGEMM with TRANSA='N', TRANSB='T'.
// The latter multiplies a (m by k) by the transpose of b (n by k).
void multiply_general(int m,int n,const ComplexValue& alpha,ComplexMatrixView a,
                      const ComplexValue* x,int incx,const ComplexValue& beta,ComplexValue* y,int incy);
void multiply_transposed_right(int m,int n,int k,const ComplexValue& alpha,
                               ComplexMatrixView a,ComplexMatrixView b,const ComplexValue& beta,
                               ComplexMatrixView out);
// Original upper ZLASYF (0x08098658). block >= 2, n >= 1. Workspace has
// block columns and stride >= n. Returns INFO; columns receives KB.
int factor_panel_upper(int n,int block,ComplexMatrixView a,int* pivots,
                       ComplexMatrixView workspace,int& columns);
// Original upper ZSYTRF (0x0809587c), including its fixed ILAENV tuning:
// preferred block size 64, minimum reduced block size 8. work_size >= 1;
// work[0] receives the original workspace recommendation on normal return.
int factor_symmetric_upper_workspace(int n,ComplexMatrixView a,int* pivots,
                                     ComplexValue* work,int work_size);
// Original inverse wrapper 0x0806d5f0, with caller-owned storage in place of
// its runtime vectors: n pivots, n factor cells and n inverse cells. Its
// incorrectly passed ILAENV string length returns 1 on the original Linux
// stack, so this wrapper forces the unblocked path even for n > 64.
// n >= 0. The original factors dense n-stride storage even if a.stride differs;
// only its optional upper-to-lower copy uses the descriptor stride.
bool invert_symmetric_matrix(int n,ComplexMatrixView a,bool mirror,int* pivots,
                              ComplexValue* factor_work,ComplexValue* inverse_work,
                              WarningSink warnings = {});
// General ZGERU, ZGETF2, and upper/no-transpose ZTRMV and ZTRTI2.
void rank1_general(int m,int n,const ComplexValue& alpha,const ComplexValue* x,int incx,
                   const ComplexValue* y,int incy,ComplexMatrixView a);
int factor_general_unblocked(int m,int n,ComplexMatrixView a,int* pivots);
void multiply_triangular_upper(int n,ComplexMatrixView a,bool unit,ComplexValue* x,int inc);
int invert_triangular_upper_unblocked(int n,ComplexMatrixView a,bool unit);

// ZGEMM NN, ZTRSM with no transpose, and left/upper/no-transpose ZTRMM.
// multiply_matrices requires valid dimensions and strides.
void multiply_matrices(int m,int n,int k,const ComplexValue& alpha,ComplexMatrixView a,
                        ComplexMatrixView b,const ComplexValue& beta,ComplexMatrixView out);
void solve_triangular(int m,int n,const ComplexValue& alpha,ComplexMatrixView a,
                       bool right,bool upper,bool unit,ComplexMatrixView b);
void multiply_triangular_left_upper(int m,int n,const ComplexValue& alpha,ComplexMatrixView a,
                                      bool unit,ComplexMatrixView b);
// ZGETRF and upper ZTRTRI, with the original 64-column blocking.
int factor_general(int m,int n,ComplexMatrixView a,int* pivots);
int invert_triangular_upper(int n,ComplexMatrixView a,bool unit);
// ZGETRI. work_size >= max(1,n); this original has no query mode.
// work[0] is written before validation; INFO and matrix/scratch mutations
// follow the binary, including reduced workspace and singular returns.
int invert_general(int n,ComplexMatrixView a,const int* pivots,ComplexValue* work,int work_size);
// ZLASWP 0x080956dc: one-based first/last rows and pivot indices. The pivot
// increment may be positive, negative or zero; row swaps span columns cells.
void swap_general_rows(int columns,ComplexMatrixView matrix,int first,int last,const int* pivots,int increment);
// ZGETRS 0x080954ec with TRANS='N'. Returns INFO and overwrites the right-hand
// sides after the forward pivot application and two triangular solves.
int solve_general(int n,int right_hand_sides,ComplexMatrixView lu,const int* pivots,ComplexMatrixView rhs);

struct GeneralInverseRuntime {
    WarningSink warnings{};
    void (*cleanup)(void*) = nullptr;
    void* user = nullptr;
};
// Original general inverse wrapper (0x0806d974), with caller-owned n pivots
// and n complex work cells. It factors dense n-stride storage, regardless
// of a.stride. Its malformed ILAENV call selects only n workspace cells.
// Factor failure invokes the capacitance cleanup boundary before warning.
bool invert_general_matrix(int n,ComplexMatrixView a,int* pivots,ComplexValue* work,
                            GeneralInverseRuntime runtime = {});
}
