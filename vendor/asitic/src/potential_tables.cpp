#include "asitic/potential_tables.hpp"
#include "x87.hpp"
#include "asitic/legacy_complex.hpp"

namespace asitic {
long double potential_clipped_cosh(double x) {
    if(x>500.)return 1e15;
    const long double exponential=x87::exp(x);
    return x87::binary64(0.5L*(1.L/exponential+exponential));
}
void potential_fft(ComplexValue* a,int n) {
    for(int i=0,j=0;i<n-1;++i) {
        if(i<j) { const ComplexValue t=a[j];a[j]=a[i];a[i]=t; }
        int k=n>>1;
        for(;k>1 && j>=k;k>>=1)j-=k;
        j+=k;
    }
    for(int half=1;half<n;) {
        const int step=half*2;
        constexpr double two_pi=0x1.921fb54442d1cp+2;
        const long double angle=static_cast<long double>(two_pi)/step;
        const long double sine=x87::sin(0.5L*angle);
        const long double wpr=(sine*-2.L)*sine;
        const double wpi=x87::binary64(x87::sin(angle));
        long double wr=1,wi=0;
        for(int m=0;m<half;++m) {
            for(int i=m;i<n;i+=step) {
                const auto other=a[i+half];
                const long double tr=wr*other.real-wi*other.imag;
                const long double ti=wr*other.imag+wi*other.real;
                a[i+half]={x87::binary64(a[i].real-tr),x87::binary64(a[i].imag-ti)};
                a[i]={x87::binary64(tr+a[i].real),x87::binary64(ti+a[i].imag)};
            }
            const long double old_wr=wr;
            wr=(wr*wpr-wi*wpi)+wr;
            wi=wi*wpr+old_wr*wpi+wi;
        }
        half=step;
    }
}
void transform_potential_grid(ComplexValue* grid,int nx,int ny,ComplexValue* scratch) {
    const int stride=ny+1;
    for(int x=0;x<=nx;++x) {
        for(int y=0;y<=ny;++y)scratch[y]=grid[x*stride+y];
        for(int y=ny+1;y<2*ny;++y)scratch[y]=grid[x*stride+2*ny-y];
        potential_fft(scratch,2*ny);
        for(int y=0;y<=ny;++y)grid[x*stride+y]=scratch[y];
    }
    for(int y=0;y<=ny;++y) {
        for(int x=0;x<=nx;++x)scratch[x]=grid[x*stride+y];
        for(int x=nx+1;x<2*nx;++x)scratch[x]=grid[(2*nx-x)*stride+y];
        potential_fft(scratch,2*nx);
        for(int x=0;x<=nx;++x)grid[x*stride+y]=scratch[x];
    }
}
void transform_potential_grid_allocated(ComplexValue* grid,int nx,int ny,SolverMemory memory) {
    auto* scratch=static_cast<ComplexValue*>(memory.arrays.get(static_cast<unsigned>(nx)*32u));
    for(int i=0;i<2*nx;++i)scratch[i]={0,0};
    if(!scratch)lapack::application_fatal_error("Unable to allocate enough memory for FFT.");
    transform_potential_grid(grid,nx,ny,scratch);
    memory.put(scratch);
}

namespace {
using x87::binary64;
double potential_tanh(long double x) {
    const long double e=-x87::abs(x+x)*x87::log2e(),integer=x87::round(e);
    const long double part=x87::scale(x87::pow2m1(e-integer),integer);
    const long double whole=x87::scale(1.L,integer);
    const long double em1=(whole-1.L)+part;
    const long double fraction=em1/(em1+2.L),sign=(-x>=0)?1.L:-1.L;
    long double value;
    asm("fmulp" : "=t"(value) : "0"(fraction),"u"(sign) : "st(1)");
    return binary64(value);
}
struct Transfer { ComplexValue u,v; };
Transfer potential_transfer(bool down,int first,int last,double k,const PotentialLayers& layers) {
    const auto* z=layers.depths;
    const double initial=potential_tanh(static_cast<long double>(k)*(down?
        static_cast<long double>(z[last+1])-z[last]:static_cast<long double>(z[0])-z[1]));
    Transfer state=down?Transfer{{1,0},{initial,0}}:Transfer{{initial,0},{1,0}};
    for(int i=down?last-1:1;down?i>=first:i<=last;down?--i:++i) {
        const long double argument=static_cast<long double>(k)*(down?
            static_cast<long double>(z[i+1])-z[i]:static_cast<long double>(z[i])-z[i+1]);
        const auto q=legacy::divide(layers.coefficients[i],layers.coefficients[down?i+1:i-1]);
        const double t=potential_tanh(argument);
        const long double qr=q.real,qi=q.imag,ur=state.u.real,ui=state.u.imag,vr=state.v.real,vi=state.v.imag;
        const long double pr=qr*vr-qi*vi,pi=qr*vi+qi*vr;
        const ComplexValue u{binary64(ur+(qr*t*vr-qi*t*vi)),binary64(qi*t*vr+qr*t*vi+ui)};
        const ComplexValue v{binary64(static_cast<long double>(t)*ur+pr),binary64(static_cast<long double>(t)*ui+pi)};
        state={u,v};
    }
    return state;
}
}
ComplexValue potential_transfer_up(int last,double k,const PotentialLayers& layers) {
    const auto state=potential_transfer(false,0,last,k,layers);
    return legacy::divide(state.u,state.v);
}
ComplexValue potential_transfer_down(int first,int last,double k,const PotentialLayers& layers) {
    const auto state=potential_transfer(true,first,last,k,layers);
    return legacy::divide(state.u,state.v);
}
ComplexValue potential_source_up(int source,double k,double height,const PotentialTableLayers& layers) {
    const double saved=binary64(layers.depths[source+1]);layers.depths[source+1]=binary64(height);
    const auto state=potential_transfer(false,0,source,k,layers.view());
    layers.depths[source+1]=saved;return state.u;
}
ComplexValue potential_source_down(int source,int last,double k,double height,const PotentialTableLayers& layers) {
    const double saved=binary64(layers.depths[source]);layers.depths[source]=binary64(height);
    const auto state=potential_transfer(true,source,last,k,layers.view());
    layers.depths[source]=saved;return state.u;
}
namespace {
struct SpectralExtended { long double real,imag; };
SpectralExtended ext(ComplexValue z) { return {z.real,z.imag}; }
double spectral_cosh(long double x,bool rounded=false) {
    if(x>500.L)return 1e15;
    if(rounded)x=binary64(x);
    const long double e=x87::exp(x);
    return binary64(0.5L*(1.L/e+e));
}
ComplexValue spectral_combine(double s,ComplexValue ratio,ComplexValue upper,ComplexValue lower,
    ComplexValue top,ComplexValue bottom,double ta,double tb,double tc,bool down) {
    const auto r=ext(ratio),u=ext(upper),d=ext(lower),t=ext(top),b=ext(bottom);
    const long double pr=binary64(binary64(u.real*b.real)-u.imag*b.imag);
    const long double pi=u.imag*b.real+binary64(u.real*b.imag);
    const long double qr=d.real*t.real-d.imag*t.imag,qi=d.real*t.imag+d.imag*t.real;
    const long double cr=b.real*t.real-b.imag*t.imag,ci=t.imag*b.real+b.imag*t.real;
    const long double sr=binary64(binary64(static_cast<long double>(s)*r.real)-0.L*r.imag);
    const long double si=binary64(static_cast<long double>(s)*r.imag+r.real*0.L);
    const double up_weight=down?tb:ta,down_weight=down?ta:tb;
    const long double real_sum=((pr*up_weight+qr*down_weight)+static_cast<long double>(binary64(cr*tb))*ta)+(d.real*u.real-d.imag*u.imag);
    const long double imag_sum=((pi*up_weight+qi*down_weight)+(ci*tb)*ta)+(d.imag*u.real+binary64(d.real*u.imag));
    const ComplexValue numerator{binary64(binary64(sr*real_sum)-si*imag_sum),binary64(imag_sum*sr+real_sum*si)};
    const ComplexValue denominator{binary64(((qr-pr)+cr*tc)-((tc*d.real)*u.real-(tc*d.imag)*u.imag)),
        binary64(((qi-binary64(pi))+static_cast<long double>(binary64(ci))*tc)-((tc*d.imag)*u.real+(tc*d.real)*u.imag))};
    return legacy::divide(numerator,denominator);
}
ComplexValue spectral_kernel(bool down,int source,int target,int last,double k,double source_height,double target_height,
    const PotentialTableLayers& layers) {
    const auto* z=layers.depths;const auto* c=layers.coefficients;
    const double ta=potential_tanh(static_cast<long double>(k)*(down?
        static_cast<long double>(z[target])-target_height:static_cast<long double>(z[target+1])-target_height));
    const double tb=potential_tanh(static_cast<long double>(k)*(down?
        static_cast<long double>(z[target+1])-source_height:static_cast<long double>(z[target])-source_height));
    const double tc=potential_tanh(static_cast<long double>(k)*(static_cast<long double>(z[target+1])-z[target]));
    double scale=1;
    for(int i=down?target:source;i<=(down?source:target);++i) {
        const double a=spectral_cosh(static_cast<long double>(k)*(static_cast<long double>(z[0])-z[i+1]));
        const double b=spectral_cosh(static_cast<long double>(k)*(static_cast<long double>(z[0])-z[i]));
        const double d=spectral_cosh(static_cast<long double>(k)*(static_cast<long double>(z[i+1])-z[i]),true);
        scale=binary64((static_cast<long double>(a)/(static_cast<long double>(b)*d))*scale);
    }
    ComplexValue top{0,0},bottom{0,0},lower{1,0};
    if(target==last)top=legacy::divide(c[target],c[target-1]);
    else {
        if(target!=0)top=legacy::divide(c[target],c[target-1]);
        bottom=legacy::divide(c[target],c[target+1]);
        lower=potential_transfer_down(target+1,last,k,layers.view());
    }
    const auto upper=potential_transfer_up(target-1,k,layers.view());
    const auto from_source=down?potential_source_down(source,last,k,source_height,layers):potential_source_up(source,k,source_height,layers);
    const auto from_target=down?potential_source_down(target,last,k,source_height,layers):potential_source_up(target,k,source_height,layers);
    const auto ratio=legacy::divide(from_source,from_target);
    auto value=spectral_combine(scale,ratio,upper,lower,top,bottom,ta,tb,tc,down);
    const double first=spectral_cosh(static_cast<long double>(k)*(down?
        static_cast<long double>(z[target])-target_height:static_cast<long double>(z[source])-source_height));
    value={binary64(static_cast<long double>(value.real)*first-static_cast<long double>(value.imag)*0.L),
           binary64(static_cast<long double>(value.imag)*first+static_cast<long double>(value.real)*0.L)};
    const double second=spectral_cosh(static_cast<long double>(k)*(static_cast<long double>(z[0])-z[down?target:source]));
    const double third=spectral_cosh(static_cast<long double>(k)*(static_cast<long double>(z[0])-z[(down?source:target)+1]));
    const long double weight=static_cast<long double>(second)/third;
    value={binary64(static_cast<long double>(value.real)*weight-static_cast<long double>(value.imag)*0.L),
           binary64(static_cast<long double>(value.imag)*weight+static_cast<long double>(value.real)*0.L)};
    const double final=spectral_cosh(static_cast<long double>(k)*(static_cast<long double>(z[(down?source:target)+1])-(down?source_height:target_height)));
    const long double real=static_cast<long double>(value.real)*final-static_cast<long double>(value.imag)*0.L;
    const long double imag=0.L*value.real+static_cast<long double>(value.imag)*final;
    const ComplexValue weighted{binary64(real*c[target].real-imag*c[target].imag),
        binary64(static_cast<long double>(c[target].imag)*binary64(real)+static_cast<long double>(c[target].real)*imag)};
    return legacy::divide(weighted,{k,0});
}
}
ComplexValue potential_spectral_a(int source,int target,int last,double k,double source_height,double target_height,const PotentialTableLayers& layers) {
    return spectral_kernel(true,source,target,last,k,source_height,target_height,layers);
}
ComplexValue potential_spectral_b(int source,int target,int last,double k,double source_height,double target_height,const PotentialTableLayers& layers) {
    return spectral_kernel(false,source,target,last,k,source_height,target_height,layers);
}
void generate_potential_tables(int source,int target,const PotentialTableGeometry& g,PotentialTableOutput out,SolverMemory memory,WarningSink status) {
    constexpr double pi=0x1.921fb54442d18p+1;
    const auto sample=[&](double k,bool y_axis) {
        const auto& sm=g.metals[source];const auto& tm=g.metals[target];
        const int sl=*reinterpret_cast<const int*>(sm.prefix+0xa0),tl=*reinterpret_cast<const int*>(tm.prefix+0xa0);
        double sh=*reinterpret_cast<const double*>(sm.prefix+0xa8),th=*reinterpret_cast<const double*>(tm.prefix+0xa8);
        const bool a=sh<th;
        if(source!=target) {
            // The Y-axis code applies the opposite half-thickness offsets
            // from the combined-grid and X-axis paths in the original.
            if(a!=y_axis) { sh=binary64(sh+0.5L*sm.thickness);th=binary64(th-0.5L*tm.thickness); }
            else { sh=binary64(sh-0.5L*sm.thickness);th=binary64(th+0.5L*tm.thickness); }
        }
        return a?potential_spectral_a(sl,tl,g.layer_count-1,k,sh,th,g.layers):
                 potential_spectral_b(sl,tl,g.layer_count-1,k,sh,th,g.layers);
    };
    status.emit("Calculating Green Function...");
    for(int x=0;x<=g.nx;++x)for(int y=0;y<=g.ny;++y) {
        auto& z=out.xy[x*(g.nx+1)+y];
        if(x==0 || y==0 || x==g.nx || y==g.ny) { z={0,0};continue; }
        const long double kx=(static_cast<long double>(x)*pi)/g.width,ky=(static_cast<long double>(y)*pi)/g.height;
        z=sample(binary64(x87::sqrt(kx*kx+ky*ky)),false);
        const ComplexValue dx{static_cast<double>(x),0},dy{static_cast<double>(y),0};
        z=legacy::divide(z,dx);z=legacy::divide(z,dx);z=legacy::divide(z,dy);z=legacy::divide(z,dy);
    }
    status.emit("Computing FFTs...            ");
    transform_potential_grid_allocated(out.xy,g.nx,g.ny,memory);
    for(int x=0;x<=g.nx;++x) {
        auto& z=out.x[x];if(x==0 || x==g.nx) { z={0,0};continue; }
        z=sample(binary64((static_cast<long double>(x)*pi)/g.width),false);
        const ComplexValue divisor{static_cast<double>(x),0};
        z=legacy::divide(z,divisor);z=legacy::divide(z,divisor);
    }
    for(int y=0;y<=g.ny;++y) {
        auto& z=out.y[y];if(y==0 || y==g.ny) { z={0,0};continue; }
        z=sample(binary64((static_cast<long double>(y)*pi)/g.height),true);
        const ComplexValue divisor{static_cast<double>(y),0};
        z=legacy::divide(z,divisor);z=legacy::divide(z,divisor);
    }
    for(int x=g.nx;x<2*g.nx;++x)out.x[x]=out.x[2*g.nx-x];
    for(int y=g.ny;y<2*g.ny;++y)out.y[y]=out.y[2*g.ny-y];
    potential_fft(out.x,2*g.nx);potential_fft(out.y,2*g.ny);
}
}
