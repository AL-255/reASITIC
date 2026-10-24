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
Number exact_sum(const Number& a,const Number& b,bool subtract=false){Number r(*a.context);a.context->status|=subtract?bf_sub(&r.value,&a.value,&b.value,BF_PREC_INF,BF_RNDN):bf_add(&r.value,&a.value,&b.value,BF_PREC_INF,BF_RNDN);return r;}
Complex exact_sum(const Complex& a,const Complex& b,bool subtract=false){return {exact_sum(a.real,b.real,subtract),exact_sum(a.imag,b.imag,subtract)};}
Number exact_product(const Number& a,const Number& b){Number r(*a.context);a.context->status|=bf_mul(&r.value,&a.value,&b.value,BF_PREC_INF,BF_RNDN);return r;}
Complex exact_product(const Complex& a,const Complex& b){return {exact_sum(exact_product(a.real,b.real),exact_product(a.imag,b.imag),true),exact_sum(exact_product(a.real,b.imag),exact_product(a.imag,b.real))};}
Complex guarded_quotient(const Complex& a,const Complex& b){
    const auto denominator=exact_sum(exact_product(b.real,b.real),exact_product(b.imag,b.imag));
    return {exact_sum(exact_product(a.real,b.real),exact_product(a.imag,b.imag))/denominator,
            exact_sum(exact_product(a.imag,b.real),exact_product(a.real,b.imag),true)/denominator};
}
bool binary64_quotient(const Complex& numerator,const Complex& denominator,double* output){
    auto& c=*numerator.real.context;
    const auto norm=exact_sum(exact_product(denominator.real,denominator.real),exact_product(denominator.imag,denominator.imag));
    const Number components[]={exact_sum(exact_product(numerator.real,denominator.real),exact_product(numerator.imag,denominator.imag)),
                               exact_sum(exact_product(numerator.imag,denominator.real),exact_product(numerator.real,denominator.imag),true)};
    for(int i=0;i<2;++i){Number quotient(c);c.status|=bf_div(&quotient.value,&components[i].value,&norm.value,53,BF_RNDN|BF_FLAG_SUBNORMAL|bf_set_exp_bits(11));output[i]=quotient.rounded();}
    return !(c.status&BF_ST_MEM_ERROR);
}
Complex shift(const Complex& a,slimb_t exponent){Complex r(a);a.real.context->status|=bf_mul_2exp(&r.real.value,exponent,BF_PREC_INF,BF_RNDN);a.imag.context->status|=bf_mul_2exp(&r.imag.value,exponent,BF_PREC_INF,BF_RNDN);return r;}
Complex negate(const Complex& a){Complex r(a);bf_neg(&r.real.value);bf_neg(&r.imag.value);return r;}
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
bool three_port_conversion(const double* impedance,bool scattering,double* output){
    if(!impedance||!output)return false;
    for(int i=0;i<18;++i)if(!__builtin_isfinite(impedance[i]))return false;
    Context c;Values a(c,9),adjugate(c,9);if(!a.data||!adjugate.data)return false;
    const double unity[]={1,0},reference[]={0x1.47ae147ae147bp-6,0},twice[]={2,0};
    const Complex one(c,unity),g(c,reference),two(c,twice);
    for(int i=0;i<3;++i)for(int j=0;j<3;++j){
        const Complex z(c,impedance+2*(3*i+j));a[3*i+j]=scattering?exact_product(z,g):z;
        if(scattering&&i==j)a[3*i+j]=exact_sum(a[3*i+j],one);
    }
    for(int i=0;i<3;++i)for(int j=0;j<3;++j){
        int rows[2],columns[2],r=0,s=0;for(int k=0;k<3;++k){if(k!=j)rows[r++]=k;if(k!=i)columns[s++]=k;}
        auto cofactor=exact_sum(exact_product(a[3*rows[0]+columns[0]],a[3*rows[1]+columns[1]]),exact_product(a[3*rows[0]+columns[1]],a[3*rows[1]+columns[0]]),true);
        adjugate[3*i+j]=(i+j)%2?negate(cofactor):cofactor;
    }
    Complex determinant(c);for(int j=0;j<3;++j)determinant=exact_sum(determinant,exact_product(a[j],adjugate[3*j]));
    if(c.status&BF_ST_MEM_ERROR)return false;
    const int order[]={1,0,2};
    for(int i=0;i<3;++i)for(int j=0;j<3;++j){
        auto numerator=adjugate[3*order[i]+order[j]];
        if(scattering){
            // (A-2I)*A^-1 = I-2*A^-1. Keep this subtraction in the
            // numerator, before division, so a small reflected component
            // is not lost by subtracting rounded inverse entries from I.
            numerator=exact_sum(i==j?determinant:Complex(c),exact_product(two,numerator),true);
        }
        if(!binary64_quotient(numerator,determinant,output+2*(3*i+j)))return false;
    }
    return true;
}
bool two_port_scattering(const double* admittance,double* scattering){
    if(!admittance||!scattering)return false;
    for(int i=0;i<8;++i)if(!__builtin_isfinite(admittance[i]))return false;
    Context c;const double reference[]={0x1.47ae147ae147bp-6,0},two_value[]={-2,0};
    const Complex g(c,reference),two(c,two_value),a(c,admittance+6),b(c,admittance),cross=exact_product(Complex(c,admittance+2),Complex(c,admittance+4));
    const auto plus=exact_sum(a,g),denominator=exact_sum(exact_product(plus,exact_sum(b,g)),cross,true);
    // Keep ASITIC's historical first diagonal: (g-a)*(g+a)+y21*y12.
    // Every sum and product remains exact, so the small reference conductance
    // survives even when the large branch-admittance products cancel.
    const Complex numerators[]={exact_sum(exact_product(exact_sum(g,a,true),plus),cross),
        exact_product(exact_product(two,g),Complex(c,admittance+2)),
        exact_product(exact_product(two,g),Complex(c,admittance+4)),
        exact_sum(exact_product(plus,exact_sum(g,b,true)),cross)};
    if(c.status&BF_ST_MEM_ERROR)return false;
    for(int i=0;i<4;++i)if(!binary64_quotient(numerators[i],denominator,scattering+2*i))return false;
    return true;
}
bool loaded_port_impedance(const double* admittance,const double* load,int port,
                           double frequency,double first_cap,double second_cap,
                           double* impedance,int& status){
    if(!admittance||!load||!impedance||(port!=1&&port!=2)||!__builtin_isfinite(frequency)||frequency<=0||!__builtin_isfinite(first_cap)||!__builtin_isfinite(second_cap))return false;
    for(int i=0;i<8;++i)if(!__builtin_isfinite(admittance[i]))return false;
    for(int i=0;i<2;++i)if(!__builtin_isfinite(load[i]))return false;
    Context c;const double unity[]={1,0};const Complex one(c,unity),z(c,load);
    Complex first(c,admittance+6),second(c,admittance);
    const auto angular=exact_product(Number(c,frequency),Number(c,0x1.921fb54442d18p+2));
    first.imag=exact_sum(first.imag,exact_product(angular,Number(c,first_cap)));
    second.imag=exact_sum(second.imag,exact_product(angular,Number(c,second_cap)));
    const auto determinant=exact_sum(exact_product(first,second),exact_product(Complex(c,admittance+2),Complex(c,admittance+4)),true);
    // Eliminate the un-driven port, then multiply through by Zload. This
    // avoids 1/Zload and includes a zero-ohm termination as a circuit limit.
    const auto numerator=exact_sum(one,exact_product(port==1?second:first,z));
    const auto denominator=exact_sum(port==1?first:second,exact_product(determinant,z));
    if(c.status&BF_ST_MEM_ERROR)return false;
    if(bf_is_zero(&denominator.real.value)&&bf_is_zero(&denominator.imag.value)){
        status=bf_is_zero(&numerator.real.value)&&bf_is_zero(&numerator.imag.value)?2:1;
        impedance[0]=impedance[1]=__builtin_nan("");return true;
    }
    // Round the exact rational components directly to binary64, including
    // its subnormal exponent range. There is no intermediate rounded Y bank
    // or host floating-point operation in the loaded circuit reduction.
    if(!binary64_quotient(numerator,denominator,impedance))return false;
    status=__builtin_isfinite(impedance[0])&&__builtin_isfinite(impedance[1])?0:3;return true;
}
bool pi_branch_impedances(const double* series,const PiShunts& shunts,double* impedances,double& differential_quality){
    if(!series||!impedances)return false;
    const double values[]={series[0],series[1],shunts.first_real,shunts.first_imag,shunts.second_real,shunts.second_imag};
    for(double value:values)if(!__builtin_isfinite(value))return false;
    Context c;const Complex s(c,values),first(c,values+2),second(c,values+4);
    const double unity[]={1,0};const Complex one(c,unity);
    const auto grounded_first=guarded_quotient(one,exact_sum(s,second)),grounded_second=guarded_quotient(one,exact_sum(s,first));
    const auto sum=exact_sum(first,second);
    auto numerator=sum,denominator=exact_sum(exact_product(s,sum),exact_product(first,second));
    // With no shunt path, the differential source sees the series branch.
    // Taking this circuit limit avoids the determinant's indeterminate 0/0.
    if(bf_is_zero(&first.real.value)&&bf_is_zero(&first.imag.value)&&bf_is_zero(&second.real.value)&&bf_is_zero(&second.imag.value)){numerator=one;denominator=s;}
    const auto differential=guarded_quotient(numerator,denominator);
    // Q = Im(Z)/Re(Z); the shared |denominator|^2 cancels symbolically.
    // Exact dyadic products retain small components even at resonance.
    const auto q=exact_sum(exact_product(numerator.imag,denominator.real),exact_product(numerator.real,denominator.imag),true)/
                 exact_sum(exact_product(numerator.real,denominator.real),exact_product(numerator.imag,denominator.imag));
    const double result[]={grounded_first.real.rounded(),grounded_first.imag.rounded(),grounded_second.real.rounded(),grounded_second.imag.rounded(),differential.real.rounded(),differential.imag.rounded(),q.rounded()};
    if(c.status&BF_ST_MEM_ERROR)return false;
    for(int i=0;i<6;++i)impedances[i]=result[i];differential_quality=result[6];return true;
}
bool folded_port_network(const double* cap,const double* ind,int n,int cap_stride,int ind_stride,
                         const double* pads,int pad_stride,double* ports,double* node_cap,double* with_pads){
    if(n<1||n>1024||!ports||!finite(cap,n,cap_stride)||!finite(ind,n,ind_stride)||(pads&&!finite(pads,2,pad_stride)))return false;
    Context c;const int width=n+1,m=n-1;
    Values nodal(c,width*width),capacitive(c,n*n),a(c,m*m),rhs(c,2*m),solution(c,2*m),port(c,4),raw(c,4);
    if(!nodal.data||!capacitive.data||!port.data||!raw.data||(m&&(!a.data||!rhs.data||!solution.data)))return false;
    double largest=0;
    for(int j=0;j<n;++j)for(int i=0;i<2*n;++i){const double x=__builtin_fabs(cap[2*j*cap_stride+i]),y=__builtin_fabs(ind[2*j*ind_stride+i]);if(x>largest)largest=x;if(y>largest)largest=y;}
    const Number scale(c,largest?largest:1.);const auto exponent=scale.value.expn-1;
    const Complex zero(c);
    const auto C=[&](int i,int j){return Complex(c,cap+2*(i+j*cap_stride));};
    // Assemble the dyadic input coefficients exactly before factorization.
    // Their finite binary64 exponent range bounds the size of these sums.
    // Every coefficient has admittance units. An exact power-of-two scale
    // makes the endpoint-voltage problem dimensionless without changing the
    // circuit or introducing a second rounding at a unit conversion.
    for(int i=0;i<n;++i){
        Complex diagonal(c);
        for(int j=0;j<n;++j){const auto cell=C(i,j);diagonal=exact_sum(diagonal,cell);if(i!=j)capacitive[i*n+j]=negate(cell);}
        capacitive[i*n+i]=diagonal;
        for(int j=0;j<n;++j)nodal[i*width+j]=shift(capacitive[i*n+j],-exponent);
    }
    // D is the oriented branch/vertex incidence. Stamp the four contributions
    // of each branch-admittance cell directly; never invert the branch matrix.
    for(int i=0;i<n;++i)for(int j=0;j<n;++j){
        const auto value=shift(Complex(c,ind+2*(i+j*ind_stride)),-exponent);
        nodal[i*width+j]=exact_sum(nodal[i*width+j],value);
        nodal[i*width+j+1]=exact_sum(nodal[i*width+j+1],value,true);
        nodal[(i+1)*width+j]=exact_sum(nodal[(i+1)*width+j],value,true);
        nodal[(i+1)*width+j+1]=exact_sum(nodal[(i+1)*width+j+1],value);
    }
    // The far endpoint is native port 1; near is native port 2. Both drives
    // share the same (n-1)-node factorization, including the n=1 direct limit.
    const int endpoints[]={n,0};
    for(int i=0;i<m;++i){
        for(int j=0;j<m;++j)a[i*m+j]=nodal[(i+1)*width+j+1];
        for(int drive=0;drive<2;++drive)rhs[drive*m+i]=zero-nodal[(i+1)*width+endpoints[drive]];
    }
    if(!solve(a,rhs,solution.data,m,2))return false;
    for(int drive=0;drive<2;++drive)for(int row=0;row<2;++row){
        Complex reaction(c);
        for(int j=0;j<m;++j)reaction=reaction+nodal[endpoints[row]*width+j+1]*solution[drive*m+j];
        raw[drive*2+row]=port[drive*2+row]=shift(exact_sum(nodal[endpoints[row]*width+endpoints[drive]],reaction),exponent);
    }
    if(pads){
        const Complex first(c,pads),cross(c,pads+2),second(c,pads+2*(1+pad_stride));
        port[0]=exact_sum(port[0],exact_sum(second,cross),true);port[3]=exact_sum(port[3],exact_sum(first,cross),true);
        port[1]=exact_sum(port[1],cross);port[2]=exact_sum(port[2],cross);
    }
    const auto valid=[](Values& values){for(int i=0;i<values.count;++i)if(!__builtin_isfinite(values[i].real.rounded())||!__builtin_isfinite(values[i].imag.rounded()))return false;return true;};
    if(!valid(port)||(node_cap&&!valid(capacitive))||(with_pads&&!valid(raw))||
       (c.status&(BF_ST_MEM_ERROR|BF_ST_INVALID_OP|BF_ST_DIVIDE_ZERO|BF_ST_OVERFLOW)))return false;
    for(int i=0;i<4;++i){ports[2*i]=port[i].real.rounded();ports[2*i+1]=port[i].imag.rounded();if(with_pads){with_pads[2*i]=raw[i].real.rounded();with_pads[2*i+1]=raw[i].imag.rounded();}}
    if(node_cap)for(int j=0;j<n;++j)for(int i=0;i<n;++i){node_cap[2*(i+j*n)]=capacitive[i*n+j].real.rounded();node_cap[2*(i+j*n)+1]=capacitive[i*n+j].imag.rounded();}
    return true;
}
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

