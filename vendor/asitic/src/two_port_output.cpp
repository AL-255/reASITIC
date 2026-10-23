#include "asitic/two_port_output.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
void store(double& destination,long double value) { asm volatile("fstpl %0":"=m"(destination):"t"(value):"st"); }
long double load(const volatile double& value) { long double out;asm volatile("fldl %1":"=t"(out):"m"(value));return out; }
// Same bundled division arithmetic, with direct stores to the return record.
// Re-loading a rounded subnormal through a double-return helper would add a
// denormal exception that the original stores do not raise.
ComplexValue divide(const ComplexValue& numerator,const ComplexValue& denominator) {
    const long double c=load(denominator.real),d=load(denominator.imag);
    unsigned short status;
    asm volatile("fldl %1; fabs; fldl %2; fabs; fcompp; fnstsw %0" : "=a"(status) : "m"(denominator.real),"m"(denominator.imag) : "st","st(1)");
    const long double real=load(numerator.real),imag=load(numerator.imag);ComplexValue out;
    if(!(status&0x500)) {
        const long double ratio=c/d,divisor=ratio*ratio*d+d;
        const long double first=(real*ratio+imag)/divisor,second=(imag*ratio-real)/divisor;
        store(out.real,first);store(out.imag,second);
    } else {
        const long double ratio=d/c,divisor=ratio*ratio*c+c;
        const long double first=(imag*ratio+real)/divisor,second=(imag-real*ratio)/divisor;
        store(out.real,first);store(out.imag,second);
    }
    return out;
}
}
TwoPortScattering convert_two_port_scattering(const TwoPortAdmittance& y) {
    const double reference=0x1.47ae147ae147bp-6,negative_two=-2.;
    TwoPortScattering result;
    // Retain the native extended arithmetic and its otherwise discarded
    // binary64 copies. The first diagonal uses y22 in both factors.
    const long double t0=load(y.y22.real);
    const long double t1=load(reference);
    const long double t2=t0+t1;
    [[maybe_unused]] double s40_3;store(s40_3,t2);
    const long double t4=load(y.y22.imag);
    [[maybe_unused]] double s38_5;store(s38_5,t4);
    const long double t6=load(y.y11.real);
    const long double t7=load(reference);
    const long double t8=t6+t7;
    const long double t9=load(y.y11.imag);
    const long double t10=t4*t9;
    const long double t11=t2*t8;
    const long double t12=t11-t10;
    const long double t13=t2*t9;
    const long double t14=t4*t8;
    const long double t15=t13+t14;
    [[maybe_unused]] double s48_16;store(s48_16,t9);
    [[maybe_unused]] double s50_17;store(s50_17,t8);
    [[maybe_unused]] double s30_18;store(s30_18,t12);
    [[maybe_unused]] double s28_19;store(s28_19,t15);
    const long double t20=load(y.y21.real);
    const long double t21=load(y.y12.real);
    const long double t22=t20*t21;
    const long double t23=load(y.y21.imag);
    const long double t24=load(y.y12.imag);
    const long double t25=t23*t24;
    const long double t26=t22-t25;
    const long double t27=load(y.y12.imag);
    const long double t28=t20*t27;
    const long double t29=load(y.y12.real);
    const long double t30=t23*t29;
    const long double t31=t28+t30;
    const long double t32=t12-t26;
    const long double t33=t15-t31;
    [[maybe_unused]] double s60_34;store(s60_34,t26);
    [[maybe_unused]] double s58_35;store(s58_35,t31);
    [[maybe_unused]] double s20_36;store(s20_36,t32);
    [[maybe_unused]] double s18_37;store(s18_37,t33);
    const ComplexValue denominator{s20_36,s18_37};
    const long double t38=load(reference);
    const long double t39=load(y.y22.real);
    const long double t40=t38-t39;
    const long double t41=-t4;
    const long double t42=t41*t4;
    const long double t43=t40*t2;
    const long double t44=t43-t42;
    [[maybe_unused]] double s28_45;store(s28_45,t41);
    const long double t46=t41*t2;
    [[maybe_unused]] double s20_47;store(s20_47,t2);
    [[maybe_unused]] double s18_48;store(s18_48,t4);
    [[maybe_unused]] double s30_49;store(s30_49,t40);
    const long double t50=t4*t40;
    const long double t51=t46+t50;
    [[maybe_unused]] double s40_52;store(s40_52,t44);
    const long double t53=t44+t26;
    [[maybe_unused]] double s38_54;store(s38_54,t51);
    const long double t55=t51+t31;
    [[maybe_unused]] double s70_56;store(s70_56,t26);
    [[maybe_unused]] double s68_57;store(s68_57,t31);
    [[maybe_unused]] double s50_58;store(s50_58,t53);
    [[maybe_unused]] double s48_59;store(s48_59,t55);
    result.s11=divide({s50_58,s48_59},denominator);
    const long double t60=load(y.y22.real);
    const long double t61=load(reference);
    const long double t62=t60+t61;
    const long double t63=load(y.y22.imag);
    [[maybe_unused]] double s38_64;store(s38_64,t63);
    const long double t65=load(reference);
    [[maybe_unused]] double s40_66;store(s40_66,t62);
    const long double t67=load(y.y11.real);
    const long double t68=t65-t67;
    const long double t69=load(y.y11.imag);
    const long double t70=-t69;
    const long double t71=t63*t70;
    const long double t72=t62*t68;
    const long double t73=t72-t71;
    const long double t74=t62*t70;
    const long double t75=t63*t68;
    const long double t76=t74+t75;
    [[maybe_unused]] double s28_77;store(s28_77,t70);
    [[maybe_unused]] double s30_78;store(s30_78,t68);
    [[maybe_unused]] double s50_79;store(s50_79,t73);
    [[maybe_unused]] double s48_80;store(s48_80,t76);
    const long double t81=load(y.y21.real);
    const long double t82=load(y.y12.real);
    const long double t83=t81*t82;
    const long double t84=load(y.y21.imag);
    const long double t85=load(y.y12.imag);
    const long double t86=t84*t85;
    const long double t87=t83-t86;
    const long double t88=load(y.y12.imag);
    const long double t89=t81*t88;
    const long double t90=t82*t84;
    const long double t91=t90+t89;
    const long double t92=t73+t87;
    const long double t93=t76+t91;
    [[maybe_unused]] double s20_94;store(s20_94,t87);
    [[maybe_unused]] double s18_95;store(s18_95,t91);
    [[maybe_unused]] double s60_96;store(s60_96,t92);
    [[maybe_unused]] double s58_97;store(s58_97,t93);
    result.s22=divide({s60_96,s58_97},denominator);
    const long double t98=load(negative_two);
    const long double t99=load(y.y21.real);
    const long double t100=t99*t98;
    const long double t101=load(y.y21.imag);
    const long double t102=t101*t98;
    [[maybe_unused]] double s50_103;store(s50_103,t100);
    const long double t104=load(reference);
    const long double t105=t100*t104;
    [[maybe_unused]] double s48_106;store(s48_106,t102);
    const long double t107=load(reference);
    const long double t108=t102*t107;
    [[maybe_unused]] double s60_109;store(s60_109,t105);
    [[maybe_unused]] double s58_110;store(s58_110,t108);
    const long double s80_111=t98;
    result.s21=divide({s60_109,s58_110},denominator);
    const long double t112=(s80_111);
    const long double t113=load(y.y12.real);
    const long double t114=t113*t112;
    const long double t115=load(y.y12.imag);
    const long double t116=t112*t115;
    [[maybe_unused]] double s50_117;store(s50_117,t114);
    const long double t118=load(reference);
    const long double t119=t114*t118;
    [[maybe_unused]] double s48_120;store(s48_120,t116);
    const long double t121=load(reference);
    const long double t122=t116*t121;
    [[maybe_unused]] double s60_123;store(s60_123,t119);
    [[maybe_unused]] double s58_124;store(s58_124,t122);
    result.s12=divide({s60_123,s58_124},denominator);
    return result;
}
namespace {
void raw(double& destination,const double& value) { __builtin_memcpy(&destination,&value,8); }
// The native i386 caller pushes saved double words directly. Passing a
// floating lvalue here would let GCC quiet a signaling NaN or raise denormal
// while assembling the argument list, changing the native observable state.
void emit(void* file,const char* format,const double* values,unsigned count,PortOutputPrint print) {
    unsigned words[20];__builtin_memcpy(words,values,count*8);
    if(count==8) print(file,format,words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7],words[8],words[9],words[10],words[11],words[12],words[13],words[14],words[15]);
    else if(count==9) print(file,format,words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7],words[8],words[9],words[10],words[11],words[12],words[13],words[14],words[15],words[16],words[17]);
    else print(file,format,words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7],words[8],words[9],words[10],words[11],words[12],words[13],words[14],words[15],words[16],words[17],words[18],words[19]);
}
struct Polar { double magnitude,phase; };
Polar output_polar(const ComplexValue& value) {
    const long double imag=load(value.imag),real=load(value.real);
    const long double angle=x87::atan2(imag,real);
    const long double square=real*real+imag*imag;
    double angle_spill;store(angle_spill,angle);Polar result;
    store(result.phase,load(angle_spill)*0x1.ca5dc1a63c1f8p+5L);
    store(result.magnitude,x87::sqrt(square));return result;
}
}
void write_two_port_record(void* file,double frequency,int parameter,unsigned char polar,TwoPortOutputState& state,PortOutputPrint print) {
    long double f;asm volatile("fldl %1":"=t"(f):"m"(frequency));
    constexpr const char* rectangular="\n%-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e";
    constexpr const char* magnitude="\n%-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15lg";
    double values[10];
    if(parameter==1 || parameter==2) {
        const ComplexValue* cells[4];const char* format=magnitude;
        if(parameter==1) {
            state.scattering=convert_two_port_scattering(state.admittance);const auto& s=state.scattering;
            cells[0]=&s.s11;cells[1]=&s.s21;cells[2]=&s.s12;cells[3]=&s.s22;if(!polar)format=rectangular;
        } else {
            const auto& y=state.admittance;cells[0]=&y.y22;cells[1]=cells[2]=&y.y21;cells[3]=&y.y11;
        }
        // The source stores all complex fields before assembling frequency.
        for(unsigned i=4;i-->0;) {
            if(polar) {
                const auto p=output_polar(*cells[i]);raw(values[2*i+1],p.magnitude);raw(values[2*i+2],p.phase);
            } else if(parameter==2 && (i==1 || i==2)) {
                store(values[2*i+1],load(cells[i]->real));store(values[2*i+2],load(cells[i]->imag));
            } else {
                raw(values[2*i+1],cells[i]->real);raw(values[2*i+2],cells[i]->imag);
            }
        }
        store(values[0],f*0x1.12e0be826d695p-30L);emit(file,format,values,9,print);
    } else if(parameter==4) {
        const auto* m=state.model;const double femto=0x1.203af9ee75616p-50,nano=0x1.12e0be826d695p-30;
        for(unsigned i=4;i<9;++i)raw(values[i+1],m[i]);
        store(values[4],load(m[3])/load(femto));store(values[3],load(m[2])/load(femto));raw(values[2],m[1]);store(values[1],load(m[0])/load(nano));store(values[0],f/1e9L);
        emit(file,"\n%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf",values,10,print);
    } else if(parameter==5) {
        const auto* m=state.model;const double nano=0x1.12e0be826d695p-30;
        raw(values[7],m[3]);raw(values[6],m[2]);store(values[5],load(m[8])/load(nano));raw(values[4],m[5]);store(values[3],load(m[1])/load(nano));raw(values[2],m[4]);store(values[1],load(m[0])/load(nano));store(values[0],f/1e9L);
        emit(file,"\n%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf%-11.2lf",values,8,print);
    }
}
}
