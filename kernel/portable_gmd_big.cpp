#include "portable_gmd.hpp"
extern "C" {
#include "libbf.h"
void* realloc(void*,size_t);
void free(void*);
}
namespace {
struct Context {
    bf_context_t value;limb_t precision;int status=0;
    static void* allocate(void*,void* p,size_t size){if(!size){free(p);return nullptr;}return realloc(p,size);}
    explicit Context(limb_t p):precision(p){bf_context_init(&value,allocate,nullptr);}
    ~Context(){bf_context_end(&value);}
};
struct Number {
    Context* context;bf_t value;
    explicit Number(Context& c):context(&c){bf_init(&c.value,&value);}
    Number(Context& c,double x):Number(c){c.status|=bf_set_float64(&value,x);}
    Number(const Number& n):Number(*n.context){context->status|=bf_set(&value,&n.value);}
    Number(Number&& n):context(n.context),value(n.value){bf_init(&context->value,&n.value);}
    ~Number(){bf_delete(&value);}
    Number& operator=(const Number& n){if(this!=&n)context->status|=bf_set(&value,&n.value);return *this;}
    Number& operator=(Number&& n){if(this!=&n){bf_delete(&value);context=n.context;value=n.value;bf_init(&context->value,&n.value);}return *this;}
    bool zero()const{return bf_is_zero(&value);}
    double rounded()const{double x;context->status|=bf_get_float64(&value,&x,BF_RNDN);return x;}
};
Number operator+(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_add(&r.value,&a.value,&b.value,a.context->precision,BF_RNDN);return r;}
Number operator-(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_sub(&r.value,&a.value,&b.value,a.context->precision,BF_RNDN);return r;}
Number operator*(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_mul(&r.value,&a.value,&b.value,a.context->precision,BF_RNDN);return r;}
Number operator/(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_div(&r.value,&a.value,&b.value,a.context->precision,BF_RNDN);return r;}
Number operator-(const Number& a){Number r(a);bf_neg(&r.value);return r;}
bool operator<(const Number& a,const Number& b){return bf_cmp_lt(&a.value,&b.value);}
Number absolute(const Number& a){Number r(a);r.value.sign=0;return r;}
Number square_root(const Number& a){Number r(*a.context);a.context->status|=bf_sqrt(&r.value,&a.value,a.context->precision,BF_RNDN);return r;}
Number logarithm(const Number& a){Number r(*a.context);a.context->status|=bf_log(&r.value,&a.value,a.context->precision,BF_RNDN);return r;}
Number arctangent(const Number& a){Number r(*a.context);a.context->status|=bf_atan(&r.value,&a.value,a.context->precision,BF_RNDN);return r;}
Number exponential(const Number& a){Number r(*a.context);a.context->status|=bf_exp(&r.value,&a.value,a.context->precision,BF_RNDN);return r;}
Number primitive(const Number& x,const Number& y,Number& bound){
    auto& c=*x.context;Number logarithmic(c,0),angular(c,0);const auto x2=x*x,y2=y*y;
    if(!x.zero()&&!y.zero()){
        logarithmic=logarithm(x2+y2)*((x2*x2-Number(c,6)*x2*y2)+y2*y2)/Number(c,24);
        angular=-(x*y*(x2*arctangent(y/x)+y2*arctangent(x/y)))/Number(c,3);
    }else{const auto& square=x.zero()?y2:x2;if(!square.zero())logarithmic=logarithm(square)*square*square/Number(c,24);}
    bound=bound+absolute(logarithmic)+absolute(angular)+(x2+y2)*(x2+y2);return logarithmic+angular;
}
}
namespace reasitic {
bool gmd_farfield_exact(double w1,double w2,double t1,double t2,double dx,double dy){
 Context c(BF_PREC_INF);Number x(c,dx),y(c,dy),width=Number(c,w1)+Number(c,w2),thickness=Number(c,t1)+Number(c,t2),factor(c,2.25);const auto radial=x*x+y*y;return factor*width*width<radial&&factor*thickness*thickness<radial;
}
double geometry_radius_big(double dx,double dy){
 for(int p=128;p<=4096;p*=2){Context c(p);Number x(c,dx),y(c,dy);const auto radius=square_root(x*x+y*y);if(!c.status)return radius.rounded();Number margin(c,1);c.status|=bf_mul_2exp(&margin.value,8-p,p,BF_RNDN);const auto error=radius*margin;Number low(c),high(c);c.status|=bf_sub(&low.value,&radius.value,&error.value,p,BF_RNDD);c.status|=bf_add(&high.value,&radius.value,&error.value,p,BF_RNDU);const double a=low.rounded(),b=high.rounded();if(!(c.status&BF_ST_MEM_ERROR)&&a==b)return a;}
 return __builtin_nan("");
}
double error_power_big(double x){
 for(int p=128;p<=2048;p*=2){
  // Exact dyadic results can lie precisely halfway between doubles. An exact
  // operation status lets the final conversion apply ties-to-even directly.
  Context c(p);const Number input(c,x);const auto result=input*square_root(input);if(!c.status)return result.rounded();Number margin(c,1);c.status|=bf_mul_2exp(&margin.value,8-p,p,BF_RNDN);const auto error=absolute(result)*margin;
  Number low(c),high(c);c.status|=bf_sub(&low.value,&result.value,&error.value,p,BF_RNDD);c.status|=bf_add(&high.value,&result.value,&error.value,p,BF_RNDU);const double a=low.rounded(),b=high.rounded();if(!(c.status&BF_ST_MEM_ERROR)&&a==b)return a;
 }return __builtin_nan("");
}
GeometricMeanDistance geometric_mean_distance_big(double w1,double w2,double t1,double t2,double dx,double dy){
    const double input[]={w1,w2,t1,t2,dx,dy};double scale=0;for(int i=0;i<6;++i){const double m=input[i]<0?-input[i]:input[i];if(m>scale)scale=m;}
    int last_loss=0;
    for(int precision=128;precision<=16384;precision*=2){
        Context context(precision);Number s(context,scale),a=Number(context,w1)/s,b=Number(context,w2)/s,c=Number(context,t1)/s,d=Number(context,t2)/s,x=Number(context,dx)/s,y=Number(context,dy)/s,half(context,.5);
        const auto ws=a+b,wd=a-b,ts=c+d,td=c-d;
        Number xs[]={wd*half-x,-ws*half-x,ws*half-x,-wd*half-x};Number ys[]={td*half-y,ts*half-y,-ts*half-y,-td*half-y};
        Number total(context,0),magnitude(context,0);const int signs[]={1,-1,-1,1};
        for(int i=0;i<4;++i){Number terms[4]={primitive(xs[i],ys[0],magnitude),primitive(xs[i],ys[1],magnitude),primitive(xs[i],ys[2],magnitude),primitive(xs[i],ys[3],magnitude)};
            const auto row=(terms[0]-terms[1])+(terms[3]-terms[2]);total=signs[i]<0?total-row:total+row;
        }
        const auto volume=(a*b)*(c*d);last_loss=static_cast<int>(magnitude.value.expn-volume.value.expn);
        // Relative cancellation of the corner sum determines the working
        // precision. Retry until both ends of a roundoff margin store the same
        // double; the budget includes primitive evaluation and normalization.
        if(precision<last_loss+96)continue;
        const auto result=s*exponential(-(total*half)/volume-Number(context,25)/Number(context,12));
        Number relative(context,1);context.status|=bf_mul_2exp(&relative.value,last_loss+20-precision,precision,BF_RNDN);const auto error=absolute(result)*relative;
        Number lower(context),upper(context);context.status|=bf_sub(&lower.value,&result.value,&error.value,precision,BF_RNDD);context.status|=bf_add(&upper.value,&result.value,&error.value,precision,BF_RNDU);
        const double low=lower.rounded(),high=upper.rounded();
        if(!(context.status&BF_ST_MEM_ERROR)&&low==high&&__builtin_isfinite(low)&&low>=0)return {low,last_loss,true,precision};
    }
    return {__builtin_nan(""),last_loss,false,16384};
}
}
