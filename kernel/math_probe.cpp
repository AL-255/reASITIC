#include "asitic/quadrature.hpp"
// Test boundary for comparing software results with actual x87 instructions.
#ifdef __EMSCRIPTEN__
#include "x87_port.hpp"
using Real=SoftReal;
namespace x87=asitic::x87;
#else
#include "x87.hpp"
using Real=long double;
namespace x87=asitic::x87;
#endif
extern "C" int sprintf(char*,const char*,...);
extern "C" const char* probe_math(int operation,double a,double b,double c) {
    static char output[512];Real input=a;
    if(operation>=16) {
        // The exact angle sequence used by the original ring constructor.
        volatile double stored_gap=x87::binary64(Real(a)*Real(0x1.1df46a2529d39p-6));
        const double gap=stored_gap;
        input=((Real(0x1.921fb54442d18p+2L)-gap)/(Real(c)-1))*b+Real(gap)*Real(.5);
        operation-=16;
    } else {
        if(c==1)input=Real(a)/b;
        if(c==2)input=Real(a)+b;
        if(c==3)input=Real(a)*b;
    }
    Real value;
    if(operation>=10 && operation<=15) {
        Real logarithm;const Real delta=input-Real(1);
#ifdef __EMSCRIPTEN__
        logarithm=x87::abs(delta)<=Real(0x1.28f5c28f5c28fp-2L)?x87::log2p1(delta):x87::log2(input);
#else
        if(x87::abs(delta)<=Real(0x1.28f5c28f5c28fp-2L))asm("fld1; fxch; fyl2xp1":"=t"(logarithm):"0"(delta):"st(1)");
        else asm("fld1; fxch; fyl2x":"=t"(logarithm):"0"(input):"st(1)");
#endif
        const Real exponent=logarithm*Real(1.5),integer=x87::round(exponent),fraction=exponent-integer,part=x87::pow2m1(fraction);
        value=operation==10?logarithm:operation==11?exponent:operation==12?fraction:operation==13?part:operation==14?part+1:x87::scale(part+1,integer);
    } else switch(operation) {
        case 0:value=x87::sqrt(input);break;
        case 1:value=x87::log(input);break;
        case 2:value=x87::log1p(input);break;
        case 3:value=x87::atan2(input,b);break;
        case 4:value=x87::sin(input);break;
        case 5:value=x87::cos(input);break;
        case 6:value=x87::exp(input);break;
        case 7:value=x87::pow2m1(input);break;
        case 8:value=input;break;
        case 9:value=asitic::quadpack::error_power(x87::binary64(input));break;
        default:return "{\"error\":\"Unknown math probe\"}";
    }
    unsigned words[3]{};
#ifdef __EMSCRIPTEN__
    words[0]=static_cast<unsigned>(value.value.signif);words[1]=static_cast<unsigned>(value.value.signif>>32);words[2]=value.value.signExp;
#else
    volatile long double stored=value;const auto* bytes=reinterpret_cast<const volatile unsigned char*>(&stored);
    for(unsigned i=0;i<10;++i)words[i/4]|=static_cast<unsigned>(bytes[i])<<((i%4)*8);
#endif
    const double rounded=x87::binary64(value);
    if(!__builtin_isfinite(rounded))return "{\"error\":\"Nonfinite math probe output\"}";
    sprintf(output,"{\"value\":%.17g,\"low\":%u,\"high\":%u,\"exponent\":%u}",rounded,words[0],words[1],words[2]);return output;
}

