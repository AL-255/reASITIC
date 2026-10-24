// Guard-precision evaluation of ASITIC's x87 elementary instructions.
// Binary128 work avoids the binary64 fallback in Emscripten's logl/exp2l.
// Final results round to binary80, with FLDLN2 and Intel's internal trig Pi.
// This is tested emulation, not a claim of universal x87 microcode equality.
#include "x87_port.hpp"
namespace {
using Q=float128_t;using R=SoftReal;
const Q one={{0,0x3fff000000000000ULL}},ln2={{0xf35793c7673007e6ULL,0x3ffe62e42fefa39eULL}};
const Q pi={{0x8469898cc51701b8ULL,0x4000921fb54442d1ULL}};
const Q trig_pi={{0x8469800000000000ULL,0x4000921fb54442d1ULL}};
Q neg(Q x) { x.v[1]^=0x8000000000000000ULL;return x; }
Q sub(Q a,Q b) { return f128_add(a,neg(b)); }
Q integer(int n) { return i32_to_f128(n); }
Q inverse_odd[65];bool planned=false;
void plan() { if(planned)return;for(int i=0;i<65;++i)inverse_odd[i]=f128_div(one,integer(2*i+1));planned=true; }
R invalid() { return asitic::x87::sqrt(R(-1)); }
bool is_nan(R x) { return (x.value.signExp&0x7fff)==0x7fff && (x.value.signif&0x7fffffffffffffffULL); }
R retain(Q x) { return R(f128_to_extF80(x)); }
Q wide(R x) { return extF80_to_f128(x.value); }
Q native_ln2() { return extF80_to_f128(extFloat80_t{0xb17217f7d1cf79acULL,0x3ffe}); }
Q odd_series(Q z) {
    plan();const Q square=f128_mul(z,z);Q sum=inverse_odd[64];
    for(int i=63;i>=0;--i)sum=f128_add(inverse_odd[i],f128_mul(square,sum));
    return f128_mul(integer(2),f128_mul(z,sum));
}
Q logarithm2(R input) {
    const int exponent=input.value.signExp&0x7fff;R normalized=input;int power=exponent-16383;
    if(!exponent) { power=-16382;while(!(normalized.value.signif&0x8000000000000000ULL)) { normalized.value.signif<<=1;--power; } }
    normalized.value.signExp=0x3fff;
    const Q m=wide(normalized),z=f128_div(sub(m,one),f128_add(m,one));
    return f128_add(integer(power),f128_div(odd_series(z),ln2));
}
Q atan_positive(Q value,bool reciprocal) {
    if(reciprocal)value=f128_div(one,value);
    const bool shifted=retain(value)>R(0.41421356237309503);
    const Q z=shifted?f128_div(sub(value,one),f128_add(value,one)):value;
    plan();const Q square=neg(f128_mul(z,z));Q sum=inverse_odd[64];
    for(int i=63;i>=0;--i)sum=f128_add(inverse_odd[i],f128_mul(square,sum));
    Q result=f128_mul(z,sum);
    if(shifted)result=f128_add(f128_div(pi,integer(4)),result);
    return reciprocal?sub(f128_div(pi,integer(2)),result):result;
}
R trig(R input,bool cosine) {
    const int exponent=input.value.signExp&0x7fff;
    if(exponent==0x7fff)return invalid();
    // FSIN/FCOS leave ST(0) unchanged and set C2 outside their native range.
    if(asitic::x87::abs(input)>=R(0x1p63L))return input;
    if(!input.value.signif)return cosine?R(1):input;
    const bool negative=input.value.signExp&0x8000;
    const Q magnitude=wide(asitic::x87::abs(input)),half=f128_div(trig_pi,integer(2));
    const R quotient=retain(f128_div(magnitude,half));
    const R rounded(extF80_roundToInt(quotient.value,softfloat_round_near_even,false));
    const long long quadrant=static_cast<long long>(rounded);
    const Q reduced=sub(magnitude,f128_mul(wide(R(quadrant)),half));
    const Q x=f128_div(f128_mul(reduced,pi),trig_pi),square=neg(f128_mul(x,x));
    const int quarter=static_cast<int>(quadrant&3);
    const bool evaluate_cosine=cosine?!(quarter&1):quarter&1;
    Q term=evaluate_cosine?one:x,sum=term;
    for(int i=1;i<=40;++i) {
        term=f128_div(f128_mul(term,square),integer(evaluate_cosine?(2*i-1)*(2*i):(2*i)*(2*i+1)));
        sum=f128_add(sum,term);
    }
    const bool negate_result=cosine?(quarter==1 || quarter==2):(quarter==2 || quarter==3);
    if(negate_result!=(!cosine && negative))sum=neg(sum);
    return retain(sum);
}
}
namespace asitic::x87 {
SoftReal log(SoftReal input) {
    if(is_nan(input))return input;
    if(!input.value.signif)return R(extF80_div(R(-1).value,R(0).value));
    if(input.value.signExp&0x8000)return invalid();
    if((input.value.signExp&0x7fff)==0x7fff)return input;
    return retain(f128_mul(native_ln2(),logarithm2(input)));
}
SoftReal log2(SoftReal input) {
    if(is_nan(input))return input;
    if(!input.value.signif)return R(extF80_div(R(-1).value,R(0).value));
    if(input.value.signExp&0x8000)return invalid();
    if((input.value.signExp&0x7fff)==0x7fff)return input;
    return retain(logarithm2(input));
}
SoftReal log1p(SoftReal input) {
    if(is_nan(input) || !input.value.signif)return input;
    if(input<=R(-1))return input==R(-1)?R(extF80_div(R(-1).value,R(0).value)):invalid();
    // FYL2XP1 callers select the native small-argument interval. Keeping 1+x
    // in guard precision is essential near zero; do not first store it in R.
    const Q value=wide(input),z=f128_div(value,f128_add(integer(2),value));
    return retain(f128_mul(f128_div(odd_series(z),ln2),native_ln2()));
}
SoftReal atan2(SoftReal y,SoftReal x) {
    if(is_nan(x) || is_nan(y))return invalid();
    const R ax=abs(x),ay=abs(y);const bool negative_y=y.value.signExp&0x8000,negative_x=x.value.signExp&0x8000;
    Q angle;
    if(!ay.value.signif)angle=negative_x?pi:integer(0);
    else if(!ax.value.signif)angle=f128_div(pi,integer(2));
    else if((ax.value.signExp&0x7fff)==0x7fff && (ay.value.signExp&0x7fff)==0x7fff) { angle=f128_div(pi,integer(4));if(negative_x)angle=sub(pi,angle); }
    else {
        const bool reciprocal=ay>ax;const Q ratio=f128_div(wide(reciprocal?ax:ay),wide(reciprocal?ay:ax));
        angle=atan_positive(ratio,false);if(reciprocal)angle=sub(f128_div(pi,integer(2)),angle);if(negative_x)angle=sub(pi,angle);
    }
    if(negative_y)angle=neg(angle);return retain(angle);
}
SoftReal sin(SoftReal x) { return trig(x,false); }
SoftReal cos(SoftReal x) { return trig(x,true); }
SoftReal pow2m1(SoftReal input) {
    if(is_nan(input) || !input.value.signif)return input;
    // Original exponent reduction passes [-0.5,0.5], within F2XM1's [-1,1].
    const Q x=f128_mul(wide(input),ln2);Q term=x,sum=x;
    for(int i=2;i<=48;++i) { term=f128_div(f128_mul(term,x),integer(i));sum=f128_add(sum,term); }
    return retain(sum);
}
}
