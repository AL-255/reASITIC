#include "asitic/model.hpp"
#include "asitic/legacy_complex.hpp"
#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
namespace detail { [[noreturn]] void model_control_word_fault(); }
namespace {
using x87::binary64;
constexpr long double scale=0x1.a36e2eb1c432dp-14L,nano=0x1.12e0be826d695p-30L;
long long truncate64(long double x) {
    unsigned short saved,temporary;long long result;
    asm volatile("fnstcw %0" : "=m"(saved));temporary=(saved&255u)|0x0c00u;
    asm volatile("fldcw %2; fistpll %0; fldcw %3" : "=m"(result) : "t"(x),"m"(temporary),"m"(saved) : "st");
    return result;
}
double power(long double base,long double exponent) {
    const long long integer=truncate64(exponent);
    if(base==0 && exponent>0)return 0;
    long double result;
    if((long double)integer==exponent) {
        unsigned long long remaining=static_cast<unsigned long long>(integer);result=1;
        while(remaining) {
            if(remaining&1u)result=base*result;
            remaining>>=1;if(remaining)base=base*base;
        }
    } else {
        long double logarithm;asm("fyl2x" : "=t"(logarithm) : "0"(base),"u"(1.0L) : "st(1)");
        const long double p=exponent*logarithm,whole=x87::round(p);
        result=x87::scale(x87::pow2m1(p-whole)+1.0L,whole);
    }
    return binary64(result);
}
long double skin_factor(long double parameter,long double aspect) {
    if(!(parameter>=2.5L))return 0x1.8fc504816f007p-7L*power(parameter,3.0L+(0x1.47ae147ae147bp-7L*parameter)*parameter)+1.0L;
    const double first=power(aspect,0x1.30a3d70a3d70ap+0),cube=power(parameter,3.0L),last=power(aspect-1.0L,0x1.ccccccccccccdp+0);
    const long double skin=(0x1.b945b6c3760bfp-2L*parameter)/(0x1.4fdf3b645a1cbp-5L*first+1.0L),
        fraction=(0x1.496bb98c7e282p+0L*parameter+0x1.1d5cfaacd9e84p+0L)/
                 (0x1.4978d4fdf3b64p+0L*cube+0x1.3ac710cb295eap+0L);
    return (skin+fraction)+0x1.cac083126e979p-9L*last;
}
struct Block { ComplexValue a,b,c,d; };
struct ExtendedComplex { long double real,imag; };
ExtendedComplex multiply(const ComplexValue& a,const ComplexValue& b) {
    return {(long double)a.real*b.real-(long double)a.imag*b.imag,
            (long double)a.imag*b.real+(long double)a.real*b.imag};
}
ComplexValue product_sum(const ComplexValue& a,const ComplexValue& b,const ComplexValue& c,const ComplexValue& d) {
    const auto first=multiply(a,b),second=multiply(c,d);
    return {binary64(first.real+second.real),binary64(first.imag+second.imag)};
}
const ConductorSegment* next(const ConductorSegment* s) { return reinterpret_cast<const ConductorSegment*>(s->next_address); }
}
bool solve_analytical_model(const ShapeFilamentList& shape,double frequency,const AnalyticalModelOptions& o,
    AnalyticalModelHistory& history,AnalyticalModelResult& result,WarningSink warnings) {
    Block current{{1,0},{0,0},{0,0},{1,0}};double coupling=0;
    const CapacitanceTechnology technology{o.metals,o.substrate};int index=1;
    for(auto* s=shape.first;s;s=next(s),index=static_cast<int>(static_cast<unsigned>(index)+1)) {
        Block block;
        if(s->metal<o.metal_count) {
            const double width=binary64(s->width*scale),thickness=binary64(o.metals[s->metal].thickness*scale);
            const long double dx=(long double)s->endpoints[3]-s->endpoints[0],dy=(long double)s->endpoints[4]-s->endpoints[1],
                dz=(long double)s->endpoints[5]-s->endpoints[2];
            const double xy=binary64(dx*dx+dy*dy);
            const long double extended_length=x87::sqrt(dz*dz+xy)*scale;
            const double length=binary64(extended_length);
            if(extended_length>0x1.b7cdfd9d7bdbbp-34L) {
                const long double twice=extended_length+extended_length,sum=(long double)width+thickness;
                history.inductance=binary64((x87::log(twice/sum)+0x1.004039abf3387p-1L+sum/(3.0L*length))*twice);
            }
            const long double sheet=o.metals[s->metal].resistance,
                parameter=x87::sqrt(((((long double)frequency*0x1.921fb544418ecp+4L)*nano)*width)/sheet);
            history.resistance=binary64(((long double)length/width)*sheet);
            history.resistance=binary64(skin_factor(parameter,(long double)width/thickness)*history.resistance);
            for(auto* other=shape.first;other;other=next(other))if(other!=s && other->metal<o.metal_count) {
                double a[8],b[8],mutual;
                __builtin_memcpy(a,s,sizeof(a));__builtin_memcpy(b,other,sizeof(b));
                const char* warning=nullptr;
                const int status=segment_mutual(a,b,&mutual,&warning);
                if(warning)warnings.emit(warning);
                if(status==-1)warnings.emit("Error:  Encountered Errors While Calculating Mutual Inductance");
                history.inductance=binary64((long double)history.inductance+mutual);
            }
            const double height=binary64(o.metal_heights[s->metal]*scale);
            const long double gap=o.kind==2?o.special_gap:(long double)o.spacing*scale;
            int substrate;double eps;
            __builtin_memcpy(&substrate,o.metals[s->metal].prefix+0xa0,4);
            __builtin_memcpy(&eps,o.substrate[substrate].prefix+4,8);
            const long double side_cap=((0x1.8ebfab864ade4p-44L*eps)*thickness)/gap;
            double pair_self,pair_mutual,edge_self,edge_mutual;
            coupled_microstrip_matrix(1,width,binary64(gap),height,&pair_self,&pair_mutual,s->metal,technology);
            coupled_microstrip_matrix(0,width,binary64(gap),height,&edge_self,&edge_mutual,s->metal,technology);
            const long double total=(long double)o.turns*o.sides;
            const bool edge=index>o.sides && !(total-o.sides<=index);
            if(total>=static_cast<int>(static_cast<unsigned>(o.sides)+static_cast<unsigned>(index)))
                coupling=binary64((long double)length*((edge?edge_mutual:pair_mutual)+side_cap)+coupling);
            // The original's second depth-square expansion loads the zero high
            // word of integer 2 into ESI, then attempts FLDCW [ESI-4]. It never
            // reaches the conductor block construction on this Linux target.
            // Preserve its temporary control word and the actual memory fault.
            volatile double ignored=power((long double)o.substrate[0].height-o.substrate[1].height,2.0L);
            (void)ignored;
            detail::model_control_word_fault();
        } else {
            const auto& via=o.vias[s->metal-o.metal_count];
            const int count=static_cast<int>(static_cast<unsigned>(s->width_count)*static_cast<unsigned>(s->thickness_count));
            history.resistance=binary64((long double)via.resistance/count);
            const long double length=x87::abs((long double)s->endpoints[5]-s->endpoints[2])*scale;
            const double width=binary64((scale*s->width_count)*via.perimeter_step);
            double step;__builtin_memcpy(&step,via.gap,8);
            const long double thickness=(scale*s->thickness_count)*step;
            if(length>0x1.b7cdfd9d7bdbbp-34L) {
                const long double sum=(long double)width+thickness,twice=length+length;
                const double twice64=binary64(twice);
                // Native grouping matters when the x87 precision control is 53 bits.
                const long double term=(x87::log(twice/sum)+0x1.004039abf3387p-1L)+sum/(3.0L*length);
                history.inductance=binary64(term*(((long double)history.angular_frequency*nano)*twice64));
            } else history.inductance=0;
            history.resistance=-history.resistance;
            block={{1,0},{history.resistance,-history.inductance},{0,0},{1,0}};
        }
        current={product_sum(current.a,block.a,current.b,block.c),product_sum(current.a,block.b,current.b,block.d),
                 product_sum(current.c,block.a,current.d,block.c),product_sum(current.c,block.b,current.d,block.d)};
    }
    const auto inverse=legacy::divide(1.,current.b);
    const ComplexValue a_minus{binary64((long double)current.a.real-1.0L),current.a.imag},
        d_minus{binary64((long double)current.d.real-1.0L),current.d.imag};
    const auto a=multiply(a_minus,inverse),d=multiply(d_minus,inverse);
    const ComplexValue a64{binary64(a.real),binary64(a.imag)},d64{binary64(d.real),binary64(d.imag)};
    const long double real=(long double)inverse.real+0.0L,imag=(long double)inverse.imag+(long double)coupling*history.angular_frequency;
    result.admittance={{binary64((long double)d64.real+real),binary64((long double)d64.imag+imag)},
                       {binary64(-real),binary64(-imag)},{binary64(-real),binary64(-imag)},
                       {binary64((long double)a64.real+real),binary64((long double)a64.imag+imag)}};
    result.pi=extract_pi_equivalent(result.admittance,frequency);
    return true;
}
}
