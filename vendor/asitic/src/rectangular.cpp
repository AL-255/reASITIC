#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
// 0x0804cb90, decoded against the instructions, not just exported C.
// The compiler used one binary64 stack slot as a running accumulator.
// These stores and the order of additions are part of the algorithm's ABI.
long double rectangular_self_inductance(double width, double height, double length) {
    using x87::binary64;
    using x87::log;
    using x87::sqrt;
    const long double a = static_cast<long double>(width) / height;
    const long double b = static_cast<long double>(length) / height;
    const long double a2 = a*a;
    const double a3 = binary64(a2*a);
    const double ab = binary64(a*b);
    const long double b2 = b*b;
    const double bd = binary64(b);
    const double ra = binary64(sqrt(a2+1.0L));
    const double rb = binary64(sqrt(b2+1.0L));
    const double rab = binary64(sqrt(a2+b2));
    const long double r = sqrt((b2+1.0L)+a2);
    const double lab = binary64(log((r+1.0L)/rab));
    const double la = binary64(log((bd+r)/ra));
    const double lb = binary64(log((a+r)/rb));
    const long double a60 = a*60.0L;
    const double a60d = binary64(a60);

    const double n13 = binary64((log((static_cast<long double>(rb)+1.0L)/bd)-lab)*b2);
    double acc = binary64(static_cast<long double>(n13)/(a*24.0L));
    const long double t12 = (log(static_cast<long double>(bd)+rb)-la)
                         / (static_cast<long double>(ab)*24.0L);
    const long double t14 = ((r-rab)*b2)/a60;
    acc = binary64((t12+acc)+t14);
    const long double t11 = ((log((a+rab)/bd)-lb)*b2)/24.0L;
    acc = binary64(t11+acc);
    const long double t10 = (b2*(static_cast<long double>(bd)-rb))/a60d;
    acc = binary64(t10+acc);
    const long double t9 = (static_cast<long double>(rb)-r)/(a*20.0L);
    const long double t8 = a*lab*0.25L;
    acc = binary64((t9+acc)+t8);
    const long double atan1 = x87::atan2(bd/(a*r),1.0L)*a2/(bd*6.0L);
    acc = binary64(acc-atan1);
    const long double t7 = (a*la)/(4.0L*bd);
    acc = binary64(t7+acc);
    const long double atan2 = (bd*x87::atan2(a/(bd*r),1.0L))/6.0L;
    acc = binary64(acc-atan2);
    const long double third_angle = x87::atan2(static_cast<long double>(ab)/r,1.0L);
    acc = binary64(static_cast<long double>(lb)*0.25L+acc);
    acc = binary64(acc-third_angle/(bd*6.0L));
    const long double t6 = (log(a+ra)-lb)/(b2*24.0L);
    acc = binary64(t6+acc);
    const long double t5 = (a*(static_cast<long double>(ra)-r))/(b2*20.0L);
    acc = binary64(t5+acc);
    const long double b60 = b2*60.0L;
    const long double t4 = (1.0L-rb)/(b60*a);
    const long double t15 = (r-ra)/(b2*a60d);
    acc = binary64((t4+acc)+t15);
    const long double t3 = (a*(static_cast<long double>(rab)-r))/20.0L;
    acc = binary64(t3+acc);
    const long double t2 = ((log((static_cast<long double>(ra)+1.0L)/a)-lab)*a3)/(b2*24.0L);
    acc = binary64(t2+acc);
    const long double t1 = ((log((static_cast<long double>(bd)+rab)/a)-la)*a3)/(bd*24.0L);
    const long double t16 = (((a-rab)+(r-ra))*a3)/binary64(b60);
    acc = binary64((t1+acc)+t16);
    return (8.0L*width)*acc;
}
}
