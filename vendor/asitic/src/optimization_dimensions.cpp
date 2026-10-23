#include "asitic/optimization_dimensions.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
long double loaded(const double& value) {
    long double result;asm volatile("fldl %1" : "=t"(result) : "m"(value));return result;
}
}
long double fit_spiral_turns(double outer_dimension,double spacing,double width,
                             int sides,int shape_type,WarningSink warnings) {
    // Even unsupported selectors load all three doubles before branching.
    const long double outer=loaded(outer_dimension),gap=loaded(spacing),wire=loaded(width);
    if(shape_type==0) {
        const long double sum=wire+gap,ratio=outer/((sum+sum)+1.L);
        const int integer=x87::truncate_int(ratio);
        const long double whole=integer;
        const int quarter=x87::truncate_int(ratio*4.L);
        return (whole+(static_cast<long double>(quarter)-whole*4.L)*0.25L)-0.25L;
    }
    if(shape_type==1) {
        const long double corner=x87::cos(0x1.921fb54442d18p+1L/static_cast<long double>(sides));
        return ((outer-wire)*corner)/(gap+wire)-2.L;
    }
    warnings.emit("Error:  Unknown spiral type in FindMaxN()");return -1.L;
}
}
