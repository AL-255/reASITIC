#pragma once
#pragma pack(push,8)
extern "C" {
#include "platform.h"
#include "softfloat.h"
}
struct SoftReal {
 extFloat80_t value;
 SoftReal():SoftReal(0.){}
 SoftReal(int x):SoftReal(static_cast<double>(x)){}
 SoftReal(unsigned x):SoftReal(static_cast<double>(x)){}
 SoftReal(long long x):SoftReal(static_cast<long double>(x)){}
 SoftReal(long double x){union{long double d;float128_t f;}u={.d=x};value=f128_to_extF80(u.f);}
 long double wide()const{union{long double d;float128_t f;}u;u.f=extF80_to_f128(value);return u.d;}
 explicit operator int()const{return extF80_to_i32(value,softfloat_round_minMag,false);}
 explicit operator long long()const{
   int e=(value.signExp&0x7fff)-16383;
   if(e<0)return 0;
   if(e>63 || (e==63 && (!(value.signExp&0x8000) || value.signif!=0x8000000000000000ULL)))return (-9223372036854775807LL-1);
   unsigned long long n=value.signif>>(63-e);
   return value.signExp&0x8000?static_cast<long long>(0ULL-n):static_cast<long long>(n);
 }
 SoftReal(double x){union{double d;float64_t f;}u={.d=x};value=f64_to_extF80(u.f);}
 explicit SoftReal(extFloat80_t x):value(x){}
 explicit operator double()const{union{double d;float64_t f;}u;u.f=extF80_to_f64(value);return u.d;}
 SoftReal operator-()const{auto x=value;x.signExp^=0x8000;return SoftReal(x);}
 SoftReal& operator+=(SoftReal x){value=extF80_add(value,x.value);return *this;}
 SoftReal& operator-=(SoftReal x){value=extF80_sub(value,x.value);return *this;}
 SoftReal& operator*=(SoftReal x){value=extF80_mul(value,x.value);return *this;}
 SoftReal& operator/=(SoftReal x){value=extF80_div(value,x.value);return *this;}
};
inline SoftReal operator+(SoftReal a,SoftReal b){return SoftReal(extF80_add(a.value,b.value));}
inline SoftReal operator-(SoftReal a,SoftReal b){return SoftReal(extF80_sub(a.value,b.value));}
inline SoftReal operator*(SoftReal a,SoftReal b){return SoftReal(extF80_mul(a.value,b.value));}
inline SoftReal operator/(SoftReal a,SoftReal b){return SoftReal(extF80_div(a.value,b.value));}
inline bool operator<(SoftReal a,SoftReal b){return extF80_lt(a.value,b.value);}
inline bool operator<=(SoftReal a,SoftReal b){return extF80_le(a.value,b.value);}
inline bool operator==(SoftReal a,SoftReal b){return extF80_eq(a.value,b.value);}
inline bool operator>(SoftReal a,SoftReal b){return b<a;}
inline bool operator>=(SoftReal a,SoftReal b){return b<=a;}
inline bool operator!=(SoftReal a,SoftReal b){return !(a==b);}
namespace asitic::x87{
inline double binary64(SoftReal a){return static_cast<double>(a);}
inline SoftReal abs(SoftReal a){a.value.signExp&=0x7fff;return a;}
inline SoftReal sqrt(SoftReal a){return SoftReal(extF80_sqrt(a.value));}
}

static_assert(sizeof(extFloat80_t)==16);
static_assert(sizeof(long double)==16);
#pragma pack(pop)
