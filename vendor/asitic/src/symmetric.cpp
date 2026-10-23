#include "asitic/lapack.hpp"
#include "x87.hpp"

namespace asitic::lapack {
namespace {
using x87::binary64;
struct Extended { long double real,imag; };
Extended multiply(Extended a,Extended b) {
    return {a.real*b.real-a.imag*b.imag,a.imag*b.real+a.real*b.imag};
}
Extended extend(ComplexValue a) { return {a.real,a.imag}; }
ComplexValue spill(Extended a) { return {binary64(a.real),binary64(a.imag)}; }
ComplexValue negate(ComplexValue a) { return {-a.real,-a.imag}; }
void exchange(ComplexValue& a,ComplexValue& b) { const auto t=a;a=b;b=t; }
}
int max_index(int n,const ComplexValue* x,int inc) {
    if(n<1 || inc<1)return 0;
    int best=1;
    long double m=abs1(x[0]);
    for(int i=1;i<n;++i) {
        const long double v=abs1(x[i*inc]);
        // IZAMAX's x87 status mask also selects the new element when either
        // norm is unordered. An all-NaN column therefore selects its last row.
        if(!(v<=m)) { best=i+1;m=v; }
    }
    return best;
}
void axpy(int n,const ComplexValue& alpha,const ComplexValue* x,int incx,ComplexValue* y,int incy) {
    if(n<=0 || abs1(alpha)==0)return;
    int ix=incx<0?(1-n)*incx:0,iy=incy<0?(1-n)*incy:0;
    for(int i=0;i<n;++i,ix+=incx,iy+=incy) {
        const auto t=multiply(extend(alpha),extend(x[ix]));
        const auto old=y[iy];
        y[iy]={binary64(t.real+old.real),binary64(t.imag+old.imag)};
    }
}
void scale(int n,const ComplexValue& alpha,ComplexValue* x,int inc) {
    if(n<=0 || inc<=0)return;
    for(int i=0;i<n;++i)x[i*inc]=spill(multiply(extend(alpha),extend(x[i*inc])));
}
void swap(int n,ComplexValue* x,int incx,ComplexValue* y,int incy) {
    int ix=incx<0?(1-n)*incx:0,iy=incy<0?(1-n)*incy:0;
    for(int i=0;i<n;++i,ix+=incx,iy+=incy)exchange(x[ix],y[iy]);
}
void rotate(int n,ComplexValue* x,int incx,ComplexValue* y,int incy,
            const ComplexValue& c,const ComplexValue& s) {
    int ix=incx<0?(1-n)*incx:0,iy=incy<0?(1-n)*incy:0;
    for(int i=0;i<n;++i,ix+=incx,iy+=incy) {
        const auto cx=multiply(extend(c),extend(x[ix]));
        const auto sy=multiply(extend(s),extend(y[iy]));
        const auto cy=multiply(extend(c),extend(y[iy]));
        const auto sx=multiply(extend(s),extend(x[ix]));
        const ComplexValue temp{binary64(cx.real+sy.real),binary64(cx.imag+sy.imag)};
        y[iy]={binary64(cy.real-sx.real),binary64(cy.imag-sx.imag)};
        x[ix]=temp;
    }
}
void rank1_upper(int n,const ComplexValue& alpha,const ComplexValue* x,int inc,ComplexMatrixView a) {
    if(n==0 || (alpha.real==0 && alpha.imag==0))return;
    const int start=inc<0?(1-n)*inc:0;
    for(int j=0;j<n;++j) {
        const auto xj=x[start+j*inc];
        if(xj.real==0 && xj.imag==0)continue;
        const auto t=spill(multiply(extend(alpha),extend(xj)));
        for(int i=0;i<=j;++i) {
            const auto v=multiply(extend(x[start+i*inc]),extend(t));
            auto& cell=a.at(i,j);
            cell={binary64(v.real+cell.real),binary64(v.imag+cell.imag)};
        }
    }
}
void copy(int n,const ComplexValue* x,int incx,ComplexValue* y,int incy) {
    int ix=incx<0?(1-n)*incx:0,iy=incy<0?(1-n)*incy:0;
    for(int i=0;i<n;++i,ix+=incx,iy+=incy)y[iy]=x[ix];
}
ComplexValue dotu(int n,const ComplexValue* x,int incx,const ComplexValue* y,int incy) {
    int ix=incx<0?(1-n)*incx:0,iy=incy<0?(1-n)*incy:0;
    Extended acc{0,0};
    for(int i=0;i<n;++i,ix+=incx,iy+=incy) {
        const auto p=multiply(extend(x[ix]),extend(y[iy]));
        acc.real+=p.real;acc.imag+=p.imag;
    }
    return spill(acc);
}
void multiply_upper(int n,const ComplexValue& alpha,ComplexMatrixView a,
                    const ComplexValue* x,int incx,const ComplexValue& beta,ComplexValue* y,int incy) {
    if(n==0 || (alpha.real==0 && alpha.imag==0 && beta.real==1 && beta.imag==0))return;
    const int startx=incx<0?(1-n)*incx:0,starty=incy<0?(1-n)*incy:0;
    if(beta.real!=1 || beta.imag!=0)for(int i=0;i<n;++i) {
        auto& yi=y[starty+i*incy];
        if(beta.real==0 && beta.imag==0)yi={0,0};
        else yi=spill(multiply(extend(beta),extend(yi)));
    }
    if(alpha.real==0 && alpha.imag==0)return;
    for(int j=0;j<n;++j) {
        const auto t1=spill(multiply(extend(alpha),extend(x[startx+j*incx])));
        ComplexValue t2{0,0};
        for(int i=0;i<j;++i) {
            const auto p=multiply(extend(t1),extend(a.at(i,j)));
            auto& yi=y[starty+i*incy];
            yi={binary64(p.real+yi.real),binary64(p.imag+yi.imag)};
            const auto q=multiply(extend(a.at(i,j)),extend(x[startx+i*incx]));
            t2={binary64(q.real+t2.real),binary64(q.imag+t2.imag)};
        }
        const auto p=multiply(extend(t1),extend(a.at(j,j)));
        const auto q=multiply(extend(alpha),extend(t2));
        auto& yj=y[starty+j*incy];
        yj={binary64((p.real+yj.real)+q.real),binary64((p.imag+yj.imag)+q.imag)};
    }
}
int factor_symmetric_upper(int n,ComplexMatrixView a,int* pivots) {
    const int invalid=n<0?2:(a.stride<(n>1?n:1)?4:0);
    if(invalid) { argument_error("ZSYTF2",invalid);return -invalid; }
    constexpr long double alpha=0x1.47e0f66afed07p-1L;
    const ComplexValue one{1,0};
    int info=0;
    for(int k=n-1;k>=0;) {
        int step=1,kp=k;
        const double absakk=binary64(abs1(a.at(k,k)));
        const int imax=k>0?max_index(k,&a.at(0,k),1)-1:0;
        const double colmax=k>0?binary64(abs1(a.at(imax,k))):0;
        if(absakk==0 && colmax==0) {
            if(info==0)info=k+1;
        } else {
            if(absakk<alpha*colmax) {
                const int jmax=imax+max_index(k-imax,&a.at(imax,imax+1),a.stride);
                double rowmax=binary64(abs1(a.at(imax,jmax)));
                if(imax>0) {
                    const int j=max_index(imax,&a.at(0,imax),1)-1;
                    const long double v=abs1(a.at(j,imax));
                    rowmax=binary64(v<=rowmax?rowmax:v);
                }
                // The original multiplies alpha*colmax before colmax/rowmax.
                if(absakk<(alpha*colmax)*((long double)colmax/rowmax)) {
                    kp=imax;
                    if(abs1(a.at(imax,imax))<alpha*rowmax)step=2;
                }
            }
            const int kk=k-step+1;
            if(kp!=kk) {
                swap(kp,&a.at(0,kk),1,&a.at(0,kp),1);
                swap(kk-kp-1,&a.at(kp+1,kk),1,&a.at(kp,kp+1),a.stride);
                exchange(a.at(kk,kk),a.at(kp,kp));
                if(step==2)exchange(a.at(k-1,k),a.at(kp,k));
            }
            if(step==1) {
                ComplexValue inv;
                divide(inv,one,a.at(k,k));
                rank1_upper(k,negate(inv),&a.at(0,k),1,a);
                scale(k,inv,&a.at(0,k),1);
            } else {
                ComplexValue r1,r2,ev{},c{},s{};
                eigen2(a.at(k-1,k-1),a.at(k-1,k),a.at(k,k),r1,r2,ev,c,s);
                if(abs1(ev)!=0) {
                    divide(r1,one,r1);divide(r2,one,r2);
                    rotate(k-1,&a.at(0,k-1),1,&a.at(0,k),1,c,s);
                    rank1_upper(k-1,negate(r1),&a.at(0,k-1),1,a);
                    rank1_upper(k-1,negate(r2),&a.at(0,k),1,a);
                    scale(k-1,r1,&a.at(0,k-1),1);
                    scale(k-1,r2,&a.at(0,k),1);
                    rotate(k-1,&a.at(0,k-1),1,&a.at(0,k),1,c,negate(s));
                } else {
                    ComplexValue inv;
                    divide(inv,one,a.at(k-1,k));
                    const auto d11raw=multiply(extend(a.at(k,k)),extend(inv));
                    const auto d22raw=multiply(extend(a.at(k-1,k-1)),extend(inv));
                    const auto d11=spill(d11raw),d22=spill(d22raw);
                    const auto p=multiply(d11raw,d22raw);
                    const ComplexValue denom{binary64(1.0L-p.real),binary64(0.0L-p.imag)};
                    ComplexValue d;divide(d,negate(inv),denom);
                    for(int j=k-2;j>=0;--j) {
                        auto t=multiply(extend(d11),extend(a.at(j,k-1)));
                        t.real-=a.at(j,k).real;t.imag-=a.at(j,k).imag;
                        const auto w1=spill(multiply(extend(d),t));
                        t=multiply(extend(d22),extend(a.at(j,k)));
                        t.real-=a.at(j,k-1).real;t.imag-=a.at(j,k-1).imag;
                        const auto w2=spill(multiply(extend(d),t));
                        axpy(j+1,negate(w1),&a.at(0,k-1),1,&a.at(0,j),1);
                        axpy(j+1,negate(w2),&a.at(0,k),1,&a.at(0,j),1);
                        a.at(j,k-1)=w1;a.at(j,k)=w2;
                    }
                }
            }
        }
        pivots[k]=step==1?kp+1:-(kp+1);
        if(step==2)pivots[k-1]=-(kp+1);
        k-=step;
    }
    return info;
}
int invert_symmetric_upper(int n,ComplexMatrixView a,const int* pivots,ComplexValue* scratch) {
    const int invalid=n<0?2:(a.stride<(n>1?n:1)?4:0);
    if(invalid) { argument_error("ZSYTRI",invalid);return -invalid; }
    for(int i=n-1;i>=0;--i)
        if(pivots[i]>0 && a.at(i,i).real==0 && a.at(i,i).imag==0)return i+1;
    const ComplexValue one{1,0},minus_one{-1,0},zero{0,0};
    for(int k=0;k<n;) {
        int step=1;
        if(pivots[k]>0) {
            divide(a.at(k,k),one,a.at(k,k));
            if(k>0) {
                copy(k,&a.at(0,k),1,scratch,1);
                multiply_upper(k,minus_one,a,scratch,1,zero,&a.at(0,k),1);
                const auto dot=dotu(k,scratch,1,&a.at(0,k),1);
                auto& diag=a.at(k,k);
                diag={binary64((long double)diag.real-dot.real),binary64((long double)diag.imag-dot.imag)};
            }
        } else {
            const auto t=a.at(k,k+1);
            ComplexValue ak,akp1,akkp1;
            divide(ak,a.at(k,k),t);divide(akp1,a.at(k+1,k+1),t);divide(akkp1,a.at(k,k+1),t);
            auto p=multiply(extend(ak),extend(akp1));
            p.real-=1.0L;p.imag+=0.0L;
            const auto d=spill(multiply(extend(t),p));
            divide(a.at(k,k),akp1,d);
            divide(a.at(k+1,k+1),ak,d);
            divide(a.at(k,k+1),negate(akkp1),d);
            if(k>0) {
                copy(k,&a.at(0,k),1,scratch,1);
                multiply_upper(k,minus_one,a,scratch,1,zero,&a.at(0,k),1);
                auto dot=dotu(k,scratch,1,&a.at(0,k),1);
                auto& diag=a.at(k,k);
                diag={binary64((long double)diag.real-dot.real),binary64((long double)diag.imag-dot.imag)};
                dot=dotu(k,&a.at(0,k),1,&a.at(0,k+1),1);
                auto& off=a.at(k,k+1);
                off={binary64((long double)off.real-dot.real),binary64((long double)off.imag-dot.imag)};
                copy(k,&a.at(0,k+1),1,scratch,1);
                multiply_upper(k,minus_one,a,scratch,1,zero,&a.at(0,k+1),1);
                dot=dotu(k,scratch,1,&a.at(0,k+1),1);
                auto& diag2=a.at(k+1,k+1);
                diag2={binary64((long double)diag2.real-dot.real),binary64((long double)diag2.imag-dot.imag)};
            }
            step=2;
        }
        const int kp=(pivots[k]<0?-pivots[k]:pivots[k])-1;
        if(kp!=k) {
            swap(kp,&a.at(0,k),1,&a.at(0,kp),1);
            swap(k-kp-1,&a.at(kp+1,k),1,&a.at(kp,kp+1),a.stride);
            exchange(a.at(k,k),a.at(kp,kp));
            if(step==2)exchange(a.at(k,k+1),a.at(kp,k+1));
        }
        k+=step;
    }
    return 0;
}
}
