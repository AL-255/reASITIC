#include "portable_gmd.hpp"
extern "C" {
#include "platform.h"
#include "softfloat.h"
int sprintf(char*,const char*,...);
}
namespace {
using Q=float128_t;
const Q ln2={{0xf35793c7673007e6ULL,0x3ffe62e42fefa39eULL}};
const Q pi={{0x8469898cc51701b8ULL,0x4000921fb54442d1ULL}};
Q from(double x){union{double d;float64_t f;}v={x};return f64_to_f128(v.f);}
double to_double(Q x){union{float64_t f;double d;}v={f128_to_f64(x)};return v.d;}
Q integer(int x){return i32_to_f128(x);}Q neg(Q x){x.v[1]^=1ULL<<63;return x;}Q absq(Q x){x.v[1]&=~(1ULL<<63);return x;}
Q add(Q a,Q b){return f128_add(a,b);}Q sub(Q a,Q b){return add(a,neg(b));}Q mul(Q a,Q b){return f128_mul(a,b);}Q divq(Q a,Q b){return f128_div(a,b);}
bool zero(Q x){return !(x.v[0]|(x.v[1]&0x7fffffffffffffffULL));}
Q inverse_odd[81];bool planned=false;
void plan(){if(planned)return;for(int i=0;i<81;++i)inverse_odd[i]=divq(integer(1),integer(2*i+1));planned=true;}
Q logarithm(Q x){
 const int exponent=static_cast<int>((x.v[1]>>48)&0x7fff)-16383;x.v[1]=(x.v[1]&0x0000ffffffffffffULL)|0x3fff000000000000ULL;
 const Q z=divq(sub(x,integer(1)),add(x,integer(1))),square=mul(z,z);plan();Q sum=inverse_odd[80];
 for(int i=79;i>=0;--i)sum=add(inverse_odd[i],mul(square,sum));
 return add(mul(integer(exponent),ln2),mul(integer(2),mul(z,sum)));
}
Q arctangent(Q x){
 const bool negative=x.v[1]>>63;x=absq(x);const bool reciprocal=f128_lt(integer(1),x);if(reciprocal)x=divq(integer(1),x);
 const bool shifted=f128_lt(from(.41421356237309503),x);if(shifted)x=divq(sub(x,integer(1)),add(x,integer(1)));
 plan();const Q square=neg(mul(x,x));Q sum=inverse_odd[80];for(int i=79;i>=0;--i)sum=add(inverse_odd[i],mul(square,sum));Q out=mul(x,sum);
 if(shifted)out=add(divq(pi,integer(4)),out);if(reciprocal)out=sub(divq(pi,integer(2)),out);return negative?neg(out):out;
}
Q exponential(Q x){
 const Q nf=f128_roundToInt(divq(x,ln2),softfloat_round_near_even,false);const int n=f128_to_i32(nf,softfloat_round_near_even,false);const Q r=sub(x,mul(nf,ln2));
 Q term=integer(1),sum=term;for(int i=1;i<=80;++i){term=divq(mul(term,r),integer(i));sum=add(sum,term);}sum.v[1]=static_cast<unsigned long long>(static_cast<long long>(sum.v[1])+static_cast<long long>(n)*0x1000000000000LL);return sum;
}
Q primitive(Q x,Q y,Q& bound){
 // This mixed antiderivative satisfies G_xxyy = -log(x*x+y*y)-25/6.
 // Its signed corner sum therefore gives the average logarithmic distance
 // after division by -2*volume and subtraction of 25/12. Keep all cancelling
 // terms wide: rounding each corner to double makes the result ill-conditioned.
 const Q x2=mul(x,x),y2=mul(y,y),x4=mul(x2,x2),y4=mul(y2,y2);Q logarithmic,angular=integer(0);
 if(!zero(x)&&!zero(y)){
  logarithmic=divq(mul(logarithm(add(x2,y2)),add(sub(x4,mul(integer(6),mul(x2,y2))),y4)),integer(24));
  angular=neg(divq(mul(mul(x,y),add(mul(x2,arctangent(divq(y,x))),mul(y2,arctangent(divq(x,y))))),integer(3)));
 }else{const Q square=zero(x)?y2:x2;logarithmic=zero(square)?integer(0):divq(mul(logarithm(square),mul(square,square)),integer(24));}
 // Include the polynomial scale even when log(x*x+y*y) is almost zero:
 // rounding 1+epsilon inside the logarithm can otherwise hide the error.
 bound=add(bound,add(add(absq(logarithmic),absq(angular)),mul(add(x2,y2),add(x2,y2))));return add(logarithmic,angular);
}
int compare_guarded(Q a,Q b){const Q margin={{0,static_cast<unsigned long long>(16383-108)<<48}},difference=sub(a,b),error=mul(add(absq(a),absq(b)),margin);return f128_lt(error,difference)?1:f128_lt(difference,neg(error))?-1:0;}
int exponent(Q x){return static_cast<int>((x.v[1]>>48)&0x7fff)-16383;}
}
namespace reasitic {
double error_power_stable(double x){
 // QUADPACK needs x^(3/2) for a nonnegative error ratio. This form avoids
 // a processor-dependent logarithm/exponential pair and its underflow stages.
 if(__builtin_isnan(x)||x<0)return __builtin_nan("");if(x==0)return 0.;if(__builtin_isinf(x))return x;
 const unsigned saved=softfloat_roundingMode;softfloat_roundingMode=softfloat_round_near_even;
 const Q input=from(x),value=mul(input,f128_sqrt(input)),margin={{0,static_cast<unsigned long long>(16383-108)<<48}},error=mul(absq(value),margin);
 const double lower=to_double(sub(value,error)),upper=to_double(add(value,error));softfloat_roundingMode=saved;
 return lower==upper?lower:error_power_big(x);
}
GeometricMeanDistance geometric_mean_distance(double w1,double w2,double t1,double t2,double dx,double dy){
 const unsigned saved=softfloat_roundingMode;softfloat_roundingMode=softfloat_round_near_even;
 struct Restore{unsigned mode;~Restore(){softfloat_roundingMode=mode;}}restore{saved};
 const double inputs[]={w1,w2,t1,t2,dx,dy};double scale=0;
 for(int i=0;i<6;++i){if(!__builtin_isfinite(inputs[i])||(i<4&&inputs[i]<=0))return {__builtin_nan(""),0,false,128};const double magnitude=inputs[i]<0?-inputs[i]:inputs[i];if(magnitude>scale)scale=magnitude;}
 const Q rawx=from(dx),rawy=from(dy),radial_square=add(mul(rawx,rawx),mul(rawy,rawy)),raw_width=add(from(w1),from(w2)),raw_thickness=add(from(t1),from(t2));
 const int width_test=compare_guarded(radial_square,mul(from(2.25),mul(raw_width,raw_width))),thickness_test=compare_guarded(radial_square,mul(from(2.25),mul(raw_thickness,raw_thickness)));
 // Preserve the model's far-field approximation. Decide a borderline cutoff
 // with exact dyadic arithmetic rather than a rounded radius or width sum.
 const bool farfield=width_test<0||thickness_test<0?false:width_test>0&&thickness_test>0?true:gmd_farfield_exact(w1,w2,t1,t2,dx,dy);
 if(farfield){
  const Q radius=f128_sqrt(radial_square),margin={{0,static_cast<unsigned long long>(16383-108)<<48}},error=mul(absq(radius),margin);const double lower=to_double(sub(radius,error)),upper=to_double(add(radius,error));const double result=lower==upper?lower:geometry_radius_big(dx,dy);return {result,0,static_cast<bool>(__builtin_isfinite(result)),128};
 }
 const Q s=from(scale),a=divq(from(w1),s),b=divq(from(w2),s),c=divq(from(t1),s),d=divq(from(t2),s),x=divq(from(dx),s),y=divq(from(dy),s),ws=add(a,b),ts=add(c,d);
 const Q half=from(.5),wd=sub(a,b),td=sub(c,d);
 const Q xs[]={sub(mul(wd,half),x),sub(neg(mul(ws,half)),x),sub(mul(ws,half),x),sub(neg(mul(wd,half)),x)};
 const Q ys[]={sub(mul(td,half),y),sub(mul(ts,half),y),sub(neg(mul(ts,half)),y),sub(neg(mul(td,half)),y)};
 Q rows[4],magnitude=integer(0);const int signs[]={1,-1,-1,1};
 for(int i=0;i<4;++i){Q terms[4];for(int j=0;j<4;++j){terms[j]=primitive(xs[i],ys[j],magnitude);if(signs[j]<0)terms[j]=neg(terms[j]);}rows[i]=add(add(terms[0],terms[1]),add(terms[2],terms[3]));if(signs[i]<0)rows[i]=neg(rows[i]);}
 const Q total=add(add(rows[0],rows[1]),add(rows[2],rows[3])),volume=mul(mul(a,b),mul(c,d));
 const int loss=exponent(magnitude)-exponent(volume);
 // 113 significand bits, with 53 result bits and a conservative guard margin.
 if(loss>40)return geometric_mean_distance_big(w1,w2,t1,t2,dx,dy);
 const Q average=sub(divq(neg(mul(total,half)),volume),divq(integer(25),integer(12)));
 const Q value=mul(s,exponential(average));
 // Bound roundoff growth by the size of the primitive terms relative to
 // the integration volume; fall back when the final double is ambiguous.
 Q relative={{0,static_cast<unsigned long long>(16383+loss-96)<<48}};const Q error=mul(absq(value),relative);
 const double lower=to_double(sub(value,error)),upper=to_double(add(value,error));
 if(lower!=upper)return geometric_mean_distance_big(w1,w2,t1,t2,dx,dy);
 return {lower,loss,__builtin_isfinite(lower)&&lower>=0,128};
}
}
extern "C" const char* probe_gmd(double w1,double w2,double t1,double t2,double dx,double dy){
 static char output[512];const auto r=reasitic::geometric_mean_distance(w1,w2,t1,t2,dx,dy);
 if(!r.reliable)sprintf(output,"{\"error\":\"Invalid or unrepresentable cross-section distance\",\"cancellation_bits\":%d,\"precision_bits\":%d}",r.cancellation_bits,r.precision_bits);
 else sprintf(output,"{\"distance\":%.17g,\"cancellation_bits\":%d,\"precision_bits\":%d}",r.distance,r.cancellation_bits,r.precision_bits);return output;
}

extern "C" const char* probe_error_power(double x){static char output[256];const double y=reasitic::error_power_stable(x);if(!__builtin_isfinite(y))return "{\"error\":\"Nonfinite power result\"}";sprintf(output,"{\"value\":%.17g}",y);return output;}
