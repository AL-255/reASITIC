#include "asitic/lapack.hpp"
#include "x87.hpp"

namespace asitic::lapack {
namespace {
using x87::binary64;
struct Extended { long double real,imag; };
Extended extend(ComplexValue z) { return {z.real,z.imag}; }
Extended product(Extended a,Extended b) { return {a.real*b.real-a.imag*b.imag,a.imag*b.real+a.real*b.imag}; }
ComplexValue spill(Extended z) { return {binary64(z.real),binary64(z.imag)}; }
bool one(ComplexValue z) { return z.real==1 && z.imag==0; }
ComplexMatrixView sub(ComplexMatrixView a,int row,int column) { return {&a.at(row,column),a.stride}; }
bool zero(ComplexValue z) { return z.real==0 && z.imag==0; }
}
void rank1_general(int m,int n,const ComplexValue& alpha,const ComplexValue* x,int incx,
                   const ComplexValue* y,int incy,ComplexMatrixView a) {
    const int error=m<0?1:n<0?2:incx==0?5:incy==0?7:a.stride<(m>1?m:1)?9:0;
    if(error) { argument_error("ZGERU ",error);return; }
    if(m==0 || n==0 || zero(alpha))return;
    const int sx=incx<0?(1-m)*incx:0,sy=incy<0?(1-n)*incy:0;
    for(int j=0;j<n;++j) {
        if(zero(y[sy+j*incy]))continue;
        const auto t=product(extend(alpha),extend(y[sy+j*incy]));
        for(int i=0;i<m;++i) {
            const auto v=product(extend(x[sx+i*incx]),t);
            auto& cell=a.at(i,j);
            cell={binary64(v.real+cell.real),binary64(v.imag+cell.imag)};
        }
    }
}
int factor_general_unblocked(int m,int n,ComplexMatrixView a,int* pivots) {
    const int error=m<0?1:n<0?2:a.stride<(m>1?m:1)?4:0;
    if(error) { argument_error("ZGETF2",error);return -error; }
    if(m==0 || n==0)return 0;
    const ComplexValue unit{1,0},minus{-1,0};
    const int steps=m<n?m:n;
    int info=0;
    for(int j=0;j<steps;++j) {
        const int jp=j+max_index(m-j,&a.at(j,j),1)-1;
        pivots[j]=jp+1;
        if(!zero(a.at(jp,j))) {
            if(jp!=j)swap(n,&a.at(j,0),a.stride,&a.at(jp,0),a.stride);
            if(j<m-1) { ComplexValue inv;divide(inv,unit,a.at(j,j));scale(m-j-1,inv,&a.at(j+1,j),1); }
        } else if(info==0)info=j+1;
        if(j<steps-1)rank1_general(m-j-1,n-j-1,minus,&a.at(j+1,j),1,
                                  &a.at(j,j+1),a.stride,{&a.at(j+1,j+1),a.stride});
    }
    return info;
}
void multiply_triangular_upper(int n,ComplexMatrixView a,bool unit,ComplexValue* x,int inc) {
    const int error=n<0?4:a.stride<(n>1?n:1)?6:inc==0?8:0;
    if(error) { argument_error("ZTRMV ",error);return; }
    const int start=inc<0?(1-n)*inc:0;
    for(int j=0;j<n;++j) {
        auto& xj=x[start+j*inc];
        if(zero(xj))continue;
        const auto t=extend(xj);
        for(int i=0;i<j;++i) {
            const auto v=product(t,extend(a.at(i,j)));
            auto& xi=x[start+i*inc];
            xi={binary64(v.real+xi.real),binary64(v.imag+xi.imag)};
        }
        if(!unit)xj=spill(product(extend(xj),extend(a.at(j,j))));
    }
}
int invert_triangular_upper_unblocked(int n,ComplexMatrixView a,bool unit) {
    const int error=n<0?3:a.stride<(n>1?n:1)?5:0;
    if(error) { argument_error("ZTRTI2",error);return -error; }
    const ComplexValue one{1,0};
    for(int j=0;j<n;++j) {
        ComplexValue t{-1,0};
        if(!unit) { ComplexValue v;divide(v,one,a.at(j,j));a.at(j,j)=v;t={-v.real,-v.imag}; }
        multiply_triangular_upper(j,a,unit,&a.at(0,j),1);
        scale(j,t,&a.at(0,j),1);
    }
    return 0;
}

void multiply_matrices(int m,int n,int k,const ComplexValue& alpha,ComplexMatrixView a,
                        ComplexMatrixView b,const ComplexValue& beta,ComplexMatrixView out) {
    if(m==0 || n==0 || ((zero(alpha) || k==0) && one(beta)))return;
    for(int j=0;j<n;++j) {
        if(!one(beta))for(int i=0;i<m;++i) {
            auto& c=out.at(i,j);c=zero(beta)?ComplexValue{0,0}:spill(product(extend(beta),extend(c)));
        }
        if(zero(alpha))continue;
        for(int l=0;l<k;++l) {
            if(zero(b.at(l,j)))continue;
            auto t=product(extend(alpha),extend(b.at(l,j)));
            // ZGEMM NN stores the real coefficient at frame -0x50.
            t.real=binary64(t.real);
            for(int i=0;i<m;++i) {
                const auto v=product(t,extend(a.at(i,l)));auto& c=out.at(i,j);
                c={binary64(v.real+c.real),binary64(v.imag+c.imag)};
            }
        }
    }
}
void solve_triangular(int m,int n,const ComplexValue& alpha,ComplexMatrixView a,
                       bool right,bool upper,bool unit,ComplexMatrixView b) {
    const int order=right?n:m;
    const int error=m<0?5:n<0?6:a.stride<(order>1?order:1)?9:b.stride<(m>1?m:1)?11:0;
    if(error) { argument_error("ZTRSM ",error);return; }
    if(n==0)return;
    if(zero(alpha)) { for(int j=0;j<n;++j)for(int i=0;i<m;++i)b.at(i,j)={0,0};return; }
    const ComplexValue unity{1,0};
    for(int j=0;j<n;++j) {
        if(!one(alpha))for(int i=0;i<m;++i)b.at(i,j)=spill(product(extend(alpha),extend(b.at(i,j))));
        if(!right)for(int kk=0;kk<m;++kk) {
            const int k=upper?m-1-kk:kk;
            if(zero(b.at(k,j)))continue;
            if(!unit) { ComplexValue v;divide(v,b.at(k,j),a.at(k,k));b.at(k,j)=v; }
            const int start=upper?0:k+1,end=upper?k:m;
            for(int i=start;i<end;++i) {
                const auto v=product(extend(b.at(k,j)),extend(a.at(i,k)));auto& c=b.at(i,j);
                c={binary64(c.real-v.real),binary64(c.imag-v.imag)};
            }
        }
    }
    if(!right)return;
    // Right-side substitution scales each target column before subtracting
    // previously solved columns, then multiplies by a rounded reciprocal.
    // Each column is independent until substitution starts.
    for(int jj=0;jj<n;++jj) {
        const int j=upper?jj:n-1-jj;
        const int start=upper?0:j+1,end=upper?j:n;
        for(int k=start;k<end;++k) {
            if(zero(a.at(k,j)))continue;
            for(int i=0;i<m;++i) {
                const auto v=product(extend(a.at(k,j)),extend(b.at(i,k)));auto& c=b.at(i,j);
                c={binary64(c.real-v.real),binary64(c.imag-v.imag)};
            }
        }
        if(!unit) {
            ComplexValue inv;divide(inv,unity,a.at(j,j));
            for(int i=0;i<m;++i)b.at(i,j)=spill(product(extend(inv),extend(b.at(i,j))));
        }
    }
}
void multiply_triangular_left_upper(int m,int n,const ComplexValue& alpha,ComplexMatrixView a,
                                      bool unit,ComplexMatrixView b) {
    const int error=m<0?5:n<0?6:a.stride<(m>1?m:1)?9:b.stride<(m>1?m:1)?11:0;
    if(error) { argument_error("ZTRMM ",error);return; }
    if(n==0)return;
    if(zero(alpha)) { for(int j=0;j<n;++j)for(int i=0;i<m;++i)b.at(i,j)={0,0};return; }
    for(int j=0;j<n;++j)for(int k=0;k<m;++k) {
        if(zero(b.at(k,j)))continue;
        auto t=product(extend(alpha),extend(b.at(k,j)));
        for(int i=0;i<k;++i) {
            const auto v=product(t,extend(a.at(i,k)));auto& c=b.at(i,j);
            c={binary64(v.real+c.real),binary64(v.imag+c.imag)};
        }
        if(!unit)t=product(t,extend(a.at(k,k)));
        b.at(k,j)=spill(t);
    }
}
int factor_general(int m,int n,ComplexMatrixView a,int* pivots) {
    const int error=m<0?1:n<0?2:a.stride<(m>1?m:1)?4:0;
    if(error) { argument_error("ZGETRF",error);return -error; }
    const int steps=m<n?m:n,block=64;
    if(steps<=block)return factor_general_unblocked(m,n,a,pivots);
    const ComplexValue one{1,0},minus{-1,0};int info=0;
    for(int j=0;j<steps;j+=block) {
        const int jb=steps-j<block?steps-j:block;
        const int part=factor_general_unblocked(m-j,jb,sub(a,j,j),pivots+j);
        if(info==0 && part>0)info=part+j;
        for(int k=j;k<j+jb;++k)pivots[k]+=j;
        for(int k=j;k<j+jb;++k)if(pivots[k]!=k+1)swap(j,&a.at(k,0),a.stride,&a.at(pivots[k]-1,0),a.stride);
        if(j+jb<n) {
            for(int k=j;k<j+jb;++k)if(pivots[k]!=k+1)swap(n-j-jb,&a.at(k,j+jb),a.stride,&a.at(pivots[k]-1,j+jb),a.stride);
            solve_triangular(jb,n-j-jb,one,sub(a,j,j),false,false,true,sub(a,j,j+jb));
            if(j+jb<m)multiply_matrices(m-j-jb,n-j-jb,jb,minus,sub(a,j+jb,j),sub(a,j,j+jb),one,sub(a,j+jb,j+jb));
        }
    }
    return info;
}
int invert_triangular_upper(int n,ComplexMatrixView a,bool unit) {
    const int error=n<0?3:a.stride<(n>1?n:1)?5:0;
    if(error) { argument_error("ZTRTRI",error);return -error; }
    if(!unit)for(int j=0;j<n;++j)if(zero(a.at(j,j)))return j+1;
    constexpr int block=64;
    if(n<=block)return invert_triangular_upper_unblocked(n,a,unit);
    const ComplexValue one{1,0},minus{-1,0};
    for(int j=0;j<n;j+=block) {
        const int jb=n-j<block?n-j:block;
        multiply_triangular_left_upper(j,jb,one,a,unit,sub(a,0,j));
        solve_triangular(j,jb,minus,sub(a,j,j),true,true,unit,sub(a,0,j));
        invert_triangular_upper_unblocked(jb,sub(a,j,j),unit);
    }
    return 0;
}
int invert_general(int n,ComplexMatrixView a,const int* pivots,ComplexValue* work,int work_size) {
    work[0]={static_cast<double>(n>1?n:1),0};
    const int error=n<0?1:a.stride<(n>1?n:1)?3:work_size<(n>1?n:1)?6:0;
    if(error) { argument_error("ZGETRI",error);return -error; }
    if(n==0)return 0;
    const int info=invert_triangular_upper(n,a,false);if(info)return info;
    int block=64,iws=n;
    if(block<n) { iws=n*block;if(work_size<iws)block=work_size/n; }
    const ComplexValue one{1,0},minus{-1,0};
    if(block<2 || block>=n) {
        for(int j=n-1;j>=0;--j) {
            for(int i=j+1;i<n;++i) { work[i]=a.at(i,j);a.at(i,j)={0,0}; }
            if(j<n-1)multiply_general(n,n-j-1,minus,sub(a,0,j+1),work+j+1,1,one,&a.at(0,j),1);
        }
    } else {
        for(int j=((n-1)/block)*block;j>=0;j-=block) {
            const int jb=n-j<block?n-j:block;
            for(int jj=j;jj<j+jb;++jj)for(int i=jj+1;i<n;++i) {
                work[i+(jj-j)*n]=a.at(i,jj);a.at(i,jj)={0,0};
            }
            if(j+jb<n)multiply_matrices(n,jb,n-j-jb,minus,sub(a,0,j+jb),{work+j+jb,n},one,sub(a,0,j));
            solve_triangular(n,jb,one,{work+j,n},true,false,true,sub(a,0,j));
        }
    }
    for(int j=n-2;j>=0;--j)if(pivots[j]!=j+1)swap(n,&a.at(0,j),1,&a.at(0,pivots[j]-1),1);
    work[0]={static_cast<double>(iws),0};return 0;
}

bool invert_general_matrix(int n,ComplexMatrixView a,int* pivots,ComplexValue* work,
                            GeneralInverseRuntime runtime) {
    const ComplexMatrixView dense{a.data,n};
    if(factor_general(n,n,dense,pivots)!=0) {
        if(runtime.cleanup)runtime.cleanup(runtime.user);
        runtime.warnings.emit("Error:  Error in matrix factorization.");return false;
    }
    for(int i=0;i<n;++i)work[i]={0,0};
    if(invert_general(n,dense,pivots,work,n)!=0) {
        runtime.warnings.emit("Error:  Error in matrix inversion.");return false;
    }
    return true;
}
}
