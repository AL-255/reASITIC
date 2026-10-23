#pragma once
#include "asitic/impedance.hpp"

namespace asitic {
struct PotentialLayers { const ComplexValue* coefficients;const double* depths; };
// Originals 0x0808e908 and 0x0808ed48: zero spatial-frequency terms of the
// layered potential. Accessed layers must exist, including negative indices.
ComplexValue potential_zero_a(int source_layer,int target_layer,double height,const PotentialLayers& layers);
// The original ignores source_layer and source_height in this branch.
ComplexValue potential_zero_b(int source_layer,int target_layer,double source_height,double target_height,const PotentialLayers& layers);
#pragma pack(push,4)
struct PotentialContact {
    unsigned char prefix[32];int x0,x1,y0,y1,metal,index;unsigned next_address;
};
#pragma pack(pop)
static_assert(sizeof(PotentialContact)==60);
struct PotentialGeometry { int nx,ny;double width,height;const MetalLayer* metals;PotentialLayers layers; };
struct PotentialTables { const ComplexValue* x;const ComplexValue* y;const ComplexValue* xy; };
struct PotentialContactGroup {
    char name[80];double start_x,end_x,start_y,end_y;
    int width_cells,length_cells,metal,reserved;
    PotentialContact* first;PotentialContactGroup* next;
};
static_assert(sizeof(PotentialContactGroup)==136);
// Original 0x080901c0. Contacts contain grid indices. The combined cache uses
// row stride nx+1, including when nx and ny differ. Every accessed cell exists.
ComplexValue contact_potential(const PotentialContact& a,const PotentialContact& b,
    const PotentialGeometry& geometry,const PotentialTables& tables);
// Original 0x0809002c. Writes the full matrix and numbers contacts in linked
// group/next order. Cache entries use the original upper packed metal index.
void fill_contact_potential(ComplexMatrixView out,PotentialContactGroup* groups,int metal_count,
    const PotentialGeometry& geometry,const PotentialTables* packed_tables);
}
