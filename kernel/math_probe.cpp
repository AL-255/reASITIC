// Test boundary for comparing software results with actual x87 instructions.
#ifdef __EMSCRIPTEN__
#include "x87_port.hpp"
using Real=SoftReal;
namespace x87=asitic::x87;
#else
#include "x87.hpp"
using Real=long double;
namespace x87=asitic::x87;
#endif
extern "C" int sprintf(char*,const char*,...);
extern "C" const char* probe_math(int operation,double a,double b,double c) {
    static char output[512];Real input=a;
    if(operation>=16) {
        // The exact angle sequence used by the original ring constructor.
        volatile double stored_gap=x87::binary64(Real(a)*Real(0x1.1df46a2529d39p-6));
        const double gap=stored_gap;
        input=((Real(0x1.921fb54442d18p+2L)-gap)/(Real(c)-1))*b+Real(gap)*Real(.5);
        operation-=16;
    } else {
        if(c==1)input=Real(a)/b;
        if(c==2)input=Real(a)+b;
        if(c==3)input=Real(a)*b;
    }
    Real value;
    switch(operation) {
        case 0:value=x87::sqrt(input);break;
        case 1:value=x87::log(input);break;
        case 2:value=x87::log1p(input);break;
        case 3:value=x87::atan2(input,b);break;
        case 4:value=x87::sin(input);break;
        case 5:value=x87::cos(input);break;
        case 6:value=x87::exp(input);break;
        case 7:value=x87::pow2m1(input);break;
        case 8:value=input;break;
        default:return "{\"error\":\"Unknown math probe\"}";
    }
    unsigned words[3]{};
#ifdef __EMSCRIPTEN__
    words[0]=static_cast<unsigned>(value.value.signif);words[1]=static_cast<unsigned>(value.value.signif>>32);words[2]=value.value.signExp;
#else
    volatile long double stored=value;const auto* bytes=reinterpret_cast<const volatile unsigned char*>(&stored);
    for(unsigned i=0;i<10;++i)words[i/4]|=static_cast<unsigned>(bytes[i])<<((i%4)*8);
#endif
    const double rounded=x87::binary64(value);
    if(!__builtin_isfinite(rounded))return "{\"error\":\"Nonfinite math probe output\"}";
    sprintf(output,"{\"value\":%.17g,\"low\":%u,\"high\":%u,\"exponent\":%u}",rounded,words[0],words[1],words[2]);return output;
}
