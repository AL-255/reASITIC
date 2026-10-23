#pragma once
#include "asitic/impedance.hpp"
#include "asitic/lapack.hpp"

namespace asitic {
// Original MV_ColMat<complex<double>> layout. References retain caller-owned
// storage; owned matrices may be resized by the solver.
struct ComplexMatrix {
    ComplexValue* data = nullptr;
    int elements = 0;
    int vector_reference = 0;
    int rows = 0;
    int columns = 0;
    int stride = 0;
    int matrix_reference = 0;
    ComplexMatrixView view() const { return {data,stride}; }
};
static_assert(sizeof(ComplexMatrix)==28);
struct SolverMemory {
    ArrayAllocator arrays;
    void (*release)(void*,void* user);
    void put(const void* p) const { if(p && release)release(const_cast<void*>(p),arrays.user); }
};
struct InductanceTimer { int accumulated,first[2],last[2],zone[2]; };
// Original 0x08094a5c: signed microsecond division, with 32-bit wrapped
// subtraction/multiplication. The accumulated field is not part of its return.
int inductance_elapsed_milliseconds(const InductanceTimer& timer);
struct InductanceReporting {
    bool progress=false,timing=false;
    int (*time)(int*,int*,void*)=nullptr;
    int (*format)(char*,const char*,...)=nullptr;
    WarningSink status;
    char* buffer=nullptr;
    void* user=nullptr;
};
struct InductanceRunOptions {
    FilamentSettings filaments;
    double thickness_cell;
    int metal_count;
    const ViaLayer* vias;
    bool impedance = false;
    // Observes the same stages as Zbig.out, Ysmall.out and Zsmall_eddy.out.
    void (*snapshot)(const char*,const ComplexMatrix&,void*) = nullptr;
    void* snapshot_user = nullptr;
    InductanceReporting reporting={};
};
// Original inverse wrapper with its runtime vectors and cleanup order.
// Dense-vector allocations must succeed, as required by the original runtime.
bool invert_matrix_allocated(ComplexMatrix& matrix,bool mirror,SolverMemory memory,
                              WarningSink warnings = {});
// Original general inverse wrapper 0x0806d974, including vector allocation,
// scratch initialization, release order and factor-failure cleanup callback.
// Runtime vector allocations must succeed, as required by the original.
bool invert_general_allocated(ComplexMatrix& matrix,SolverMemory memory,
                                lapack::GeneralInverseRuntime runtime = {});
// Original 0x08064360 with g_eddy_current_enabled == 0. This integrates linked
// list setup, meshing, fill, inversion, reduction and via corrections. Shapes
// must have at least one conductor and valid, positive meshing counts. Optional
// progress/timing uses the reporting callbacks; file serialization is external. Use the
// general entry below for eddy dispatch.
bool solve_inductance_without_eddy(const ShapeFilamentList& shape,ComplexMatrix& output,
                                    double frequency,const InductanceRunOptions& options,
                                    SolverMemory memory,WarningSink warnings = {});
struct EddyRunOptions;
// Full numerical driver at 0x08064360, including both eddy dispatch paths.
// Eddy technology, integration state and optional Eddy.out snapshot are explicit.
bool solve_inductance(const ShapeFilamentList& shape,ComplexMatrix& output,double frequency,
    const InductanceRunOptions& options,const EddyRunOptions& eddy,SolverMemory memory,WarningSink warnings = {});
}
