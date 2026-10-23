#include "asitic/impedance.hpp"
#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
bool fill_filament_impedance(ComplexMatrixView out,const InductanceContext& ctx,
                             const FilamentSettings& settings,WarningSink warnings) {
    using x87::binary64;
    constexpr long double tau=0x1.921fb54442d18p+2L;
    constexpr long double nanosecond=0x1.12e0be826d695p-30L;
    constexpr long double eps=0x1.b7cdfd9d7bdbbp-34L;
    const double omega=binary64(nanosecond*binary64(tau*ctx.frequency));
    int row_base=0;
    for(int i=0;i<ctx.segment_count;++i) {
        const int ni=ctx.width_counts[i]*ctx.thickness_counts[i];
        int column_base=0;
        for(int j=0;j<ctx.segment_count;++j) {
            const int nj=ctx.width_counts[j]*ctx.thickness_counts[j];
            for(int ii=0;ii<ni;++ii) for(int jj=0;jj<nj;++jj) {
                Filament a,b;
                build_filament(*ctx.segments[i],a,ctx.origin,0,
                               (ii/ctx.thickness_counts[i])%ctx.width_counts[i],ii%ctx.thickness_counts[i],
                               1,ctx.width_counts[i],ctx.thickness_counts[i],settings);
                build_filament(*ctx.segments[j],b,ctx.origin,0,
                               (jj/ctx.thickness_counts[j])%ctx.width_counts[j],jj%ctx.thickness_counts[j],
                               1,ctx.width_counts[j],ctx.thickness_counts[j],settings);
                const int row=row_base+ii,col=column_base+jj;
                if(ii==jj && i==j) {
                    const long double length=distance(a.values+3,a.values);
                    double inductance=0;
                    long double resistance=0;
                    if(eps < length) {
                        const long double width=a.values[6], thick=a.values[7];
                        const long double sum=width+thick;
                        inductance=binary64((length*0x1.a36e2eb1c432dp-13L)
                                             *((x87::log((length+length)/sum)+0x1.004039abf3387p-1L)
                                               +sum/(length*3.0L)));
                        resistance=((length/width)*settings.metals[a.metal].resistance)*ctx.thickness_counts[i];
                    }
                    out.at(row,col)={binary64(resistance),binary64(static_cast<long double>(omega)*inductance)};
                } else if(row < col) {
                    double mutual;
                    const char* message=nullptr;
                    if(segment_mutual(a.values,b.values,&mutual,&message)==-1) {
                        warnings.emit("Error:  CalcMutInd error.");return false;
                    }
                    if(message) warnings.emit(message);
                    out.at(row,col)={0.0,binary64(static_cast<long double>(omega)*mutual)};
                }
            }
            column_base+=nj;
        }
        row_base+=ni;
    }
    return true;
}

void reduce_filament_impedance(ComplexMatrixView full,ComplexMatrixView reduced,
                               const InductanceContext& ctx) {
    using x87::binary64;
    int row_base=0,rows=0;
    const auto group_size=[&](int group) {
        int count=ctx.width_counts[group]*ctx.thickness_counts[group];
        for(int k=1;k<=ctx.extra_segments[group];++k)
            count+=ctx.width_counts[group+k]*ctx.thickness_counts[group+k];
        return count;
    };
    for(int i=0;i<ctx.group_count;++i) {
        row_base+=rows;
        int col_base=0,cols=0;
        for(int j=0;j<ctx.group_count;++j) {
            col_base+=cols;
            ComplexValue& value=reduced.at(i,j);
            value={0.0,0.0};
            rows=group_size(i);cols=group_size(j);
            for(int r=0;r<rows;++r) for(int c=0;c<cols;++c) {
                const int row=row_base+r,col=col_base+c;
                const auto& input=row > col ? full.at(col,row) : full.at(row,col);
                value.real=binary64(static_cast<long double>(value.real)+input.real);
                value.imag=binary64(static_cast<long double>(value.imag)+input.imag);
            }
        }
    }
}
}
