#include "asitic/optimization_radius.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
long double loaded(const double& value) {
    long double result;
    asm volatile("fldl %1" : "=t"(result) : "m"(value));
    return result;
}
double floor_stored(long double value) {
    unsigned short saved,temporary;
    double result;
    asm volatile("fnstcw %0" : "=m"(saved));
    temporary=(saved&0xf3ffu)|0x0400u;
    asm volatile("fldcw %2; frndint; fldcw %3; fstpl %0"
                 : "=m"(result) : "t"(value),"m"(temporary),"m"(saved) : "st");
    return result;
}
}
long double maximum_spiral_turns(double outer_dimension,double spacing,double width,
                                int sides,int shape_type) {
    // The three initial FLDs are observable for signaling NaNs even when the
    // selector takes the fallback branch and discards every input value.
    const long double outer=loaded(outer_dimension);
    const long double gap=loaded(spacing);
    const long double wire=loaded(width);
    long double count;
    if(shape_type==0 || shape_type==3 || shape_type==5 || shape_type==16 || shape_type==18)
        count=((outer+wire)/(wire+gap))*0.5L;
    else if(shape_type==1 || shape_type==17 || shape_type==20)
        count=(outer*x87::cos(0x1.921fb54442d18p+1L/static_cast<long double>(sides)))/(gap+wire);
    else count=1e15L;
    const long double whole=loaded(floor_stored(count));
    const long double second_whole=loaded(floor_stored(count));
    const long double product=(count-second_whole)*static_cast<long double>(sides);
    // Native FST precedes FRNDINT without replacing its extended operand.
    double discarded;
    asm volatile("fstl %0" : "=m"(discarded) : "t"(product));
    const double fraction=floor_stored(product);
    long double result=static_cast<long double>(fraction)/static_cast<long double>(sides)+whole;
    if(static_cast<unsigned>(shape_type)-16u<2u)result=loaded(floor_stored(result));
    return result;
}
}
