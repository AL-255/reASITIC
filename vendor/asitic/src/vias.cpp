#include "asitic/impedance.hpp"
#include "x87.hpp"

namespace asitic {
bool add_via_impedance(ComplexMatrixView out,const InductanceContext& ctx,
                       const ViaLayer* vias,int metal_count,const ConductorSegment** scratch,
                       WarningSink warnings) {
    if(!scratch) { warnings.emit("Error:  Out of memory.");return false; }
    int count=0;
    for(const ConductorSegment* p=ctx.shape->first;p;
        p=reinterpret_cast<const ConductorSegment*>(p->next_address))
        if(p->metal>=metal_count) scratch[count++]=p;
    using x87::binary64;
    constexpr long double tau=0x1.921fb54442d18p+2L;
    constexpr long double eps=0x1.b7cdfd9d7bdbbp-34L;
    const double omega=binary64(tau*ctx.frequency);
    for(int i=0;i<ctx.via_count;++i) {
        const ConductorSegment& s=*scratch[i];
        const ViaLayer& via=vias[s.metal-metal_count];
        const long double perimeter=static_cast<long double>(via.perimeter_step)*s.thickness_count
                                    +static_cast<long double>(via.perimeter_step)*s.width_count;
        const long double height=static_cast<long double>(s.endpoints[5])-s.endpoints[2];
        if(!(height>eps)) continue;
        const long double resistance=static_cast<long double>(via.resistance)/(s.width_count*s.thickness_count);
        const long double inductance=(height*0x1.a36e2eb1c432dp-13L)
                                     *((x87::log((height+height)/perimeter)+0x1.004039abf3387p-1L)
                                       +perimeter/(height*3.0L));
        const double reactance=binary64((inductance*omega)*0x1.12e0be826d695p-30L);
        ComplexValue& z=out.at(ctx.via_indices[i],ctx.via_indices[i]);
        z.real=binary64(static_cast<long double>(z.real)+resistance);
        z.imag=binary64(static_cast<long double>(z.imag)+reactance);
    }
    return true;
}
}
