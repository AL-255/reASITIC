#include "pi_shunts.hpp"
#include <new>
extern "C" {
#include "libbf.h"
void* realloc(void*,size_t);
void free(void*);
void* malloc(size_t);
int sprintf(char*,const char*,...);
}
namespace {
struct Context {
    bf_context_t value;int status=0;
    static void* allocate(void*,void* p,size_t size){if(!size){free(p);return nullptr;}return realloc(p,size);}
    Context(){bf_context_init(&value,allocate,nullptr);}
    ~Context(){bf_context_end(&value);}
};
struct Number {
    Context* context;bf_t value;
    explicit Number(Context& c):context(&c){bf_init(&c.value,&value);bf_set_zero(&value,0);}
    Number(Context& c,double x):Number(c){c.status|=bf_set_float64(&value,x);}
    Number(const Number& x):Number(*x.context){context->status|=bf_set(&value,&x.value);}
    ~Number(){bf_delete(&value);}
    Number& operator=(const Number& x){if(this!=&x)context->status|=bf_set(&value,&x.value);return *this;}
    double rounded()const{double x;context->status|=bf_get_float64(&value,&x,BF_RNDN);return x;}
};
Number operator+(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_add(&r.value,&a.value,&b.value,128,BF_RNDN);return r;}
Number operator-(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_sub(&r.value,&a.value,&b.value,128,BF_RNDN);return r;}
Number operator*(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_mul(&r.value,&a.value,&b.value,128,BF_RNDN);return r;}
Number operator/(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_div(&r.value,&a.value,&b.value,128,BF_RNDN);return r;}
struct Complex {
    Number real,imag;
    explicit Complex(Context& c):real(c),imag(c){}
    Complex(Context& c,const double* p):real(c,p[0]),imag(c,p[1]){}
    Complex(Number r,Number i):real(r),imag(i){}
};
Complex operator+(const Complex& a,const Complex& b){return {a.real+b.real,a.imag+b.imag};}
Complex operator-(const Complex& a,const Complex& b){return {a.real-b.real,a.imag-b.imag};}
Complex operator*(const Complex& a,const Complex& b){return {a.real*b.real-a.imag*b.imag,a.real*b.imag+a.imag*b.real};}
Complex operator/(const Complex& a,const Complex& b){const auto norm=b.real*b.real+b.imag*b.imag;return {(a.real*b.real+a.imag*b.imag)/norm,(a.imag*b.real-a.real*b.imag)/norm};}
Number norm(const Complex& a){return a.real*a.real+a.imag*a.imag;}
struct Values {
    Complex* data;int count;
    Values(Context& c,int n):data(static_cast<Complex*>(malloc(sizeof(Complex)*n))),count(n){if(data)for(int i=0;i<n;++i)new(data+i)Complex(c);}
    ~Values(){if(data){for(int i=0;i<count;++i)data[i].~Complex();free(data);}}
    Complex& operator[](int i){return data[i];}
};
bool finite(const double* values,int n,int stride){if(!values||stride<n)return false;for(int j=0;j<n;++j)for(int i=0;i<2*n;++i)if(!__builtin_isfinite(values[2*j*stride+i]))return false;return true;}
}
namespace reasitic {
bool common_mode_shunts(const double* cap,const double* ind,int n,int cap_stride,int ind_stride,
                       const double* pads,int pad_stride,PiShunts& out){
    if(n<1||n>1024||!finite(cap,n,cap_stride)||!finite(ind,n,ind_stride)||(pads&&!finite(pads,2,pad_stride)))return false;
    Context c;const int m=n-1;Values a(c,m*m),delta(c,n+1),rhs(c,m);
    if(!delta.data||(m&&(!a.data||!rhs.data)))return false;
    const auto C=[&](int i,int j){return Complex(c,cap+2*(i+j*cap_stride));};
    const auto Y=[&](int i,int j){return Complex(c,ind+2*(i+j*ind_stride));};
    const Complex zero(c);
    // V=1+delta, with delta=0 at both endpoints. The chain voltage drops
    // depend only on delta. The capacitance row sum is its raw diagonal, so
    // neither the forcing term nor the port currents subtract large values.
    for(int i=1;i<n;++i){
        rhs[i-1]=zero-C(i,i);
        for(int j=1;j<n;++j){
            Complex capacitance(c);
            if(i==j)for(int k=0;k<n;++k)capacitance=capacitance+C(i,k);
            else capacitance=zero-C(i,j);
            a[(i-1)*m+j-1]=capacitance+(Y(i,j)-Y(i-1,j))-(Y(i,j-1)-Y(i-1,j-1));
        }
    }
    // Partial-pivot elimination with 128 bits of software guard precision.
    // No host transcendental or floating-point control mode is used.
    for(int k=0;k<m;++k){
        int pivot=k;auto largest=norm(a[k*m+k]);
        for(int i=k+1;i<m;++i){auto candidate=norm(a[i*m+k]);if(bf_cmp_lt(&largest.value,&candidate.value)){largest=candidate;pivot=i;}}
        if(bf_is_zero(&largest.value)||bf_is_nan(&largest.value))return false;
        if(pivot!=k){for(int j=k;j<m;++j){const auto saved=a[k*m+j];a[k*m+j]=a[pivot*m+j];a[pivot*m+j]=saved;}const auto saved=rhs[k];rhs[k]=rhs[pivot];rhs[pivot]=saved;}
        for(int i=k+1;i<m;++i){const auto factor=a[i*m+k]/a[k*m+k];for(int j=k+1;j<m;++j)a[i*m+j]=a[i*m+j]-factor*a[k*m+j];rhs[i]=rhs[i]-factor*rhs[k];}
    }
    for(int i=m-1;i>=0;--i){auto value=rhs[i];for(int j=i+1;j<m;++j)value=value-a[i*m+j]*delta[j+1];delta[i+1]=value/a[i*m+i];}
    auto near=C(0,0);Complex far(c);
    for(int j=1;j<n;++j)near=near-C(0,j)*delta[j];
    for(int j=0;j<n;++j){const auto drop=delta[j]-delta[j+1];near=near+Y(0,j)*drop;far=far-Y(n-1,j)*drop;}
    // Row sums of the two-pad admittance are its two raw self-capacitances;
    // the mutual term cancels analytically before numerical subtraction.
    if(pads){near=near-Complex(c,pads);far=far-Complex(c,pads+2*(1+pad_stride));}
    const PiShunts result{far.real.rounded(),far.imag.rounded(),near.real.rounded(),near.imag.rounded()};
    const double values[]={result.first_real,result.first_imag,result.second_real,result.second_imag};
    if(c.status&(BF_ST_MEM_ERROR|BF_ST_INVALID_OP|BF_ST_DIVIDE_ZERO|BF_ST_OVERFLOW))return false;
    for(double value:values)if(!__builtin_isfinite(value))return false;
    out=result;return true;
}
}
extern "C" const char* probe_pi_shunts(const double* cap,const double* ind,int n,const double* pads,int pad_stride){
    static char output[256];reasitic::PiShunts result;
    if(!reasitic::common_mode_shunts(cap,ind,n,n,n,pads,pad_stride,result))return "{\"error\":\"Common-mode shunt solve failed\"}";
    sprintf(output,"{\"first\":[%.17g,%.17g],\"second\":[%.17g,%.17g],\"precision_bits\":128}",result.first_real,result.first_imag,result.second_real,result.second_imag);return output;
}
