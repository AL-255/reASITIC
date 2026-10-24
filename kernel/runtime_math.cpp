#include "runtime_math.hpp"
extern "C" int sprintf(char*,const char*,...);
#ifdef __EMSCRIPTEN__
extern "C" double reasitic_glibc_sin(double);
extern "C" double reasitic_glibc_cos(double);
namespace reasitic {
SoftReal runtime_sin(double x){return SoftReal(reasitic_glibc_sin(x));}
SoftReal runtime_cos(double x){return SoftReal(reasitic_glibc_cos(x));}
}
using Real=SoftReal;
#else
// Preserve the native ST(0) return word directly for verification. libm's
// scalar kernels select 53-bit precision internally, then restore native PC.
extern "C" long double native_sin(double) asm("sin");
extern "C" long double native_cos(double) asm("cos");
namespace reasitic {
long double runtime_sin(double x){return native_sin(x);}
long double runtime_cos(double x){return native_cos(x);}
}
using Real=long double;
#endif
extern "C" const char* probe_runtime_trig(int cosine,double x) {
    static char result[512];const Real value=cosine?reasitic::runtime_cos(x):reasitic::runtime_sin(x);unsigned words[3]{};
#ifdef __EMSCRIPTEN__
    words[0]=value.value.signif;words[1]=value.value.signif>>32;words[2]=value.value.signExp;const double rounded=static_cast<double>(value);
#else
    volatile long double stored=value;const auto* bytes=reinterpret_cast<const volatile unsigned char*>(&stored);
    for(unsigned i=0;i<10;++i)words[i/4]|=static_cast<unsigned>(bytes[i])<<((i%4)*8);volatile double rounded=value;
#endif
    if(!__builtin_isfinite(rounded))return "{\"error\":\"Nonfinite runtime trig result\"}";
    sprintf(result,"{\"value\":%.17g,\"low\":%u,\"high\":%u,\"exponent\":%u}",rounded,words[0],words[1],words[2]);return result;
}
