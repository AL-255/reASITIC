#pragma once
#include "x87.hpp"
namespace asitic::x11_render_math {
inline long double load(const double& value) { long double out;asm volatile("fldl %1":"=t"(out):"m"(value));return out; }
inline const double& number(const void* record,unsigned offset) { return *reinterpret_cast<const double*>(static_cast<const char*>(record)+offset); }
inline unsigned truncate_low64(long double value) {
    unsigned short saved,temporary;unsigned long long result;
    asm volatile("fnstcw %0":"=m"(saved));temporary=(saved&255u)|0x0c00u;
    asm volatile("fldcw %2; fistpq %0; fldcw %3":"=m"(result):"t"(value),"m"(temporary),"m"(saved):"st");return static_cast<unsigned>(result);
}
inline unsigned wrap_add(unsigned a,unsigned b) { return a+b; }
inline int wrap_abs_difference(int a,int b) { unsigned d=static_cast<unsigned>(a)-static_cast<unsigned>(b);if(static_cast<int>(d)<0)d=0u-d;return static_cast<int>(d); }
}
