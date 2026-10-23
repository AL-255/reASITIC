#include "asitic/capacitance.hpp"
#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
void coupled_microstrip_capacitance(double width,double gap,double height,double permittivity,
    double* plate,double* fringe,double* inner,double* air,double* dielectric) {
    using x87::binary64;using x87::sqrt;
    constexpr long double epsilon0=0x1.8ebfab864ade4p-44L,pi=0x1.921fb54442d18p+1L;
    const long double ratio=(long double)width/height,eps=permittivity;
    const double first=binary64(x87::exp(0x1.2a3d70a3d70a4p+1L-ratio*0x1.43d70a3d70a3dp+1L)),
        attenuation=binary64(x87::exp((long double)first*-0x1.999999999999ap-4L));
    const long double root=sqrt(((long double)height/width)*12.0L+1.0L),
        effective=(eps+1.0L)*0.5L+(eps-1.0L)/(root+root);
    long double impedance;
    if(ratio<=1.0L)impedance=x87::log((long double)width/((long double)height*4.0L)+((long double)height*8.0L)/width)*(60.0L/sqrt(effective));
    else impedance=0x1.78fdb9effea46p+8L/(sqrt(effective)*
        (0x1.55810624dd2f2p-1L*x87::log(0x1.71a9fbe76c8b4p+0L+ratio)+binary64(0x1.649ba5e353f7dp+0L+ratio)));
    const long double parallel=(eps*epsilon0)*ratio;
    *plate=binary64(parallel);
    *fringe=binary64((sqrt(effective)/(impedance*0x1.bf08eb0000000p+34L)-parallel)*0.5L);
    const long double q=((long double)gap*8.0L)/height,exponent=-x87::abs(q+q)*x87::log2e(),whole=x87::round(exponent),
        small=x87::scale(x87::pow2m1(exponent-whole),whole),base=x87::scale(1.0L,whole),em1=(base-1.0L)+small;
    long double signed_tanh;
    const long double fraction=em1/(2.0L+em1),sign=-q>=0?1.0L:-1.0L;
    // FMUL preserves an indefinite NaN's sign; a compiler-generated FCHS does not.
    asm("fmulp" : "=t"(signed_tanh) : "0"(fraction),"u"(sign) : "st(1)");
    const double tanh=binary64(signed_tanh);
    *inner=binary64((long double)*fringe/((((long double)height/gap)*attenuation)*tanh+1.0L));
    const long double separation=(long double)gap/height,k=separation/(separation+((long double)width+width)/height),
        complement=sqrt(1.0L-k*k);
    long double elliptic;
    if(binary64(k*k)<=0.5) {
        const long double sum=(long double)binary64(sqrt(complement))+1.0L;
        elliptic=x87::log((sum+sum)/(1.0L-sqrt(complement)))*0x1.45f306dc9c883p-2L;
    } else {
        const long double root_k=sqrt(k),sum=root_k+1.0L;
        elliptic=pi/x87::log((sum+sum)/(1.0L-root_k));
    }
    *air=binary64(elliptic*0x1.8ebfab864ade4p-45L);
    const long double logarithm=x87::log(coth(binary64(((long double)gap*pi)/((long double)height*4.0L)))),
        inverse=1.0L/eps;
    const double inverse_square=binary64(inverse*inverse);
    *dielectric=binary64((((eps*epsilon0)/pi)*logarithm)+
        ((1.0L+(0x1.47ae147ae147bp-6L*sqrt(eps))/separation)-inverse_square)*(0x1.4cccccccccccdp-1L*(*fringe)));
}
void coupled_microstrip_matrix(int mode,double width,double gap,double height,double* self,double* mutual,
    int metal,const CapacitanceTechnology& technology) {
    int substrate;double eps;
    __builtin_memcpy(&substrate,technology.metals[metal].prefix+0xa0,4);
    __builtin_memcpy(&eps,technology.substrate[substrate].prefix+4,8);
    double plate,fringe,inner,air,dielectric;
    coupled_microstrip_capacitance(width,gap,height,eps,&plate,&fringe,&inner,&air,&dielectric);
    long double even,odd;
    if(mode==1) {
        const long double common=(long double)fringe+plate;
        even=common+inner;odd=(common+air)+dielectric;
    } else {
        even=((long double)inner+inner)+plate;
        const long double edges=(long double)air+dielectric;
        odd=(edges+edges)+plate;
    }
    const long double difference=odd-even;
    *self=x87::binary64(even);*mutual=x87::binary64(difference*0.5L);
}
void shape_microstrip_capacitance(const ShapeFilamentList& shape,const MicrostripShapeOptions& options,
    const CapacitanceTechnology& technology,double* first,double* second,double* mutual) {
    using x87::binary64;
    constexpr long double scale=0x1.a36e2eb1c432dp-14L;
    const auto next=[](const ConductorSegment* s) { return reinterpret_cast<const ConductorSegment*>(s->next_address); };
    const auto length=[](const ConductorSegment& s) {
        const long double dx=(long double)s.endpoints[3]-s.endpoints[0],dy=(long double)s.endpoints[4]-s.endpoints[1],
            dz=(long double)s.endpoints[5]-s.endpoints[2];
        return x87::sqrt((dx*dx+dy*dy)+dz*dz)*scale;
    };
    double last_length=0;
    for(auto* s=shape.first;s;s=next(s))last_length=binary64(length(*s));
    double a=0,b=0,cumulative=0;long double coupling=0;int index=1;
    for(auto* s=shape.first;s;s=next(s),index=static_cast<int>(static_cast<unsigned>(index)+1)) {
        const double height=binary64(options.metal_heights[s->metal]*scale),gap=binary64(options.spacing*scale),
            width=binary64(s->width*scale);
        double pair_self,pair_mutual,edge_self,edge_mutual;
        coupled_microstrip_matrix(1,width,gap,height,&pair_self,&pair_mutual,s->metal,technology);
        coupled_microstrip_matrix(0,width,gap,height,&edge_self,&edge_mutual,s->metal,technology);
        const long double segment_length=length(*s),total=(long double)options.turns*options.sides;
        cumulative=binary64((long double)cumulative+segment_length);
        const bool edge=index>options.sides && !(total-options.sides<=index);
        if(total>=static_cast<int>(static_cast<unsigned>(options.sides)+static_cast<unsigned>(index)))
            coupling+=segment_length*(edge?edge_mutual:pair_mutual);
        const long double contribution=segment_length*(edge?edge_self:pair_self);
        if((long double)last_length*0.5L>=cumulative)a=binary64(contribution+(long double)a);
        else b=binary64(contribution+(long double)b);
    }
    *first=a;*second=b;*mutual=binary64(coupling);
}
}
