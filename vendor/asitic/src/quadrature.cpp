#include "asitic/quadrature.hpp"
#include "x87.hpp"

namespace asitic::quadpack {
namespace {
using x87::binary64;
constexpr double epsilon=0x1p-52,huge=0x1.fffffffffffffp1023;
}
double error_power(double x) {
    if(x==0)return 0.0;
    if(__builtin_isinf(x))return x87::binary64(x87::abs(x));
    if(x<0 || __builtin_isnan(x))return x87::binary64(x87::sqrt(x));
    long double logarithm;
    const long double delta=(long double)x-1.0L;
    if(x87::abs(delta)<=0x1.28f5c28f5c28fp-2L) {
        asm("fld1; fxch; fyl2xp1" : "=t"(logarithm) : "0"(delta) : "st(1)");
    } else {
        asm("fld1; fxch; fyl2x" : "=t"(logarithm) : "0"((long double)x) : "st(1)");
    }
    const long double exponent=logarithm*1.5L,integer=x87::round(exponent);
    return x87::binary64(x87::scale(x87::pow2m1(exponent-integer)+1.0L,integer));
}
void sort_errors(int limit,int last,int& maximum,double& error,const double* errors,int* order,int& rank) {
    if(last<=2) { order[0]=1;order[1]=2; }
    else {
        const double top=errors[maximum-1];
        for(int remaining=rank-2;remaining>=0;--remaining) {
            if(top<=errors[order[rank-2]-1])break;
            order[rank-1]=order[rank-2];--rank;
        }
        const int count=last>limit/2+2?limit-last+3:last;
        const double bottom=errors[last-1];
        const int end=count-1;
        int next=rank+1;
        while(next<=end && !(top>=errors[order[next-1]-1])) {
            order[next-2]=order[next-1];++next;
        }
        if(next>end) { order[count-2]=maximum;order[count-1]=last; }
        else {
            order[next-2]=maximum;
            int k=end;
            while(k>=next && !(bottom<errors[order[k-1]-1])) {
                order[k]=order[k-1];--k;
            }
            order[k]=last;
        }
    }
    maximum=order[rank-1];error=errors[maximum-1];
}
void extrapolate(int& length,double* table,double& result,double& error,double* history,int& calls) {
    ++calls;error=huge;result=table[length-1];
    if(length>=3) {
        const int original_length=length,new_elements=(length-1)/2;
        table[length+1]=result;table[length-1]=huge;
        int k=length;
        for(int i=1;i<=new_elements;++i) {
            const double e2=table[k+1],e0=table[k-3],e1=table[k-2];
            const long double e1abs=x87::abs(e1),delta2=(long double)e2-e1,delta3=(long double)e1-e0;
            const double d2=binary64(delta2),d3=binary64(delta3),err2=binary64(x87::abs(delta2)),
                tol2=binary64((x87::abs(e2)>=e1abs?x87::abs(e2):e1abs)*epsilon);
            const long double err3=x87::abs(delta3),tol3=(e1abs>=x87::abs(e0)?e1abs:x87::abs(e0))*epsilon;
            if(!(err2>tol2) && !(tol3<err3)) {
                result=e2;error=binary64((long double)err2+err3);
                goto finish;
            }
            const double e3=table[k-1];table[k-1]=e1;
            // Delta1 stays extended for the reciprocal; delta2/3 are reloaded
            // from binary64 stores at 0x080a95c7/0x080a95d2.
            const long double delta1=(long double)e1-e3;
            const double err1=binary64(x87::abs(delta1));
            const long double tol1=(x87::abs(e3)<=e1abs?e1abs:x87::abs(e3))*epsilon;
            if(tol1>=err1 || err2<=tol2 || err3<=tol3) { length=2*i-1;break; }
            const long double s=(1.0L/d2+1.0L/delta1)-1.0L/d3;
            // 0x080a95ee continues only for an ordered greater comparison.
            // In particular, a NaN also truncates the extrapolation table.
            if(!(x87::abs(s*e1)>0.0001)) { length=2*i-1;break; }
            const long double estimate=(long double)e1+1.0L/s;
            table[k-1]=binary64(estimate);k-=2;
            const long double estimate_error=(x87::abs(estimate-e2)+err2)+err3;
            if(!(estimate_error>error)) { error=binary64(estimate_error);result=binary64(estimate); }
        }
        if(length==50)length=49;
        int at=(original_length%2==0)?1:0;
        for(int i=0;i<=new_elements;++i,at+=2)table[at]=table[at+2];
        if(original_length!=length)for(int i=0;i<length;++i)table[i]=table[original_length-length+i];
        if(calls<4) { history[calls-1]=result;error=huge; }
        else {
            error=binary64((x87::abs((long double)result-history[2])+x87::abs((long double)result-history[1]))
                           +x87::abs((long double)result-history[0]));
            history[0]=history[1];history[1]=history[2];history[2]=result;
        }
    }
finish:
    const long double floor=(5.0L*epsilon)*x87::abs(result);
    if(!(error>=floor))error=binary64(floor);
}
RuleResult infinite_rule(Integrand f,double bound,int direction,double a,double b) {
    constexpr double gauss[]={0,0,0x1.092f69f826d57p-3,0,0x1.1e6b1713d8644p-2,0,0x1.86fe74ee32b3dp-2,0};
    constexpr double kronrod[]={0x1.abfd7e03c2fa6p-2,0x1.77c5b67d57470p-6,0x1.026cdaa7b61c4p-4,
        0x1.ad384a34814c6p-4,0x1.200ed0f46e8c1p-3,0x1.5a1f266e47d5cp-3,0x1.85d6861c80eb1p-3,0x1.a2adbcbec9cd8p-3};
    constexpr double nodes[]={0x1.ad04f9087090fp-3,0x1.fba009d4d09b1p-1,0x1.e5f178e7c6229p-1,
        0x1.bacf827b9bb3ep-1,0x1.7ba9f9be3a1d6p-1,0x1.2c13a049dfa24p-1,0x1.9f95df119fd62p-2,0x1.a98b2892e0c77p-3};
    const int sign=direction>1?1:direction;
    const double center=binary64(((long double)a+b)*0.5L),half=binary64(((long double)b-a)*0.5L);
    double x=binary64(((1.0L-center)*sign)/center+bound),fc=binary64(f(&x));
    if(direction==2) { const double negative=-x;fc=binary64(f(&negative)+(long double)fc); }
    const long double central=((long double)fc/center)/center;
    const double central64=binary64(central);
    double g=binary64(central*kronrod[0]),k=binary64((long double)central64*nodes[0]);
    RuleResult out{};
    out.absolute=binary64(x87::abs((long double)central64*nodes[0]));
    double left[7],right[7];
    for(int j=1;j<=7;++j) {
        const long double offset=(long double)half*nodes[j],low=(long double)center-offset;
        const double low64=binary64(low),high=binary64(offset+center);
        // The left FST at 0x080a98b5 retains the extended abscissa for
        // forming x. Its rounded copy is used later to rescale f(x).
        x=binary64(((1.0L-low)*sign)/low+bound);
        const double y=binary64(((1.0L-high)*sign)/high+bound);
        double f1=binary64(f(&x)),f2=binary64(f(&y));
        if(direction==2) {
            double negative=-x;f1=binary64(f(&negative)+(long double)f1);
            negative=-y;f2=binary64(f(&negative)+(long double)f2);
        }
        left[j-1]=binary64(((long double)f1/low64)/low64);
        right[j-1]=binary64(((long double)f2/high)/high);
        const long double sum=(long double)left[j-1]+right[j-1];
        g=binary64(sum*gauss[j]+g);k=binary64(sum*kronrod[j]+k);
        out.absolute=binary64((x87::abs(left[j-1])+x87::abs(right[j-1]))*kronrod[j]+out.absolute);
    }
    const long double mean=(long double)k*0.5L;
    out.ascendant=binary64(x87::abs((long double)central64-mean)*nodes[0]);
    for(int j=1;j<=7;++j)
        out.ascendant=binary64((x87::abs((long double)left[j-1]-mean)+x87::abs((long double)right[j-1]-mean))
            *kronrod[j]+out.ascendant);
    out.integral=binary64((long double)k*half);
    const long double asc=(long double)half*out.ascendant;
    out.ascendant=binary64(asc);out.absolute=binary64((long double)half*out.absolute);
    const long double raw=x87::abs(((long double)k-g)*half);
    out.error=binary64(raw);
    if(asc!=0 && raw!=0) {
        const double ratio=binary64((raw*200.0L)/out.ascendant),power=error_power(ratio);
        out.error=binary64((power<=1.0?(long double)power:1.0L)*out.ascendant);
    }
    if(0x1p-1022L/(50.0L*epsilon)<out.absolute) {
        const long double floor=(50.0L*epsilon)*out.absolute;
        if(floor>=out.error)out.error=binary64(floor);
    }
    return out;
}
IntegralResult integrate_infinite(Integrand f,double bound,int direction,double absolute_tolerance,
                                  double relative_tolerance,InfiniteWorkspace w) {
    IntegralResult out{};
    w.left[0]=0;w.right[0]=1;w.integrals[0]=0;w.errors[0]=0;w.order[0]=0;
    if(absolute_tolerance<=0 && relative_tolerance<50.0L*epsilon) { out.status=6;return out; }
    if(direction==2)bound=0;
    const auto first=infinite_rule(f,bound,direction,0,1);
    out.integral=first.integral;out.error=first.error;out.intervals=1;
    w.integrals[0]=out.integral;w.errors[0]=out.error;w.order[0]=1;
    auto tolerance=[&](double value) {
        const long double relative=x87::abs(value)*relative_tolerance;
        return absolute_tolerance>=relative?absolute_tolerance:binary64(relative);
    };
    double target=tolerance(out.integral);
    if(out.error<=(100.0L*epsilon)*first.absolute && target<out.error)out.status=2;
    if(w.limit==1)out.status=1;
    if(out.status || (out.error<=target && first.ascendant!=out.error) || out.error==0)goto finish;
    {
        double table[52],history[3];
        table[0]=out.integral;
        double maximum_error=out.error,area=out.integral,error_sum=out.error,
            small=0,error_large=0,extrap_target=0,correction=0;
        out.error=huge;
        int maximum=1,rank=1,extrap_calls=0,without_improvement=0,table_length=2,
            roundoff1=0,roundoff2=0,roundoff3=0,extrap_status=0;
        bool extrapolating=false,no_extrapolation=false;
        const int sign=(1.0L-50.0L*epsilon)*first.absolute<=x87::abs(out.integral)?1:-1;
        for(out.intervals=2;out.intervals<=w.limit;++out.intervals) {
            const int at=maximum-1,new_at=out.intervals-1;
            const double left=w.left[at],middle=binary64(((long double)w.left[at]+w.right[at])*0.5L),
                right=w.right[at],previous_error=maximum_error;
            const auto a=infinite_rule(f,bound,direction,left,middle),
                       b=infinite_rule(f,bound,direction,middle,right);
            // The area sum survives in an x87 register across the accumulator
            // updates; the child error sum is stored and reloaded first.
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
            if(out.intervals==2) {
                small=0.375;extrap_target=target;error_large=error_sum;table[1]=area;
            } else if(!no_extrapolation) {
                // 0x080a68be stores reduced without popping it before the
                // optional addition. Rounding that live value changes bits.
                const long double reduced=(long double)error_large-previous_error;
                error_large=binary64(reduced);
                if(small<x87::abs((long double)middle-left))error_large=binary64(reduced+error12);
                if(!extrapolating) {
                    if(small<x87::abs((long double)w.right[maximum-1]-w.left[maximum-1]))continue;
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
                maximum=w.order[0];maximum_error=w.errors[maximum-1];rank=1;extrapolating=false;
                small=binary64((long double)small*0.5L);error_large=error_sum;
            }
        }
        if(out.error==huge)goto sum;
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
            if(ratio<0.01 || ratio>100 || x87::abs(area)<error_sum)out.status=6;
        }
        goto finish;
sum:
        out.integral=0;
        for(int i=0;i<out.intervals;++i)out.integral=binary64((long double)out.integral+w.integrals[i]);
        out.error=error_sum;
    }
finish:
    out.evaluations=(30*out.intervals-15)*(direction==2?2:1);
    if(out.status>2)--out.status;
    return out;
}
}
