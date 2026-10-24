#pragma once
#include "soft_real.hpp"
extern "C" long double sinl(long double);
extern "C" long double cosl(long double);
extern "C" long double atan2l(long double,long double);
extern "C" long double logl(long double);
extern "C" long double log1pl(long double);
extern "C" long double exp2l(long double);
extern "C" long double ldexpl(long double,int);
namespace asitic::x87 {
// Arithmetic and sqrt are software binary80. Transcendentals remain a parity
// gap: these use Emscripten's libm, not an emulation of Intel's x87 microcode.
inline SoftReal sin(SoftReal x) { return SoftReal(sinl(x.wide())); }
inline SoftReal cos(SoftReal x) { return SoftReal(cosl(x.wide())); }
inline SoftReal atan2(SoftReal y,SoftReal x) { return SoftReal(atan2l(y.wide(),x.wide())); }
inline SoftReal log(SoftReal x) { return SoftReal(logl(x.wide())); }
inline SoftReal log1p(SoftReal x) { return SoftReal(log1pl(x.wide())); }
inline SoftReal log2(SoftReal x) { return log(x)/SoftReal(0.693147180559945309417232121458176568L); }
inline SoftReal round(SoftReal x) { return SoftReal(extF80_roundToInt(x.value,softfloat_round_near_even,false)); }
inline SoftReal floor(SoftReal x) { return SoftReal(extF80_roundToInt(x.value,softfloat_round_min,false)); }
inline SoftReal ceil(SoftReal x) { return SoftReal(extF80_roundToInt(x.value,softfloat_round_max,false)); }
inline int truncate_int(SoftReal x) { return extF80_to_i32(x.value,softfloat_round_minMag,false); }
inline long long truncate64(SoftReal x) { return static_cast<long long>(x); }
inline SoftReal pow2m1(SoftReal x) { return SoftReal(exp2l(x.wide())-1.L); }
inline SoftReal scale(SoftReal x,SoftReal e) { return SoftReal(ldexpl(x.wide(),truncate_int(e))); }
}

namespace asitic::x87 {
inline SoftReal log2e() { return SoftReal(extFloat80_t{0xb8aa3b295c17f0bcULL,0x3fff}); }
inline SoftReal exp(SoftReal x) { const SoftReal e=x*log2e(),i=round(e);return scale(pow2m1(e-i)+1,i); }
inline SoftReal atanh(SoftReal x) {
    const SoftReal a=abs(x),u=-(a+a)/(a+1);
    const SoftReal crossover=SoftReal(0x95f619980c4336f8p-65L);
    const SoftReal logarithm=abs(u)>=crossover?log(1+u):log1p(u);
    return (SoftReal(-.5)*logarithm)*(x>=0?1:-1);
}
}
