#pragma once
#include "asitic/optimization.hpp"
namespace reasitic {
using GridCandidateSink=bool (*)(const asitic::GeometryShape&,unsigned,void*);
// Extracted original SWEEP iteration. No candidate dimensions pass through a
// second command parser or grid snap after the original range normalization.
bool enumerate_spiral_grid(const asitic::OptimizationSweepState&,double,double,GridCandidateSink,void*);
bool accept_spiral_grid(double,double,double,double,double);
}
