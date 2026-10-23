#include "asitic/lapack.hpp"
#include "x87.hpp"

namespace asitic::lapack {
namespace {
using x87::binary64;
struct Extended { long double real,imag; };
Extended extend(ComplexValue z) { return {z.real,z.imag}; }
Extended product(Extended a,Extended b) { return {a.real*b.real-a.imag*b.imag,a.imag*b.real+a.real*b.imag}; }
ComplexValue spill(Extended z) { return {binary64(z.real),binary64(z.imag)}; }
bool zero(ComplexValue z) { return z.real==0 && z.imag==0; }
bool one(ComplexValue z) { return z.real==1 && z.imag==0; }
ComplexMatrixView sub(ComplexMatrixView a,int row,int column) { return {&a.at(row,column),a.stride}; }
}
void multiply_general(int m,int n,const ComplexValue& alpha,ComplexMatrixView a,
                      const ComplexValue* x,int incx,const ComplexValue& beta,ComplexValue* y,int incy) {
    if(m==0 || n==0 || (zero(alpha) && one(beta)))return;
    const int sx=incx<0?(1-n)*incx:0,sy=incy<0?(1-m)*incy:0;
    if(!one(beta))for(int i=0;i<m;++i) {
        auto& yi=y[sy+i*incy];
        yi=zero(beta)?ComplexValue{0,0}:spill(product(extend(beta),extend(yi)));
    }
    if(zero(alpha))return;
    for(int j=0;j<n;++j) {
        if(zero(x[sx+j*incx]))continue;
        auto t=product(extend(alpha),extend(x[sx+j*incx]));
        // ZGEMV retains the real coefficient and reloads the imaginary one.
        t.imag=binary64(t.imag);
        for(int i=0;i<m;++i) {
            const auto p=product(t,extend(a.at(i,j)));
            auto& yi=y[sy+i*incy];
            yi={binary64(p.real+yi.real),binary64(p.imag+yi.imag)};
        }
    }
}
void multiply_transposed_right(int m,int n,int k,const ComplexValue& alpha,
                               ComplexMatrixView a,ComplexMatrixView b,const ComplexValue& beta,
                               ComplexMatrixView out) {
    if(m==0 || n==0 || ((zero(alpha) || k==0) && one(beta)))return;
    for(int j=0;j<n;++j) {
        if(!one(beta))for(int i=0;i<m;++i) {
            auto& cell=out.at(i,j);
            cell=zero(beta)?ComplexValue{0,0}:spill(product(extend(beta),extend(cell)));
        }
        if(zero(alpha))continue;
        for(int l=0;l<k;++l) {
            if(zero(b.at(j,l)))continue;
            auto t=product(extend(alpha),extend(b.at(j,l)));
            // ZGEMM NT spills the real coefficient, retaining the imaginary.
            t.real=binary64(t.real);
            for(int i=0;i<m;++i) {
                const auto p=product(t,extend(a.at(i,l)));
                auto& cell=out.at(i,j);
                cell={binary64(p.real+cell.real),binary64(p.imag+cell.imag)};
            }
        }
    }
}
int factor_panel_upper(int n,int block,ComplexMatrixView a,int* pivots,ComplexMatrixView w,int& columns) {
    constexpr long double alpha=0x1.47e0f66afed07p-1L;
    const ComplexValue unit{1,0},minus{-1,0};
    int info=0,k=n-1,kw=0;
    for(;;) {
        kw=k+block-n;
        if(k<0 || (block<n && k<=n-block))break;
        copy(k+1,&a.at(0,k),1,&w.at(0,kw),1);
        if(k<n-1)multiply_general(k+1,n-k-1,minus,sub(a,0,k+1),&w.at(k,kw+1),w.stride,unit,&w.at(0,kw),1);
        int step=1,kp=k;
        const double absakk=binary64(abs1(w.at(k,kw)));
        const int imax=k>0?max_index(k,&w.at(0,kw),1)-1:0;
        const double colmax=k>0?binary64(abs1(w.at(imax,kw))):0;
        if(absakk==0 && colmax==0) {
            if(info==0)info=k+1;
        } else {
            if(absakk<alpha*colmax) {
                copy(imax+1,&a.at(0,imax),1,&w.at(0,kw-1),1);
                copy(k-imax,&a.at(imax,imax+1),a.stride,&w.at(imax+1,kw-1),1);
                if(k<n-1)multiply_general(k+1,n-k-1,minus,sub(a,0,k+1),&w.at(imax,kw+1),w.stride,unit,&w.at(0,kw-1),1);
                const int jmax=imax+max_index(k-imax,&w.at(imax+1,kw-1),1);
                double rowmax=binary64(abs1(w.at(jmax,kw-1)));
                if(imax>0) {
                    const int j=max_index(imax,&w.at(0,kw-1),1)-1;
                    const long double v=abs1(w.at(j,kw-1));
                    rowmax=binary64(v<=rowmax?rowmax:v);
                }
                if(absakk<(alpha*colmax)*((long double)colmax/rowmax)) {
                    kp=imax;
                    if(abs1(w.at(imax,kw-1))>=alpha*rowmax)copy(k+1,&w.at(0,kw-1),1,&w.at(0,kw),1);
                    else step=2;
                }
            }
            const int kk=k-step+1,kkw=kw-step+1;
            if(kp!=kk) {
                a.at(kp,k)=a.at(kk,k);
                copy(k-kp-1,&a.at(kp+1,kk),1,&a.at(kp,kp+1),a.stride);
                copy(kp+1,&a.at(0,kk),1,&a.at(0,kp),1);
                swap(n-kk,&a.at(kk,kk),a.stride,&a.at(kp,kk),a.stride);
                swap(n-kk,&w.at(kk,kkw),w.stride,&w.at(kp,kkw),w.stride);
            }
            if(step==1) {
                copy(k+1,&w.at(0,kw),1,&a.at(0,k),1);
                ComplexValue inv;divide(inv,unit,a.at(k,k));
                scale(k,inv,&a.at(0,k),1);
            } else {
                if(k>1) {
                    ComplexValue d11,d22,t,d;
                    const auto off=w.at(k-1,kw);
                    divide(d11,w.at(k,kw),off);divide(d22,w.at(k-1,kw-1),off);
                    auto p=product(extend(d11),extend(d22));
                    const ComplexValue denom{binary64(p.real-1.0L),binary64(p.imag+0.0L)};
                    divide(t,unit,denom);divide(d,t,off);
                    for(int j=0;j<k-1;++j) {
                        p=product(extend(d11),extend(w.at(j,kw-1)));
                        p.real-=w.at(j,kw).real;p.imag-=w.at(j,kw).imag;
                        a.at(j,k-1)=spill(product(extend(d),p));
                        p=product(extend(d22),extend(w.at(j,kw)));
                        p.real-=w.at(j,kw-1).real;p.imag-=w.at(j,kw-1).imag;
                        a.at(j,k)=spill(product(extend(d),p));
                    }
                }
                a.at(k-1,k-1)=w.at(k-1,kw-1);
                a.at(k-1,k)=w.at(k-1,kw);
                a.at(k,k)=w.at(k,kw);
            }
        }
        pivots[k]=step==1?kp+1:-(kp+1);
        if(step==2)pivots[k-1]=-(kp+1);
        k-=step;
    }
    if(k>=0)for(int j=(k/block)*block;j>=0;j-=block) {
        const int jb=k-j+1<block?k-j+1:block;
        for(int jj=j;jj<j+jb;++jj)
            multiply_general(jj-j+1,n-k-1,minus,sub(a,j,k+1),&w.at(jj,kw+1),w.stride,unit,&a.at(j,jj),1);
        multiply_transposed_right(j,jb,n-k-1,minus,sub(a,0,k+1),sub(w,j,kw+1),unit,sub(a,0,j));
    }
    for(int j=k+1;j<n;) {
        const int jj=j,kp=(pivots[j]<0?-pivots[j]:pivots[j])-1;
        if(pivots[j]<0)++j;
        ++j;
        if(kp!=jj && j<n)swap(n-j,&a.at(kp,j),a.stride,&a.at(jj,j),a.stride);
    }
    columns=n-k-1;
    return info;
}
int factor_symmetric_upper_workspace(int n,ComplexMatrixView a,int* pivots,ComplexValue* work,int work_size) {
    const int invalid=n<0?2:(a.stride<(n>1?n:1)?4:(work_size<1?7:0));
    if(invalid) { argument_error("ZSYTRF",invalid);return -invalid; }
    int block=64,recommended=1;
    if(n>block) {
        recommended=block*n;
        if(work_size<recommended) {
            block=work_size/n;
            if(block<1)block=1;
            if(block<8)block=n;
        }
    }
    int info=0;
    for(int k=n;k>0;) {
        int columns,partial;
        if(k>block)partial=factor_panel_upper(k,block,a,pivots,{work,n},columns);
        else { partial=factor_symmetric_upper(k,a,pivots);columns=k; }
        if(info==0 && partial>0)info=partial;
        k-=columns;
    }
    work[0]={static_cast<double>(recommended),0};
    return info;
}
bool invert_symmetric_matrix(int n,ComplexMatrixView a,bool mirror,int* pivots,
                              ComplexValue* factor_work,ComplexValue* inverse_work,WarningSink warnings) {
    const ComplexMatrixView dense{a.data,n};
    // The C++ wrapper passes a stack pointer as ILAENV's by-value string
    // length. The signed negative length pads the name with spaces, returning
    // block size 1. Preserve its n-cell workspace and resulting dispatch.
    int info=factor_symmetric_upper_workspace(n,dense,pivots,factor_work,n);
    if(info<0) { warnings.emit("Error:  Illegal option passed to zsytrf.");return false; }
    if(info>0) { warnings.emit("Error:  Matrix factorization produced a singular matrix.");return false; }
    info=invert_symmetric_upper(n,dense,pivots,inverse_work);
    if(info<0) { warnings.emit("Error:  Illegal option passed to zsytri.");return false; }
    if(info>0) { warnings.emit("Error:  Matrix factorization produced a singular matrix.");return false; }
    if(mirror)for(int j=0;j<n;++j)for(int i=0;i<j;++i)a.at(j,i)=a.at(i,j);
    return true;
}
}
