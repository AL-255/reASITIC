#pragma once
#include "asitic/green_integration.hpp"

namespace asitic {
// One-based packed square-spiral index, preserving 32-bit integer wrapping.
int eddy_packed_index(int index,int turns);
long double eddy_folded_position(int index,double dimension,double width,double spacing,int turns);
long double eddy_signed_half_position(int index,double dimension,double width,double spacing,int turns);
long double eddy_signed_separation(int i,int j,double dimension,double width,double spacing,int turns);
// Wire records contain start[3], end[3], and a radius/edge inset at [6].
long double eddy_axial_length(const double* wire);
// Returns 0 (near, almost parallel), 7 (small direction product), or 9.
// The product output is overwritten with a normalized cosine only on the
// near-pair path. Otherwise it retains the original raw direction product.
int eddy_classify_pair(const double* a,const double* b,double* product);
struct EddyEnvironment {
    double coordinate_scale;
    GreenIntegrationState* state;
    GreenIntegrationWorkspace* workspace;
    GreenIntegrationRuntime runtime;
};
long double eddy_axial_term(const double* wire,double frequency,const EddyEnvironment& environment);
long double eddy_pair_term(const double* a,const double* b,double frequency,const EddyEnvironment& environment);
struct RealMatrixView {
    double* data;
    int stride;
    double& at(int row,int column) const { return data[column*stride+row]; }
};
struct SquareSpiralGeometry { double dimension,width,spacing,turns; };
// Original 0x080930a4. out has 2*ceil(turns) rows/columns and a valid stride;
// storage/padding is caller-owned. The original calls Green even for zero turns.
void assemble_eddy_matrix(RealMatrixView out,const SquareSpiralGeometry& geometry,
    double frequency,double height,const EddyEnvironment& environment);
struct SolverMemory;
struct EddySquareOptions {
    int layer_count,shape_kind;
    double metal_height;
    void (*snapshot)(const char*,RealMatrixView,int,int,void*);
    void* snapshot_user;
};
struct EddyRunOptions {
    // Original g_eddy_current_enabled: 0 disables; 1 selects packed stamping
    // for square shapes; every other enabled case uses segment folding.
    int mode,layer_count;
    const double* metal_heights;
    EddyEnvironment environment;
    void (*snapshot)(const char*,RealMatrixView,int,int,void*);
    void* snapshot_user;
};
// Original 0x08092cd0, including temporary real-matrix allocation/cleanup,
// warning exits, optional Eddy.out snapshot and packed complex-matrix stamps.
// Dense allocation must succeed, as required by the original constructor.
void add_eddy_square(ComplexMatrixView out,const SquareSpiralGeometry& geometry,double frequency,
    const EddySquareOptions& options,const EddyEnvironment& environment,SolverMemory memory,WarningSink warnings = {});
// Original 0x08092f30. rows must accommodate all valid conductor diagonals.
// The original off-diagonal scan does not skip intervening via records.
void fold_eddy_segments(const ShapeFilamentList& shape,ComplexMatrixView out,unsigned rows,
    double frequency,int metal_count,const EddyEnvironment& environment);
}
