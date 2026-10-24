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
bool store(Context& c,const Complex& far,const Complex& near,reasitic::PiShunts& out){
    const reasitic::PiShunts result{far.real.rounded(),far.imag.rounded(),near.real.rounded(),near.imag.rounded()};
    const double values[]={result.first_real,result.first_imag,result.second_real,result.second_imag};
    if(c.status&(BF_ST_MEM_ERROR|BF_ST_INVALID_OP|BF_ST_DIVIDE_ZERO|BF_ST_OVERFLOW))return false;
    for(double value:values)if(!__builtin_isfinite(value))return false;
    out=result;return true;
}

bool solve(Values& a,Values& rhs,Complex* solution,int m,int columns=1){
    // Partial-pivot elimination with 128 bits of software guard precision.
    // No host transcendental or floating-point control mode is used.
    for(int k=0;k<m;++k){
        int pivot=k;auto largest=norm(a[k*m+k]);
        for(int i=k+1;i<m;++i){auto candidate=norm(a[i*m+k]);if(bf_cmp_lt(&largest.value,&candidate.value)){largest=candidate;pivot=i;}}
        if(bf_is_zero(&largest.value)||bf_is_nan(&largest.value))return false;
        if(pivot!=k){for(int j=k;j<m;++j){const auto saved=a[k*m+j];a[k*m+j]=a[pivot*m+j];a[pivot*m+j]=saved;}for(int c=0;c<columns;++c){const auto saved=rhs[c*m+k];rhs[c*m+k]=rhs[c*m+pivot];rhs[c*m+pivot]=saved;}}
        for(int i=k+1;i<m;++i){const auto factor=a[i*m+k]/a[k*m+k];for(int j=k+1;j<m;++j)a[i*m+j]=a[i*m+j]-factor*a[k*m+j];for(int c=0;c<columns;++c)rhs[c*m+i]=rhs[c*m+i]-factor*rhs[c*m+k];}
    }
    for(int c=0;c<columns;++c)for(int i=m-1;i>=0;--i){auto value=rhs[c*m+i];for(int j=i+1;j<m;++j)value=value-a[i*m+j]*solution[c*m+j];solution[c*m+i]=value/a[i*m+i];}
    return true;
}

}
namespace reasitic {
bool extended_port_network(const double* cap,const double* impedance,int n,
                           int cap_stride,int ind_stride,double* ports,
                           double* currents,double* voltages){
    if(n<1||n>1024||!ports||!currents||!voltages||!finite(cap,n,cap_stride)||!finite(impedance,n,ind_stride))return false;
    Context c;Values a(c,n*n),rhs(c,2*n),solution(c,2*n),hc(c,n*n),zhc(c,n*n),states(c,2*(n+1)),port(c,4);
    if(!a.data||!rhs.data||!solution.data||!hc.data||!zhc.data||!states.data||!port.data)return false;
    double largest=0;
    for(int j=0;j<n;++j)for(int i=0;i<2*n;++i){const double v=__builtin_fabs(impedance[2*j*ind_stride+i]);if(v>largest)largest=v;}
    if(largest==0)return false;
    const Number scale(c,largest);
    const auto C=[&](int i,int j){return Complex(c,cap+2*(i+j*cap_stride));};
    const auto Z=[&](int i,int j){return Complex(c,impedance+2*(i+j*ind_stride));};
    const double half_value[]={.5,0},one_value[]={1,0};const Complex half(c,half_value),one(c,one_value),zero(c);
    const Complex impedance_scale(scale,Number(c));
    // Conservation: I=1*I0-T*C*B*V. Voltage drops: D*V=Z*B*I.
    // Eliminate currents, fix both endpoint voltages, and solve n equations
    // for n-1 interior voltages and J0=scale*I0. H=B*T is triangular.
    // Scaling the current unknown removes the arbitrary voltage/current unit
    // ratio from pivot selection; no inverse of Z is needed.
    for(int j=0;j<n;++j){Complex prefix(c);for(int i=0;i<n;++i){const auto cell=C(i,j);hc[i*n+j]=prefix+half*cell;prefix=prefix+cell;}}
    for(int i=0;i<n;++i){
        Complex row_sum(c);
        for(int j=0;j<n;++j){for(int k=0;k<n;++k)zhc[i*n+j]=zhc[i*n+j]+Z(i,k)*hc[k*n+j];row_sum=row_sum+Z(i,j)/impedance_scale;}
        for(int j=1;j<n;++j){auto cell=half*(zhc[i*n+j-1]+zhc[i*n+j]);if(i==j)cell=cell+one;if(i+1==j)cell=cell-one;a[i*n+j-1]=cell;}
        a[i*n+n-1]=zero-row_sum;
        rhs[i]=zero-half*zhc[i*n];if(i==0)rhs[i]=rhs[i]-one;
        rhs[n+i]=zero-half*zhc[i*n+n-1];if(i==n-1)rhs[n+i]=rhs[n+i]+one;
    }
    if(!solve(a,rhs,solution.data,n,2))return false;
    for(int drive=0;drive<2;++drive){
        const auto V=[&](int vertex){return vertex==0?(drive==0?one:zero):vertex==n?(drive==1?one:zero):solution[drive*n+vertex-1];};
        const auto near=solution[drive*n+n-1]/impedance_scale;auto current=near;
        if(drive==0){states[0]=current;states[n+1]=one;}
        for(int i=0;i<n;++i){
            for(int j=0;j<n;++j)current=current-C(i,j)*(half*(V(j)+V(j+1)));
            if(drive==0){states[i+1]=current;states[n+2+i]=V(i+1);}
        }
        // Native port order: far endpoint first, near endpoint second.
        port[drive==0?2:0]=zero-current;port[drive==0?3:1]=near;
    }
    // Validate all outputs before publishing any of them.
    for(int i=0;i<states.count;++i){const double r=states[i].real.rounded(),v=states[i].imag.rounded();if(!__builtin_isfinite(r)||!__builtin_isfinite(v))return false;}
    for(int i=0;i<4;++i){const double r=port[i].real.rounded(),v=port[i].imag.rounded();if(!__builtin_isfinite(r)||!__builtin_isfinite(v))return false;}
    if(c.status&(BF_ST_MEM_ERROR|BF_ST_INVALID_OP|BF_ST_DIVIDE_ZERO|BF_ST_OVERFLOW))return false;
    for(int i=0;i<4;++i){ports[2*i]=port[i].real.rounded();ports[2*i+1]=port[i].imag.rounded();}
    for(int i=0;i<=n;++i){currents[2*i]=states[i].real.rounded();currents[2*i+1]=states[i].imag.rounded();voltages[2*i]=states[n+1+i].real.rounded();voltages[2*i+1]=states[n+1+i].imag.rounded();}
    return true;
}
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
    if(!solve(a,rhs,delta.data+1,m))return false;
    auto near=C(0,0);Complex far(c);
    for(int j=1;j<n;++j)near=near-C(0,j)*delta[j];
    for(int j=0;j<n;++j){const auto drop=delta[j]-delta[j+1];near=near+Y(0,j)*drop;far=far-Y(n-1,j)*drop;}
    // Row sums of the two-pad admittance are its two raw self-capacitances;
    // the mutual term cancels analytically before numerical subtraction.
    if(pads){near=near-Complex(c,pads);far=far-Complex(c,pads+2*(1+pad_stride));}
    return store(c,far,near,out);
}
bool extended_common_mode_shunts(const double* cap,const double* impedance,int n,
                                int cap_stride,int ind_stride,PiShunts& out){
    if(n<1||n>1024||!finite(cap,n,cap_stride)||!finite(impedance,n,ind_stride))return false;
    Context c;
    if(n==1){
        // One midpoint capacitor splits equally between the endpoints. Cancel
        // the branch impedance symbolically, preserving even a real component
        // hundreds of bits smaller than the imaginary one.
        const Complex z(c,impedance);if(bf_is_zero(&z.real.value)&&bf_is_zero(&z.imag.value))return false;
        const double half_value[]={.5,0};const auto shunt=Complex(c,cap)*Complex(c,half_value);
        return store(c,shunt,shunt,out);
    }
    Values a(c,n*n),rhs(c,n),solution(c,n),hc(c,n*n),zhc(c,n*n);
    if(!a.data||!rhs.data||!solution.data||!hc.data||!zhc.data)return false;
    const auto C=[&](int i,int j){return Complex(c,cap+2*(i+j*cap_stride));};
    const auto Z=[&](int i,int j){return Complex(c,impedance+2*(i+j*ind_stride));};
    const double half_value[]={.5,0},one_value[]={1,0};const Complex half(c,half_value),one(c,one_value),zero(c);
    // I = 1*I0 - T*C*B*(1+delta), with B the adjacent-node average and T
    // cumulative integration. H=B*T has ones below its diagonal and 1/2
    // on it. Eliminate currents and the two zero endpoint deltas first:
    // (D + Z*H*C*B)*delta - Z*1*I0 = -Z*H*C*1.
    for(int j=0;j<n;++j){Complex prefix(c);for(int i=0;i<n;++i){const auto cell=C(i,j);hc[i*n+j]=prefix+half*cell;prefix=prefix+cell;}}
    for(int i=0;i<n;++i){
        Complex row_sum(c);
        for(int j=0;j<n;++j){
            auto& cell=zhc[i*n+j];for(int k=0;k<n;++k)cell=cell+Z(i,k)*hc[k*n+j];
            rhs[i]=rhs[i]-cell;row_sum=row_sum+Z(i,j);
        }
        for(int j=1;j<n;++j){
            auto cell=half*(zhc[i*n+j-1]+zhc[i*n+j]);
            if(i==j)cell=cell+one;if(i+1==j)cell=cell-one;
            a[i*n+j-1]=cell;
        }
        a[i*n+n-1]=zero-row_sum;
    }
    if(!solve(a,rhs,solution.data,n))return false;
    const auto delta=[&](int vertex){return vertex>0&&vertex<n?solution[vertex-1]:zero;};
    const auto near=solution[n-1];auto far=zero-near;
    for(int j=0;j<n;++j){const auto midpoint=one+half*(delta(j)+delta(j+1));for(int i=0;i<n;++i)far=far+C(i,j)*midpoint;}
    return store(c,far,near,out);
}
}
extern "C" const char* probe_pi_shunts(const double* cap,const double* ind,int n,const double* pads,int pad_stride){
    static char output[256];reasitic::PiShunts result;
    if(!reasitic::common_mode_shunts(cap,ind,n,n,n,pads,pad_stride,result))return "{\"error\":\"Common-mode shunt solve failed\"}";
    sprintf(output,"{\"first\":[%.17g,%.17g],\"second\":[%.17g,%.17g],\"precision_bits\":128}",result.first_real,result.first_imag,result.second_real,result.second_imag);return output;
}
extern "C" const char* probe_extended_pi_shunts(const double* cap,const double* impedance,int n){
    static char output[256];reasitic::PiShunts result;
    if(!reasitic::extended_common_mode_shunts(cap,impedance,n,n,n,result))return "{\"error\":\"Extended common-mode shunt solve failed\"}";
    sprintf(output,"{\"first\":[%.17g,%.17g],\"second\":[%.17g,%.17g],\"precision_bits\":128}",result.first_real,result.first_imag,result.second_real,result.second_imag);return output;
}
