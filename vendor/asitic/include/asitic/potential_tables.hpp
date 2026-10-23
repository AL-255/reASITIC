#pragma once
#include "asitic/potential.hpp"
#include "asitic/solver.hpp"

namespace asitic {
// Original 0x0809123c, despite the generated 'clipped_pow2_x' name.
// Returns a binary64-rounded cosh for x <= 500 (including unordered inputs),
// or the original fixed 1e15 for x > 500. Negative x is not clipped.
long double potential_clipped_cosh(double x);
// Original 0x080912c0, positive-sign unnormalized radix-2 FFT. For n > 0,
// storage contains at least the next power-of-two number of complex cells;
// this also preserves the binary's accesses for non-power-of-two lengths.
// n <= 1 leaves storage untouched. Uses the original FSIN/twiddle recurrence.
void potential_fft(ComplexValue* values,int n);
// Original 0x08091548: even-reflect each row, FFT, retain ny+1 cells, then
// even-reflect each column, FFT, retain nx+1 cells. Grid is x-major with
// stride ny+1. Scratch has at least max(2*nx,2*ny,nx+1,ny+1) cells for
// power-of-two nx/ny. Zero dimensions still copy their endpoint cells.
void transform_potential_grid(ComplexValue* grid,int nx,int ny,ComplexValue* scratch);
// Original allocation boundary requests only 2*nx cells, even if ny > nx.
// Embedders must provide sufficient backing storage for original accesses.
// The null check follows scratch initialization: nx > 0 allocation failure
// faults before the diagnostic; nx == 0 invokes the fatal runtime boundary.
void transform_potential_grid_allocated(ComplexValue* grid,int nx,int ny,SolverMemory memory);

struct PotentialTableLayers {
    const ComplexValue* coefficients;
    double* depths;
    PotentialLayers view() const { return {coefficients,depths}; }
};
// Regional Green transfer terms, originals 0x0808f004 and 0x0808f80c.
// Indices may be negative when the backing layer arrays contain them.
ComplexValue potential_transfer_up(int last,double k,const PotentialLayers& layers);
ComplexValue potential_transfer_down(int first,int last,double k,const PotentialLayers& layers);
// Originals 0x0808f3f0 and 0x0808fc04. Temporarily replace depth[source+1]
// or depth[source] with height, run the transfer, and restore the x87-saved
// value. The original loads/stores quiet signaling NaNs during this sequence.
ComplexValue potential_source_up(int source,double k,double height,const PotentialTableLayers& layers);
ComplexValue potential_source_down(int source,int last,double k,double height,const PotentialTableLayers& layers);
// Original regional spectral Green functions 0x0808cc90/0x0808dad4.
ComplexValue potential_spectral_a(int source,int target,int last,double k,double source_height,double target_height,const PotentialTableLayers& layers);
ComplexValue potential_spectral_b(int source,int target,int last,double k,double source_height,double target_height,const PotentialTableLayers& layers);

struct PotentialTableGeometry {
    int nx,ny;
    double width,height;
    const MetalLayer* metals;
    PotentialTableLayers layers;
    int layer_count;
    PotentialGeometry view() const { return {nx,ny,width,height,metals,layers.view()}; }
};
struct PotentialTableOutput { ComplexValue* x;ComplexValue* y;ComplexValue* xy; };
// Original 0x0808c350: generate native combined and single-axis Green tables,
// apply the original integer normalization, reflections and FFTs. Outputs
// need 2*nx/2*ny axis cells (at least one for zero dimensions). Combined-grid
// generation uses nx+1 stride, while its transform uses ny+1; backing storage
// must contain every original access, including nonsquare-grid padding.
void generate_potential_tables(int source,int target,const PotentialTableGeometry& geometry,
    PotentialTableOutput out,SolverMemory memory,WarningSink status = {});
}
