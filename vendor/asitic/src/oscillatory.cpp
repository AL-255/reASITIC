#include "asitic/quadrature.hpp"
#include "x87.hpp"

namespace asitic::quadpack {
namespace {
using x87::binary64;
}
int solve_tridiagonal(int n,double* c,double* d,double* e,double* rhs) {
    c[0]=d[0];
    if(n>1) {
        d[0]=e[0];e[0]=0;e[n-1]=0;
        for(int k=1;k<n;++k) {
            // An unordered comparison also takes the original swap branch.
            if(!(x87::abs(c[k-1])>x87::abs(c[k]))) {
                double temp=c[k];c[k]=c[k-1];c[k-1]=temp;
                temp=d[k];d[k]=d[k-1];d[k-1]=temp;
                temp=e[k];e[k]=e[k-1];e[k-1]=temp;
                temp=rhs[k];rhs[k]=rhs[k-1];rhs[k-1]=temp;
            }
            if(c[k-1]==0)return k;
            const long double multiplier=-((long double)c[k]/c[k-1]);
            c[k]=binary64(multiplier*d[k-1]+d[k]);
            d[k]=binary64(multiplier*e[k-1]+e[k]);e[k]=0;
            rhs[k]=binary64(multiplier*rhs[k-1]+rhs[k]);
        }
    }
    if(c[n-1]==0)return n;
    rhs[n-1]=binary64((long double)rhs[n-1]/c[n-1]);
    if(n>1)rhs[n-2]=binary64(((long double)rhs[n-2]-(long double)d[n-2]*rhs[n-1])/c[n-2]);
    for(int k=n-3;k>=0;--k)
        rhs[k]=binary64((((long double)rhs[k]-(long double)d[k]*rhs[k+1])-(long double)e[k]*rhs[k+2])/c[k]);
    return 0;
}
RuleResult weighted_rule(Integrand f,Weight weight,const double* p1,const double* p2,
                          const double* p3,const double* p4,const int* selector,double a,double b) {
    constexpr double wg[]={0,0x1.092f69f826d57p-3,0x1.1e6b1713d8645p-2,0x1.86fe74ee3302ap-2};
    constexpr double wk[]={0x1.abfd7e03c2fa6p-2,0x1.77c5b67d5746fp-6,0x1.026cdaa7b61c3p-4,
        0x1.ad384a34814c7p-4,0x1.200ed0f46e8c0p-3,0x1.5a1f266e47d5cp-3,0x1.85d6861c80eb0p-3,0x1.a2adbcbec9cd9p-3};
    constexpr double xk[]={0x1.ad04f9087090ep-3,0x1.fba009d4d09b1p-1,0x1.e5f178e7c6229p-1,
        0x1.bacf827b9bb3ep-1,0x1.7ba9f9be3a1d5p-1,0x1.2c13a049dfa24p-1,0x1.9f95df119fd62p-2,0x1.a98b2892e0c78p-3};
    const double center=binary64(((long double)a+b)*0.5L),half=binary64(((long double)b-a)*0.5L),
        absolute_half=binary64(x87::abs(((long double)b-a)*0.5L));
    const double central_f=binary64(f(&center));
    const long double central=weight(&center,p1,p2,p3,p4,selector)*central_f;
    const double central64=binary64(central);
    double g=binary64(central*wk[0]),k=binary64((long double)central64*xk[0]);
    RuleResult out{};out.absolute=binary64(x87::abs((long double)central64*xk[0]));
    double left[7],right[7];
    for(int phase=0;phase<2;++phase)for(int i=1;i<=(phase==0?3:4);++i) {
        const int j=2*i-(phase==0?0:1);
        const long double offset=(long double)half*xk[j];
        const double x=binary64((long double)center-offset),y=binary64(offset+center);
        const double f1=binary64(f(&x));
        const double weighted1=binary64(weight(&x,p1,p2,p3,p4,selector)*f1);
        const double f2=binary64(f(&y));
        const long double weighted2=(long double)f2*weight(&y,p1,p2,p3,p4,selector);
        left[j-1]=weighted1;right[j-1]=binary64(weighted2);
        const long double sum=(long double)weighted1+weighted2;
        if(phase==0)g=binary64(sum*wg[i]+g);
        k=binary64(sum*wk[j]+k);
        out.absolute=binary64((x87::abs(weighted2)+x87::abs(weighted1))*wk[j]+out.absolute);
    }
    const long double mean=(long double)k*0.5L;
    out.ascendant=binary64(x87::abs((long double)central64-mean)*xk[0]);
    for(int j=1;j<=7;++j)
        out.ascendant=binary64((x87::abs((long double)left[j-1]-mean)+x87::abs((long double)right[j-1]-mean))
            *wk[j]+out.ascendant);
    out.integral=binary64((long double)k*half);out.absolute=binary64((long double)absolute_half*out.absolute);
    const long double asc=(long double)absolute_half*out.ascendant;
    out.ascendant=binary64(asc);
    const long double raw=x87::abs(((long double)k-g)*half);out.error=binary64(raw);
    if(asc!=0 && raw!=0) {
        const double power=error_power(binary64(raw*200.0L/out.ascendant));
        out.error=binary64((power<=1.0?(long double)power:1.0L)*out.ascendant);
    }
    if(0x1p-1022L/(50.0L*0x1p-52L)<out.absolute) {
        const long double floor=(50.0L*0x1p-52L)*out.absolute;
        if(floor>=out.error)out.error=binary64(floor);
    }
    return out;
}
void chebyshev_transform(const double* c,double* f,double* y,double* z) {
    double v[12];
    for(int i=0;i<12;++i) {
        v[i]=binary64((long double)f[i]-f[24-i]);f[i]=binary64((long double)f[i]+f[24-i]);
    }
    const long double base=(long double)v[0]-v[8];
    const double t=binary64((((long double)v[2]-v[6])-v[10])*c[5]);
    y[3]=binary64(base+t);y[9]=binary64(base-t);
    const double u=binary64(((long double)v[1]-v[7])-v[9]),w=binary64(((long double)v[3]-v[5])-v[11]);
    auto rounded_pair=[&](int a,int b,int center,long double r) {
        z[a]=binary64(r+y[center]);z[b]=binary64((long double)y[center]-binary64(r));
    };
    auto extended_pair=[&](int a,int b,int center,long double r) {
        z[a]=binary64(r+y[center]);z[b]=binary64((long double)y[center]-r);
    };
    rounded_pair(3,21,3,(long double)u*c[2]+(long double)w*c[8]);
    rounded_pair(9,15,9,(long double)u*c[8]-(long double)w*c[2]);
    const double p=binary64((long double)c[3]*v[4]),q=binary64((long double)c[7]*v[8]),
        r=binary64((long double)c[5]*v[6]),
        a=binary64(((long double)p+v[0])+q),
        b=binary64((long double)c[9]*v[10]+binary64((long double)c[1]*v[2]+r));
    y[1]=binary64((long double)a+b);y[11]=binary64((long double)a-b);
    double s=binary64((long double)c[0]*v[1]+(long double)c[2]*v[3]);
    s=binary64((long double)c[4]*v[5]+s);s=binary64((long double)c[6]*v[7]+s);
    s=binary64((long double)c[8]*v[9]+s);
    rounded_pair(1,23,1,(long double)c[10]*v[11]+s);
    s=binary64((long double)c[10]*v[1]-(long double)c[8]*v[3]);
    s=binary64((long double)c[6]*v[5]+s);s=binary64((long double)s-(long double)c[4]*v[7]);
    s=binary64((long double)c[2]*v[9]+s);
    rounded_pair(11,13,11,(long double)s-(long double)c[0]*v[11]);
    const double a2=binary64(((long double)v[0]-p)+q),
        b2=binary64((long double)c[9]*v[2]-r),b3=binary64((long double)c[1]*v[10]);
    const long double b4=(long double)b2+b3;
    y[5]=binary64((long double)a2+b4);y[7]=binary64((long double)a2-b4);
    s=binary64((long double)c[4]*v[1]-(long double)c[8]*v[3]);
    s=binary64((long double)s-(long double)c[0]*v[5]);s=binary64((long double)s-(long double)c[10]*v[7]);
    s=binary64((long double)c[2]*v[9]+s);
    rounded_pair(5,19,5,(long double)c[6]*v[11]+s);
    extended_pair(7,17,7,(((((long double)c[6]*v[1]-(long double)c[2]*v[3])-(long double)c[10]*v[5])
                          +(long double)c[0]*v[7])-(long double)c[8]*v[9])-(long double)c[4]*v[11]);
    for(int i=0;i<6;++i) {
        v[i]=binary64((long double)f[i]-f[12-i]);f[i]=binary64((long double)f[i]+f[12-i]);
    }
    const long double a3=(long double)c[7]*v[4]+v[0],b5=(long double)c[3]*v[2];
    y[2]=binary64(a3+b5);y[10]=binary64((long double)binary64(a3)-b5);y[6]=binary64((long double)v[0]-v[4]);
    s=binary64((long double)c[1]*v[1]+(long double)c[5]*v[3]);
    rounded_pair(2,22,2,(long double)c[9]*v[5]+s);
    rounded_pair(6,18,6,(((long double)v[1]-v[3])-v[5])*c[5]);
    extended_pair(10,14,10,((long double)c[9]*v[1]-(long double)c[5]*v[3])+(long double)c[1]*v[5]);
    for(int i=0;i<3;++i) {
        v[i]=binary64((long double)f[i]-f[6-i]);f[i]=binary64((long double)f[i]+f[6-i]);
    }
    y[4]=binary64((long double)c[7]*v[2]+v[0]);y[8]=binary64((long double)f[0]-(long double)c[7]*f[2]);
    extended_pair(4,20,4,(long double)c[3]*v[1]);
    extended_pair(8,16,8,(long double)c[7]*f[1]-f[3]);
    y[0]=binary64((long double)f[0]+f[2]);extended_pair(0,24,0,(long double)f[1]+f[3]);
    y[12]=z[12]=binary64((long double)v[0]-v[2]);
    for(int i=1;i<=11;++i)y[i]=binary64((long double)y[i]*0x1.5555555555555p-3);
    y[0]=binary64((long double)y[0]*0x1.5555555555555p-4);y[12]=binary64((long double)y[12]*0x1.5555555555555p-4);
    for(int i=1;i<=23;++i)z[i]=binary64((long double)z[i]*0x1.5555555555555p-4);
    z[0]=binary64((long double)z[0]*0x1.5555555555555p-5);z[24]=binary64((long double)z[24]*0x1.5555555555555p-5);
}
namespace {
constexpr double cheb_cosines[]={0x1.fb9ea92ec689bp-1,0x1.ee8dd4748bf15p-1,0x1.d906bcf328d46p-1,
    0x1.bb67ae8584caap-1,0x1.963268b572492p-1,0x1.6a09e667f3bcdp-1,0x1.37af93f9513eap-1,
    0x1.0000000000000p-1,0x1.87de2a6aea963p-2,0x1.0907dc1930690p-2,0x1.0b5150f6da2d1p-3};
long double oscillatory_weight(const double* x,const double* frequency,const double* opaque,
                               const double*,const double*,const int* selector) {
    const auto& math=*reinterpret_cast<const Trigonometry*>(opaque);
    const double argument=binary64((long double)*frequency**x);
    return *selector==2?math.sine(argument):math.cosine(argument);
}
void compute_moments(double par,int row,MomentCache& cache,Trigonometry math) {
    const double squared=binary64((long double)par*par),plus=binary64((long double)par*par+2.0L),
        sine=binary64(math.sine(par)),cosine=binary64(math.cosine(par));
    double v[28],diagonal[25],lower[25],upper[25];
    v[0]=binary64(((long double)sine+sine)/par);
    const double eight_cos=binary64((long double)cosine*8.0L);
    v[1]=binary64(((((long double)squared+squared)-8.0L)*sine/par+eight_cos)/squared);
    const long double polynomial=((long double)squared-80.0L)*squared+192.0L;
    v[2]=binary64(((polynomial+polynomial)*sine/par+((long double)squared-12.0L)*32.0L*cosine)/
                   ((long double)squared*squared));
    const long double ac=(long double)par*24.0L*sine;
    if(x87::abs(par)>24.0L) {
        for(int k=3;k<13;++k) {
            const long double n=2*k-2,sq=n*n,difference=((long double)plus-sq)-sq;
            v[k]=binary64(((((difference+difference)*v[k-1]-eight_cos)*(sq-4.0L)+ac)
                -(n+1.0L)*squared*(n+2.0L)*v[k-2])/((n-2.0L)*(n-1.0L)*squared));
        }
    } else {
        for(int i=0;i<25;++i) {
            const long double n=6+2*i,sq=n*n,difference=sq-4.0L;
            diagonal[i]=binary64(-((difference+difference)*(((long double)plus-sq)-sq)));
            v[i+3]=binary64(ac-difference*eight_cos);
            if(i<24) {
                upper[i]=binary64((n-2.0L)*(n-1.0L)*squared);
                lower[i+1]=binary64((n+4.0L)*(n+3.0L)*squared);
            }
        }
        v[3]=binary64((long double)v[3]-(56.0L*squared)*v[2]);
        const long double n=54,sq=n*n,b=(long double)par*sine;
        const long double asymptote=((((b*15.0L+((((long double)squared*210.0L-1.0L)*cosine-
            ((long double)squared*105.0L-63.0L)*b)/sq-(1.0L-(long double)squared*15.0L)*cosine))/sq
            -cosine)+b*3.0L)/sq-cosine)/sq;
        v[27]=binary64((long double)v[27]-(n-2.0L)*(n-1.0L)*(asymptote+asymptote)*squared);
        solve_tridiagonal(25,lower,diagonal,upper,v+3);
    }
    for(int i=0;i<13;++i)cache.data[(2*i)*cache.rows+row]=v[i];
    v[0]=binary64((((long double)sine-(long double)par*cosine)*2.0L)/squared);
    const long double div48=48.0L/squared;
    v[1]=binary64(((18.0L-div48)*sine)/squared+((div48-2.0L)*cosine)/par);
    const double ac_odd=binary64(-((long double)par*24.0L*cosine));
    const long double eight_sine=-((long double)sine*8.0L);
    if(x87::abs(par)>24.0L) {
        for(int k=2;k<12;++k) {
            const long double n=2*k-1,sq=n*n,difference=((long double)plus-sq)-sq;
            v[k]=binary64(((((difference+difference)*v[k-1]+eight_sine)*(sq-4.0L)+ac_odd)
                -(n+1.0L)*squared*(n+2.0L)*v[k-2])/((n-2.0L)*(n-1.0L)*squared));
        }
    } else {
        for(int i=0;i<25;++i) {
            const long double n=5+2*i,sq=n*n,difference=sq-4.0L;
            diagonal[i]=binary64(-((((long double)plus-sq)-sq)*(difference+difference)));
            v[i+2]=binary64(difference*eight_sine+ac_odd);
            if(i<24) {
                upper[i]=binary64((n-2.0L)*(n-1.0L)*squared);
                lower[i+1]=binary64((n+4.0L)*(n+3.0L)*squared);
            }
        }
        v[2]=binary64((long double)v[2]-(42.0L*squared)*v[1]);
        const long double n=53,sq=n*n,b=(long double)par*cosine;
        const long double asymptote=((((((long double)squared*15.0L-1.0L)*sine+
            (((long double)squared*210.0L-1.0L)*sine+((long double)squared*105.0L-63.0L)*b)/sq)
            -b*15.0L)/sq-b*3.0L-sine)/sq-sine)/sq;
        v[26]=binary64((long double)v[26]-(n-2.0L)*(n-1.0L)*(asymptote+asymptote)*squared);
        solve_tridiagonal(25,lower,diagonal,upper,v+2);
    }
    for(int i=0;i<12;++i)cache.data[(2*i+1)*cache.rows+row]=v[i];
}
}
OscillatoryRuleResult oscillatory_rule(Integrand f,double a,double b,double frequency,int selector,
    int level,bool reuse,MomentCache& cache,Trigonometry math) {
    const double center=binary64(((long double)b+a)*0.5L),half=binary64(((long double)b-a)*0.5L);
    const long double extended_par=(((long double)b-a)*0.5L)*frequency;
    const double par=binary64(extended_par);
    if(!(x87::abs(extended_par)>2.0L)) {
        const auto value=weighted_rule(f,oscillatory_weight,&frequency,reinterpret_cast<const double*>(&math),
            nullptr,nullptr,&selector,a,b);
        return {value,15};
    }
    const double argument=binary64((long double)center*frequency),
        cc=binary64(math.cosine(argument)*half),ss=binary64(math.sine(argument)*half);
    int row=cache.computed;
    if(level>=cache.computed && !reuse)compute_moments(par,row,cache,math);
    if(level<cache.computed)row=level;
    if(cache.computed<cache.rows-1 && level>=cache.computed)++cache.computed;
    double samples[25],c12[13],c24[25];
    double x=binary64((long double)half+center);samples[0]=binary64(f(&x)*0.5L);
    samples[12]=binary64(f(&center));
    x=binary64((long double)center-half);samples[24]=binary64(f(&x)*0.5L);
    for(int i=0;i<11;++i) {
        x=binary64((long double)half*cheb_cosines[i]+center);samples[i+1]=binary64(f(&x));
        x=binary64((long double)center-(long double)half*cheb_cosines[i]);samples[23-i]=binary64(f(&x));
    }
    chebyshev_transform(cheb_cosines,samples,c12,c24);
    const auto moment=[&](int column) { return cache.data[column*cache.rows+row]; };
    long double even12=(long double)c12[12]*moment(12),odd12=0;
    for(int j=11;j>=1;j-=2) { even12+=(long double)c12[j-1]*moment(j-1);odd12+=(long double)c12[j]*moment(j); }
    long double even24=(long double)c24[24]*moment(24),odd24=0;
    RuleResult out{};out.absolute=binary64(x87::abs(c24[24]));
    for(int j=23;j>=1;j-=2) {
        even24+=(long double)c24[j-1]*moment(j-1);odd24+=(long double)c24[j]*moment(j);
        // The original overwrites RESABS in every iteration instead of adding.
        out.absolute=binary64(x87::abs(c24[j])+x87::abs(c24[j-1]));
    }
    out.absolute=binary64(x87::abs(half)*out.absolute);out.ascendant=0x1.fffffffffffffp1023;
    const long double even_error=x87::abs(even24-even12),odd_error=x87::abs(odd24-odd12);
    if(selector==2) {
        out.integral=binary64((long double)cc*odd24+(long double)ss*even24);
        out.error=binary64(x87::abs((long double)ss*even_error)+x87::abs((long double)cc*odd_error));
    } else {
        out.integral=binary64((long double)cc*even24-(long double)ss*odd24);
        out.error=binary64(x87::abs((long double)ss*odd_error)+x87::abs((long double)cc*even_error));
    }
    return {out,25};
}

IntegralResult integrate_oscillatory(Integrand f,double start,double end,double frequency,int selector,
    double absolute_tolerance,double relative_tolerance,int sequence,OscillatoryWorkspace workspace,
    MomentCache& cache,Trigonometry math) {
    constexpr double epsilon=0x1p-52,huge=0x1.fffffffffffffp1023;
    auto& w=workspace.intervals;
    IntegralResult out{};
    w.left[0]=start;w.right[0]=end;w.integrals[0]=0;w.errors[0]=0;w.order[0]=0;workspace.levels[0]=0;
    if(selector<1 || selector>2 || (absolute_tolerance<=0 && relative_tolerance<50.0L*epsilon)
       || sequence<1 || cache.rows<1) { out.status=6;return out; }
    const double omega=binary64(x87::abs(frequency));
    if(sequence<2)cache.computed=0;
    const auto first_rule=oscillatory_rule(f,start,end,omega,selector,0,false,cache,math);
    const auto& first=first_rule.value;
    out.integral=first.integral;out.error=first.error;out.evaluations=first_rule.evaluations;
    auto tolerance=[&](double value) {
        const long double relative=x87::abs(value)*relative_tolerance;
        return absolute_tolerance>=relative?absolute_tolerance:binary64(relative);
    };
    double target=tolerance(out.integral);
    w.integrals[0]=out.integral;w.errors[0]=out.error;w.order[0]=1;
    if(out.error<=(100.0L*epsilon)*first.absolute && target<out.error)out.status=2;
    if(w.limit==1)out.status=1;
    // DQAWOE leaves LAST at zero on this initial-return path.
    if(out.status || out.error<=target)goto sign_result;
    {
        double table[52],history[3];
        double maximum_error=out.error,area=out.integral,error_sum=out.error,
            small=binary64(x87::abs((long double)end-start)*0.75L),error_large=0,extrap_target=0,correction=0;
        out.error=huge;
        int maximum=1,rank=1,extrap_calls=0,without_improvement=0,table_length=0,
            roundoff1=0,roundoff2=0,roundoff3=0,extrap_status=0;
        bool extrapolating=false,no_extrapolation=false;
        const long double span=x87::abs((long double)end-start);
        bool eligible=!(2.0L<span*0.5L*omega);
        if(eligible)table[table_length++]=out.integral;
        eligible=(span*0.25L*omega<=2.0L) || eligible;
        const int sign=(1.0L-50.0L*epsilon)*first.absolute<=x87::abs(out.integral)?1:-1;
        for(out.intervals=2;out.intervals<=w.limit;++out.intervals) {
            const int at=maximum-1,new_at=out.intervals-1,level=workspace.levels[at]+1;
            const double left=w.left[at],middle=binary64(((long double)w.left[at]+w.right[at])*0.5L),
                right=w.right[at],previous_error=maximum_error;
            const auto rule_a=oscillatory_rule(f,left,middle,omega,selector,level,false,cache,math);
            out.evaluations+=rule_a.evaluations;
            const auto rule_b=oscillatory_rule(f,middle,right,omega,selector,level,true,cache,math);
            out.evaluations+=rule_b.evaluations;
            const auto& a=rule_a.value;const auto& b=rule_b.value;
            const long double area12=(long double)a.integral+b.integral;
            const double error12=binary64((long double)a.error+b.error);
            error_sum=binary64(((long double)error_sum+error12)-maximum_error);
            area=binary64(((long double)area+area12)-w.integrals[at]);
            if(a.error!=a.ascendant && b.error!=b.ascendant) {
                if(!(x87::abs(area12)*1e-5<x87::abs((long double)w.integrals[at]-area12))
                   && !((long double)maximum_error*0.99>error12)) {
                    if(extrapolating)++roundoff2;else ++roundoff1;
                }
                if(out.intervals>10 && maximum_error<error12)++roundoff3;
            }
            w.integrals[at]=a.integral;w.integrals[new_at]=b.integral;
            workspace.levels[at]=level;workspace.levels[new_at]=level;
            target=tolerance(area);
            if(roundoff1+roundoff2>9 || roundoff3>19)out.status=2;
            if(roundoff2>4)extrap_status=3;
            if(out.intervals==w.limit)out.status=1;
            const long double largest=x87::abs(left)>=x87::abs(right)?x87::abs(left):x87::abs(right);
            if(largest<=(1000.0L*0x1p-1022L+x87::abs(middle))*(100.0L*epsilon+1.0L))out.status=4;
            if(a.error<b.error) {
                w.left[at]=middle;w.left[new_at]=left;w.right[new_at]=middle;
                w.integrals[at]=b.integral;w.integrals[new_at]=a.integral;
                w.errors[at]=b.error;w.errors[new_at]=a.error;
            } else {
                w.left[new_at]=middle;w.right[at]=middle;w.right[new_at]=right;
                w.errors[at]=a.error;w.errors[new_at]=b.error;
            }
            sort_errors(w.limit,out.intervals,maximum,maximum_error,w.errors,w.order,rank);
            if(error_sum<=target)goto sum;
            if(out.status)break;
            if(out.intervals==2 && eligible) {
                small=binary64((long double)small*0.5L);table[table_length++]=area;
                extrap_target=target;error_large=error_sum;continue;
            }
            if(no_extrapolation)continue;
            if(eligible) {
                const long double reduced=(long double)error_large-previous_error;
                error_large=binary64(reduced);
                if(small<x87::abs((long double)middle-left))error_large=binary64(reduced+error12);
            }
            if(!extrapolating) {
                const long double width=x87::abs((long double)w.right[maximum-1]-w.left[maximum-1]);
                if(width>small)continue;
                if(!eligible) {
                    small=binary64((long double)small*0.5L);
                    if(width*0.25L*omega<=2.0L) { eligible=true;extrap_target=target;error_large=error_sum; }
                    continue;
                }
                extrapolating=true;rank=2;
            }
            if(extrap_status!=3 && !(error_large<=extrap_target)) {
                const int count=out.intervals>w.limit/2+2?w.limit-out.intervals+3:out.intervals;
                bool found=false;
                for(int remaining=count-rank;remaining>=0;--remaining) {
                    maximum=w.order[rank-1];maximum_error=w.errors[maximum-1];
                    if(small<x87::abs((long double)w.right[maximum-1]-w.left[maximum-1])) { found=true;break; }
                    ++rank;
                }
                if(found)continue;
            }
            table[table_length++]=area;
            if(table_length>2) {
                double estimate,estimate_error;
                extrapolate(table_length,table,estimate,estimate_error,history,extrap_calls);
                ++without_improvement;
                if(without_improvement>5 && out.error<(long double)error_sum*0.001)out.status=5;
                if(!(estimate_error>=out.error)) {
                    without_improvement=0;out.error=estimate_error;out.integral=estimate;
                    correction=error_large;extrap_target=tolerance(estimate);
                    if(out.error<=extrap_target)break;
                }
                if(table_length==1)no_extrapolation=true;
                if(out.status==5)break;
            }
            maximum=w.order[0];maximum_error=w.errors[maximum-1];rank=1;extrapolating=false;
            small=binary64((long double)small*0.5L);error_large=error_sum;
        }
        if(out.error==huge || extrap_calls==0)goto sum;
        if(extrap_status+out.status!=0) {
            if(extrap_status==3)out.error=binary64((long double)correction+out.error);
            if(out.status==0)out.status=3;
            if(out.integral==0 || area==0) {
                if(error_sum<out.error)goto sum;
                if(area==0)goto finish;
            } else if((long double)error_sum/x87::abs(area)<(long double)out.error/x87::abs(out.integral))goto sum;
        }
        if(sign==-1) {
            const long double largest=x87::abs(out.integral)>=x87::abs(area)?x87::abs(out.integral):x87::abs(area);
            if(largest<=0.01*first.absolute)goto finish;
        }
        {
            const long double ratio=(long double)out.integral/area;
            if(ratio<0.01 || ratio>100 || x87::abs(area)<=error_sum)out.status=6;
        }
        goto finish;
sum:
        out.integral=0;
        for(int i=0;i<out.intervals;++i)out.integral=binary64((long double)out.integral+w.integrals[i]);
        out.error=error_sum;
    }
finish:
    if(out.status>2)--out.status;
sign_result:
    if(selector==2 && frequency<0)out.integral=-out.integral;
    return out;
}

FourierResult integrate_fourier(Integrand f,double start,double frequency,int selector,
    double absolute_tolerance,FourierWorkspace w,Trigonometry math) {
    FourierResult out{};
    if(selector<1 || selector>2 || absolute_tolerance<=0 || w.cycle_limit<3) { out.status=6;return out; }
    if(frequency==0) {
        if(selector==1) {
            const auto value=integrate_infinite(f,0,1,absolute_tolerance,0,w.cycle.intervals);
            out.integral=value.integral;out.error=value.error;out.evaluations=value.evaluations;out.status=value.status;
        }
        w.integrals[0]=out.integral;w.errors[0]=out.error;w.statuses[0]=out.status;out.cycles=1;
        return out;
    }
    const long double absolute_frequency=x87::abs(frequency);
    const int truncated=x87::truncate_int(absolute_frequency);
    const int odd=static_cast<int>(2u*static_cast<unsigned>(truncated)+1u);
    const double cycle_length=binary64(((long double)odd*0x1.921fb54442d18p+1)/absolute_frequency);
    constexpr double decay=0x1.ccccccccccccdp-1;
    const double one_minus=binary64(1.0L-decay);
    double eps=absolute_tolerance;
    if(0x1p-1022L/one_minus<eps)eps=binary64((long double)one_minus*eps);
    const double initial_eps=eps;
    double factor=1.0,correction=0,error_sum=0,tail=0,left=start,right=binary64((long double)cycle_length+start),
        table[52];
    double* const history=w.extrapolation_history;
    int table_length=0,previous_length=0,extrap_calls=0,without_improvement=0;
    MomentCache cache{w.moments,w.moment_rows,0};
    for(out.cycles=1;out.cycles<=w.cycle_limit;++out.cycles) {
        const int at=out.cycles-1;
        const double tolerance=binary64((long double)eps*factor);
        const auto value=integrate_oscillatory(f,left,right,frequency,selector,tolerance,0,out.cycles,w.cycle,cache,math);
        w.integrals[at]=value.integral;w.errors[at]=value.error;w.statuses[at]=value.status;
        out.evaluations+=value.evaluations;
        factor=binary64((long double)factor*decay);
        const long double updated_sum=(long double)error_sum+value.error;
        error_sum=binary64(updated_sum);tail=binary64(x87::abs(value.integral)*50.0L);
        // The first convergence test keeps the unrounded error accumulator.
        if(updated_sum+tail<=absolute_tolerance && out.cycles>5)goto direct_sum;
        if(!(correction>=value.error))correction=value.error;
        if(value.status!=0) {
            const long double updated_eps=(long double)correction*one_minus;
            eps=initial_eps>=updated_eps?initial_eps:binary64(updated_eps);
            out.status=7;
        }
        if(out.status==7 && (long double)error_sum+tail<=(long double)correction*10.0L && out.cycles>5)goto direct_sum;
        ++table_length;
        if(out.cycles<2)table[0]=value.integral;
        else {
            table[table_length-1]=binary64((long double)table[previous_length-1]+value.integral);
            if(out.cycles!=2) {
                if(out.cycles==w.cycle_limit)out.status=1;
                double estimate,estimate_error;
                extrapolate(table_length,table,estimate,estimate_error,history,extrap_calls);
                ++without_improvement;
                if(without_improvement>14 && out.error<=((long double)error_sum+tail)*0.001)out.status=4;
                if(estimate_error<=out.error || out.cycles==3) {
                    out.error=estimate_error;out.integral=estimate;without_improvement=0;
                    if((long double)correction*10.0L+out.error<=absolute_tolerance ||
                       (estimate_error<=absolute_tolerance && absolute_tolerance<=(long double)correction*10.0L))break;
                }
                if(out.status!=0 && out.status!=7)break;
            }
        }
        previous_length=table_length;left=right;right=binary64((long double)cycle_length+right);
    }
    out.error=binary64((long double)correction*10.0L+out.error);
    if(out.status==0)return out;
    if(out.integral==0 || table[table_length-1]==0) {
        if(error_sum<out.error)goto direct_sum;
        if(table[table_length-1]==0)return out;
    }
    if(((long double)error_sum+tail)/x87::abs(table[table_length-1])<
        (long double)out.error/x87::abs(out.integral))goto direct_sum;
    if(out.status>0 && out.status!=7)out.error=binary64((long double)tail+out.error);
    return out;
direct_sum:
    // This jump can precede appending the current cycle. Preserve the original
    // table-tail selection rather than recomputing a sum of all cycle entries.
    out.integral=table[table_length-1];out.error=binary64((long double)error_sum+tail);
    return out;
}

}
