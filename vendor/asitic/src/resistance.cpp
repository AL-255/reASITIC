#include "asitic/resistance.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
constexpr double frequency_factor=0x1.921fb544418ecp+4,unit_scale=0x1.a36e2eb1c432dp-14;
long long truncate64(long double x) {
    unsigned short saved,temporary;long long result;
    asm volatile("fnstcw %0" : "=m"(saved));
    temporary=(saved&255u)|0x0c00u;
    asm volatile("fldcw %2; fistpll %0; fldcw %3" : "=m"(result) : "t"(x),"m"(temporary),"m"(saved) : "st");
    return result;
}
// The emitted powers use a 64-bit integer fast path, otherwise FYL2X followed
// by F2XM1/FSCALE. Their exponents are 1.19, 3, 1.8, or 3+0.01*u*u.
double resistance_power(long double base,long double exponent) {
    const long long integer=truncate64(exponent);
    if(base==0 && exponent>0)return 0;
    long double result;
    if((long double)integer==exponent) {
        unsigned long long remaining=static_cast<unsigned long long>(integer);
        result=1;
        while(remaining) {
            if(remaining&1u)result=base*result;
            remaining>>=1;
            if(remaining)base=base*base;
        }
    } else {
        long double logarithm;
        asm("fyl2x" : "=t"(logarithm) : "0"(base),"u"(1.0L) : "st(1)");
        const long double power=exponent*logarithm,whole=x87::round(power);
        result=x87::scale(x87::pow2m1(power-whole)+1.0L,whole);
    }
    return binary64(result);
}
long double via_resistance(const ConductorSegment& segment,const ResistanceTechnology& technology) {
    const int count=static_cast<int>(static_cast<unsigned>(segment.width_count)*static_cast<unsigned>(segment.thickness_count));
    return (long double)technology.vias[segment.metal-technology.metal_count].resistance/count;
}
long double high_factor(long double parameter,long double aspect) {
    const double aspect64=binary64(aspect),first=resistance_power(aspect,0x1.30a3d70a3d70ap+0),
        cube=resistance_power(parameter,3.0L),last=resistance_power((long double)aspect64-1.0L,0x1.ccccccccccccdp+0);
    const long double skin=(0x1.b945b6c3760bfp-2L*parameter)/(0x1.4fdf3b645a1cbp-5L*first+1.0L),
        fraction=(0x1.496bb98c7e282p+0L*parameter+0x1.1d5cfaacd9e84p+0L)/
                 (0x1.4978d4fdf3b64p+0L*cube+0x1.3ac710cb295eap+0L);
    return (skin+fraction)+0x1.cac083126e979p-9L*last;
}
const ConductorSegment* next(const ConductorSegment* segment) {
    return reinterpret_cast<const ConductorSegment*>(segment->next_address);
}
}
long double segment_resistance(const ConductorSegment& s,double frequency,const ResistanceTechnology& technology) {
    if(s.metal>=technology.metal_count)return via_resistance(s,technology);
    const long double dx=(long double)s.endpoints[3]-s.endpoints[0],dy=(long double)s.endpoints[4]-s.endpoints[1],
        dz=(long double)s.endpoints[5]-s.endpoints[2],width=s.width,resistance=technology.metals[s.metal].resistance;
    const double xy=binary64(dx*dx+dy*dy);
    const long double dc=(x87::sqrt(dz*dz+xy)/width)*resistance;
    if(!(frequency>0))return dc;
    const long double parameter=x87::sqrt((((long double)frequency*frequency_factor)*width*unit_scale)/resistance);
    if(parameter>=2.5L)return high_factor(parameter,width/technology.metals[s.metal].thickness)*dc;
    const double power=resistance_power(parameter,3.0L+(0x1.47ae147ae147bp-7L*parameter)*parameter);
    return ((long double)power*0x1.8fc504816f007p-7L)*dc+dc;
}
long double shape_resistance_dc(const ShapeFilamentList& shape,const ResistanceTechnology& technology) {
    long double result=0;
    for(auto* s=shape.first;s;s=next(s)) {
        if(s->metal>=technology.metal_count)result+=via_resistance(*s,technology);
        else {
            const long double dx=(long double)s->endpoints[3]-s->endpoints[0],dy=(long double)s->endpoints[4]-s->endpoints[1],
                dz=(long double)s->endpoints[5]-s->endpoints[2];
            result+=(x87::sqrt((dz*dz+dy*dy)+dx*dx)/s->width)*technology.metals[s->metal].resistance;
            // The command makes and discards this extra scalar-helper call.
            volatile long double ignored=segment_resistance(*s,-1,technology);(void)ignored;
        }
    }
    return result;
}
long double shape_resistance_high_frequency(const ShapeFilamentList& shape,double frequency,const ResistanceTechnology& technology) {
    double result=0;
    for(auto* s=shape.first;s;s=next(s)) {
        long double contribution;
        if(s->metal>=technology.metal_count)contribution=via_resistance(*s,technology);
        else {
            const long double width=s->width,sheet=technology.metals[s->metal].resistance,
                parameter=x87::sqrt(((((long double)frequency*frequency_factor)*0x1.12e0be826d695p-30L)*width*unit_scale)/sheet),
                dx=(long double)s->endpoints[3]-s->endpoints[0],dy=(long double)s->endpoints[4]-s->endpoints[1],
                dz=(long double)s->endpoints[5]-s->endpoints[2],dc=(x87::sqrt((dz*dz+dy*dy)+dx*dx)/width)*sheet;
            long double factor;
            if(parameter>=2.5L)factor=high_factor(parameter,width/technology.metals[s->metal].thickness);
            else {
                const double power=resistance_power(parameter,3.0L+(0x1.47ae147ae147bp-7L*parameter)*parameter);
                factor=0x1.8fc504816f007p-7L*power+1.0L;
            }
            contribution=factor*dc;
        }
        result=binary64(contribution+(long double)result);
    }
    return result;
}
}
