#pragma once

namespace asitic {
// Native 0x806c608: quantized maximum turn count allowed by an outer dimension.
// Shape selectors 0/3/5/16/18 use the square formula; 1/17/20 use
// the polygon corner factor. The other selectors retain the native fallback.
long double maximum_spiral_turns(double outer_dimension,double spacing,double width,
                                int sides,int shape_type);
}
