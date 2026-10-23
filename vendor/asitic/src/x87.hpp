#pragma once

namespace asitic::x87 {
inline long double sqrt(long double x) {
    long double r;
    asm("fsqrt" : "=t"(r) : "0"(x));
    return r;
}
inline long double atan2(long double y, long double x) {
    long double r;
    asm("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

// This must use FYL2X, not a libm approximation and not ln(2) * x.
// Ghidra's exported C loses FYL2X in several ASITIC functions.
inline long double log(long double x) {
    long double r;
    asm("fldln2; fxch; fyl2x" : "=t"(r) : "0"(x) : "st(1)");
    return r;
}
inline long double log1p(long double x) {
    long double r;
    asm("fldln2; fxch; fyl2xp1" : "=t"(r) : "0"(x) : "st(1)");
    return r;
}
inline long double cos(long double x) {
    long double r;
    asm("fcos" : "=t"(r) : "0"(x));
    return r;
}
inline long double sin(long double x) {
    long double r;
    asm("fsin" : "=t"(r) : "0"(x));
    return r;
}
inline long double log2e() {
    long double r;
    asm("fldl2e" : "=t"(r));
    return r;
}
inline long double round(long double x) {
    long double r;
    asm("frndint" : "=t"(r) : "0"(x));
    return r;
}
inline long double pow2m1(long double x) {
    long double r;
    asm("f2xm1" : "=t"(r) : "0"(x));
    return r;
}
inline long double scale(long double x, long double exponent) {
    long double r;
    asm("fscale" : "=t"(r) : "0"(x), "u"(exponent));
    return r;
}
inline long double exp(long double x) {
    const long double e = x * log2e();
    const long double i = round(e);
    return scale(pow2m1(e-i)+1.0L, i);
}
inline long double abs(long double x) {
    long double r;
    asm("fabs" : "=t"(r) : "0"(x));
    return r;
}
// Exact inlined atanh sequence found in the planar and spatial kernels.
inline long double atanh(long double x) {
    const long double a=abs(x);
    const long double u=-(a+a)/(a+1.0L);
    constexpr long double crossover=0x95f619980c4336f8p-65L;
    const long double logarithm=abs(u) >= crossover ? log(1.0L+u) : log1p(u);
    return (-0.5L*logarithm)*(x >= 0 ? 1.0L : -1.0L);
}
// Volatile prevents GCC from retaining an extended temporary across an
// original fstp QWORD/fld QWORD rounding boundary.
inline double binary64(long double x) {
    volatile double r = x;
    return r;
}
// Original fistp sequence in the filament-count setup. Preserve the complete
// control word, including its unusual temporary precision-control setting.
inline int truncate_int(long double x) {
    unsigned short saved,temporary;
    int result;
    asm volatile("fnstcw %0" : "=m"(saved));
    temporary=(saved&255u)|0x0c00u;
    asm volatile("fldcw %2; fistpl %0; fldcw %3"
                 : "=m"(result) : "t"(x),"m"(temporary),"m"(saved) : "st");
    return result;
}
}
