#pragma once

namespace asitic::quadpack {
// The 1.5 power used by QUADPACK's error estimator, preserving the i386
// audited pow@GLIBC_2.0 x87 algorithm. Its scalar result is rounded to binary64.
double error_power(double x);
using Integrand=long double(*)(const double*);
// absolute is the absolute integral estimate; ascendant is the integral of
// absolute deviation from the weighted mean (QUADPACK's RESASC).
struct RuleResult { double integral,error,absolute,ascendant; };
// DQK15I on a transformed subinterval of (0,1). direction is -1, 1 or 2.
RuleResult infinite_rule(Integrand f,double bound,int direction,double a,double b);
// DQPSRT uses one-based interval indices and a one-based selected rank.
void sort_errors(int limit,int last,int& maximum,double& error,const double* errors,int* order,int& rank);
// DQELG: table has at least 52 cells, history has three, and 1<=length<=50.
void extrapolate(int& length,double* table,double& result,double& error,double* history,int& calls);
struct InfiniteWorkspace {
    double *left,*right,*integrals,*errors;
    int* order;
    int limit;
};
struct IntegralResult { double integral,error;int evaluations,status,intervals; };
// DQAGIE; all workspace arrays have at least limit cells (limit>=1).
// Status codes preserve QUADPACK, including a possibly useful partial result.
IntegralResult integrate_infinite(Integrand f,double bound,int direction,double absolute_tolerance,
                                  double relative_tolerance,InfiniteWorkspace workspace);
using Weight=long double(*)(const double*,const double*,const double*,const double*,const double*,const int*);
// DQK15W passes its four parameter pointers and selector through unchanged.
RuleResult weighted_rule(Integrand f,Weight weight,const double* p1,const double* p2,
                          const double* p3,const double* p4,const int* selector,double a,double b);
// DGTSL: c, d, e and rhs each have n cells, n>=1. All are overwritten.
int solve_tridiagonal(int n,double* c,double* d,double* e,double* rhs);
// DQCHEB: cosine table has 11 cells; samples has 25 (endpoints pre-halved).
// samples is overwritten. The two output arrays have 13 and 25 cells.
void chebyshev_transform(const double* cosines,double* samples,double* degree12,double* degree24);
// The original oscillatory kernels import binary64 libm sin/cos. The embedding
// application supplies those standard runtime operations; x87 return precision
// is retained until each original store. These are not ASITIC function bridges.
struct Trigonometry { long double(*sine)(double);long double(*cosine)(double); };
struct MomentCache { double* data;int rows;int computed; };
struct OscillatoryRuleResult { RuleResult value;int evaluations; };
// DQC25F. Cache storage is column-major, rows*25 cells. Reuse refers to an
// already-computed level or the just-computed final workspace row when full.
OscillatoryRuleResult oscillatory_rule(Integrand f,double a,double b,double frequency,int selector,
    int level,bool reuse,MomentCache& cache,Trigonometry math);
struct OscillatoryWorkspace { InfiniteWorkspace intervals;int* levels; };
// DQAWOE. sequence==1 resets the moment cache; later calls reuse it for the
// same interval length/frequency. Interval workspaces are reset on every call.
IntegralResult integrate_oscillatory(Integrand f,double a,double b,double frequency,int selector,
    double absolute_tolerance,double relative_tolerance,int sequence,OscillatoryWorkspace workspace,
    MomentCache& cache,Trigonometry math);
struct FourierWorkspace {
    OscillatoryWorkspace cycle;
    double *integrals,*errors;
    int *statuses,cycle_limit;
    double* moments;
    int moment_rows;
    // Three history cells. DQELG can skip writes on short/converged tables;
    // DQAWFE may subsequently read those old bytes. Keep this state explicit.
    double* extrapolation_history;
};
struct FourierResult { double integral,error;int evaluations,status,cycles; };
// DQAWFE. Per-cycle arrays have cycle_limit cells; cycle and moment workspace
// are reused. frequency==0 follows the original special cases (bound zero).
FourierResult integrate_fourier(Integrand f,double start,double frequency,int selector,
    double absolute_tolerance,FourierWorkspace workspace,Trigonometry math);
struct PackedWorkspace {
    double* real;
    int real_length;
    int* integer;
    int integer_length;
};
// XERROR runtime boundary: message, explicit Fortran length, error number and
// level (1 for invalid input, 0 for other abnormal returns).
struct Diagnostic {
    void (*emit)(void*,const char*,int,int,int);
    void* context;
};
// DQAGI partitions real workspace into four limit-cell arrays. Integer storage
// must hold limit cells; its length is not an argument in the original API.
void integrate_infinite_packed(Integrand f,double bound,int direction,double absolute_tolerance,
    double relative_tolerance,int limit,PackedWorkspace workspace,IntegralResult& result,Diagnostic diagnostic);
// DQAWF partitions real/integer workspaces and validates their declared sizes.
// On a workspace error result.cycles retains its incoming value, as in ASITIC.
// Nonzero-frequency calls require the same explicit history/runtime as DQAWFE.
void integrate_fourier_packed(Integrand f,double start,double frequency,int selector,double absolute_tolerance,
    int cycle_limit,int moment_rows,PackedWorkspace workspace,FourierResult& result,
    double* history,Trigonometry math,Diagnostic diagnostic);
}
