#pragma once
#include "asitic/impedance.hpp"
namespace reasitic {
// The native edge grading uses floor(0.4*ny) edge cells and divides the
// middle-cell width by that count. With ny < 3 and active grading the
// original produces an infinite width. Reject that mesh before solving;
// the native -1/-1 history override to 3x3 remains valid and unchanged.
template<class Real> bool valid_width_grading(const asitic::InductanceContext& context,double cell,int maximum){
    for(int i=0;i<context.segment_count;++i)
        if(context.width_counts[i]<3 && Real(context.segments[i]->width)/cell>maximum)return false;
    return true;
}
}