// Diagnostic boundary for a retained binary80 operand. The application never
// substitutes captured outputs: these words only reproduce instruction inputs.
extern "C" const char* probe_retained_math(int operation,unsigned low,unsigned high,int exponent) {
    static char output[512];
    if(exponent<0 || exponent>65535 || (exponent&0x7fff)==0x7fff || ((exponent&0x7fff) && !(high&0x80000000u)))return "{\"error\":\"Invalid finite binary80 operand\"}";
#ifdef __EMSCRIPTEN__
    Real input(extFloat80_t{static_cast<unsigned long long>(high)<<32|low,static_cast<unsigned short>(exponent)});
#else
    unsigned words[3]={low,high,static_cast<unsigned>(exponent)};Real input;__builtin_memcpy(&input,words,sizeof(input));
#endif
    Real value;
    switch(operation){case 0:value=x87::sqrt(input);break;case 1:value=x87::log(input);break;case 2:value=x87::log1p(input);break;case 6:value=x87::exp(input);break;case 8:value=input;break;default:return "{\"error\":\"Unknown retained math probe\"}";}
    unsigned words_out[3]{};
#ifdef __EMSCRIPTEN__
    words_out[0]=value.value.signif;words_out[1]=value.value.signif>>32;words_out[2]=value.value.signExp;
#else
    volatile long double stored=value;const auto* bytes=reinterpret_cast<const volatile unsigned char*>(&stored);
    for(unsigned i=0;i<10;++i)words_out[i/4]|=static_cast<unsigned>(bytes[i])<<((i%4)*8);
#endif
    const double rounded=x87::binary64(value);if(!__builtin_isfinite(rounded))return "{\"error\":\"Nonfinite retained math output\"}";
    sprintf(output,"{\"value\":%.17g,\"low\":%u,\"high\":%u,\"exponent\":%u}",rounded,words_out[0],words_out[1],words_out[2]);return output;
}

// Analytic full-domain contact: all nonzero cosine modes integrate to zero.
// Explicit constant/first-mode samples isolate index normalization from FFT cost.
#include "potential_grid.hpp"
#include "numerical_policy.hpp"
#include "asitic/potential.hpp"
extern "C" void* calloc(unsigned,unsigned);
extern "C" void free(void*);
extern "C" const char* probe_potential_contact(int nx,int ny){
    static char output[256];
    if(reasitic::reference_numerics()||!reasitic::potential_grid_fits(nx,ny,1))return "{\"error\":\"Invalid potential probe dimensions\"}";
    using namespace asitic;
    auto* xy=static_cast<ComplexValue*>(calloc(static_cast<unsigned>((nx+1)*(ny+1)),16));
    auto* x=static_cast<ComplexValue*>(calloc(static_cast<unsigned>(2*nx),16));
    auto* y=static_cast<ComplexValue*>(calloc(static_cast<unsigned>(2*ny),16));
    if(!xy||!x||!y){free(xy);free(x);free(y);return "{\"error\":\"Potential probe allocation failed\"}";}
    MetalLayer metal{};int layer=1;double height=32;__builtin_memcpy(metal.prefix+0xa0,&layer,4);__builtin_memcpy(metal.prefix+0xa8,&height,8);
    ComplexValue coefficients[]={{1,0},{2,-1}};double depths[]={750,50,0};
    PotentialContact a{};a.x1=nx;a.y1=ny;
    const PotentialGeometry geometry{nx,ny,512,512,&metal,{coefficients,depths}};
    const PotentialTables tables{x,y,xy};const auto z=contact_potential(a,a,geometry,tables);
    ComplexValue half{};const bool has_half=nx>1||ny>1;
    if(has_half){
        // Required samples of the first cosine mode T(j)=2*(3-2i)*cos(pi*j/n).
        // The midpoint is exactly zero; the remaining accessed values are +/-2G.
        if(nx>=ny){x[0]={6,-4};x[nx]={-6,4};a.x1=nx/2;}
        else{y[0]={6,-4};y[ny]={-6,4};a.y1=ny/2;}
        half=contact_potential(a,a,geometry,tables);
    }
    free(xy);free(x);free(y);
    if(!__builtin_isfinite(z.real)||!__builtin_isfinite(z.imag)||(has_half&&(!__builtin_isfinite(half.real)||!__builtin_isfinite(half.imag))))return "{\"error\":\"Nonfinite analytic contact potential\"}";
    char* out=output;out+=sprintf(out,"{\"potential\":[%.17g,%.17g],\"half_domain_potential\":",z.real,z.imag);
    if(has_half)sprintf(out,"[%.17g,%.17g]}",half.real,half.imag);else sprintf(out,"null}");return output;
}