extern "C" const char* loaded_port_impedance(const double* admittance,double load_real,double load_imag,int port,double frequency,double first_cap,double second_cap){
    static char output[256];const double load[]={load_real,load_imag};double result[2];int status;
    if(!reasitic::loaded_port_impedance(admittance,load,port,frequency,first_cap,second_cap,result,status))return "{\"error\":\"Use a finite 2x2 admittance, complex load, capacitances, positive frequency and port 1 or 2\"}";
    const char* names[]={"finite","pole","indeterminate","overflow"};
    char* out=output;out+=sprintf(out,"{\"status\":\"%s\",\"impedance_ohm\":[",names[status]);
    for(int i=0;i<2;++i){if(i)out+=sprintf(out,",");out+=__builtin_isfinite(result[i])?sprintf(out,"%.17g",result[i]):sprintf(out,"null");}sprintf(out,"]}");return output;
}

extern "C" const char* probe_scattering(const double* admittance){
    static char output[1024];double values[8];
    if(!reasitic::two_port_scattering(admittance,values))return "{\"error\":\"Use a finite 2x2 admittance\"}";
    char* out=output;out+=sprintf(out,"{\"scattering_matrix\":[");
    for(int i=0;i<2;++i){out+=sprintf(out,"%s[",i?",":"");for(int j=0;j<2;++j){out+=sprintf(out,"%s[",j?",":"");for(int k=0;k<2;++k){const double value=values[4*i+2*j+k];if(k)out+=sprintf(out,",");out+=__builtin_isfinite(value)?sprintf(out,"%.17g",value):sprintf(out,"null");}out+=sprintf(out,"]");}out+=sprintf(out,"]");}sprintf(out,"]}");return output;
}

extern "C" const char* probe_three_conversion(const double* impedance,int scattering){
    static char output[2048];double values[18];
    if((scattering!=0&&scattering!=1)||!reasitic::three_port_conversion(impedance,scattering!=0,values))return "{\"error\":\"Use a finite 3x3 impedance and conversion mode 0 or 1\"}";
    char* out=output;out+=sprintf(out,"{\"matrix\":[");
    for(int i=0;i<3;++i){out+=sprintf(out,"%s[",i?",":"");for(int j=0;j<3;++j){out+=sprintf(out,"%s[",j?",":"");for(int k=0;k<2;++k){const double value=values[2*(3*i+j)+k];if(k)out+=sprintf(out,",");out+=__builtin_isfinite(value)?sprintf(out,"%.17g",value):sprintf(out,"null");}out+=sprintf(out,"]");}out+=sprintf(out,"]");}sprintf(out,"]}");return output;
}
