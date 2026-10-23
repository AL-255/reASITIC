#pragma once
#include "asitic/filaments.hpp"
#include "asitic/technology.hpp"
#include "asitic/impedance.hpp"

namespace asitic {
// Original 0x0804df6c. Raw width/gap/height units are cm; outputs are F/cm.
// Writes in plate/fringe/inner/air/dielectric order and preserves output aliasing.
void coupled_microstrip_capacitance(double width,double gap,double height,double permittivity,
    double* plate,double* fringe,double* inner,double* air,double* dielectric);
struct CapacitanceTechnology { const MetalLayer* metals;const SubstrateLayer* substrate; };
// Original 0x0804ecac. The metal's substrate index is at +0xa0 and the selected
// substrate's relative permittivity is at +4. Mode 1 selects the coupled pair;
// every other mode selects the alternative edge sums.
void coupled_microstrip_matrix(int mode,double width,double gap,double height,double* self,double* mutual,
    int metal,const CapacitanceTechnology& technology);
struct MicrostripShapeOptions { int sides;double spacing,turns;const double* metal_heights; };
// Original 0x0804dd40. Shape and all accessed layer records must exist; the
// original unconditionally reads its first segment. Geometry units are microns.
// The bucket threshold intentionally uses half the LAST segment's length.
void shape_microstrip_capacitance(const ShapeFilamentList& shape,const MicrostripShapeOptions& options,
    const CapacitanceTechnology& technology,double* first,double* second,double* mutual);
}
