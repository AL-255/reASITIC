#pragma once
#include "soft_real.hpp"
extern "C" long double ldexpl(long double,int);
namespace asitic::x87 {
SoftReal sin(SoftReal);
SoftReal cos(SoftReal);
SoftReal atan2(SoftReal,SoftReal);
SoftReal log(SoftReal);
SoftReal log1p(SoftReal);
SoftReal log2(SoftReal);
SoftReal log2p1(SoftReal);
SoftReal pow2m1(SoftReal);
SoftReal tan(SoftReal);
inline SoftReal load_float_word(unsigned word) { float value;__builtin_memcpy(&value,&word,4);return SoftReal(value); }
inline unsigned comparison(SoftReal a,SoftReal b) { return a!=a || b!=b ? 0x45 : a<b ? 1 : a==b ? 0x40 : 0; }
inline SoftReal round(SoftReal x) { return SoftReal(extF80_roundToInt(x.value,softfloat_round_near_even,false)); }
inline SoftReal floor(SoftReal x) { return SoftReal(extF80_roundToInt(x.value,softfloat_round_min,false)); }
inline SoftReal ceil(SoftReal x) { return SoftReal(extF80_roundToInt(x.value,softfloat_round_max,false)); }
inline int truncate_int(SoftReal x) { return extF80_to_i32(x.value,softfloat_round_minMag,false); }
inline long long truncate64(SoftReal x) { return static_cast<long long>(x); }
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
