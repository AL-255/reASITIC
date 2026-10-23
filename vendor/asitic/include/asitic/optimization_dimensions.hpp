#pragma once
#include "asitic/impedance.hpp"

namespace asitic {
// Native 0x08072a80: square selector 0 uses truncated signed 32-bit integer
// quarter counts and subtracts a quarter turn; polygon selector 1 retains the
// extended cosine/radius result. Other selectors warn and return -1.
long double fit_spiral_turns(double outer_dimension,double spacing,double width,
                             int sides,int shape_type,WarningSink warnings={});
}
