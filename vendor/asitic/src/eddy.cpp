#include "asitic/eddy.hpp"
#include "asitic/numerics.hpp"
#include "asitic/solver.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
int reflected(int index,int turns) {
    while(index>turns)index=static_cast<int>(2u*static_cast<unsigned>(turns)-static_cast<unsigned>(index)+1u);
    return index;
}
long double wire_length(const double* a) {
    const long double dx=(long double)a[3]-a[0],dy=(long double)a[4]-a[1],dz=(long double)a[5]-a[2];
    return x87::sqrt((dx*dx+dy*dy)+dz*dz);
}
long double integral(double frequency,double separation,double height,double width,const EddyEnvironment& e) {
    return green_integral(frequency,separation,height,width,*e.state,*e.workspace,e.runtime);
}
int rounded_integer(long double value,unsigned short mode) {
    unsigned short saved,temporary;
    asm volatile("fnstcw %0" : "=m"(saved));
    temporary=(saved&0xf3ffu)|mode;
    long double rounded;
    asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(rounded) : "0"(value),"m"(temporary),"m"(saved));
    return x87::truncate_int(binary64(rounded));
}
const ConductorSegment* next_segment(const ConductorSegment* segment) {
    return reinterpret_cast<const ConductorSegment*>(segment->next_address);
}
void read_wire(const ConductorSegment* segment,double* wire) {
    __builtin_memcpy(wire,segment,7*sizeof(double));
}
}
int eddy_packed_index(int index,int turns) {
    const unsigned i=static_cast<unsigned>(index),n=static_cast<unsigned>(turns);
    return static_cast<int>(index<=turns?4u*i-3u:4u*n-(4u*(i-1u-n)+1u));
}
long double eddy_folded_position(int index,double dimension,double width,double spacing,int turns) {
    index=reflected(index,turns);
    const int offset=static_cast<int>(static_cast<unsigned>(index)-1u);
    return ((long double)dimension-width)-((long double)width+spacing)*((long double)offset+offset);
}
long double eddy_signed_half_position(int index,double dimension,double width,double spacing,int turns) {
    if(index>turns) {
        const int folded=static_cast<int>(2u*static_cast<unsigned>(turns)-static_cast<unsigned>(index)+1u);
        return -eddy_signed_half_position(folded,dimension,width,spacing,turns);
    }
    const int offset=static_cast<int>(static_cast<unsigned>(index)-1u);
    return ((long double)dimension-width)*0.5L-((long double)width+spacing)*offset;
}
long double eddy_signed_separation(int i,int j,double dimension,double width,double spacing,int turns) {
    const int a=static_cast<int>(static_cast<unsigned>(reflected(i,turns))-1u),
              b=static_cast<int>(static_cast<unsigned>(reflected(j,turns))-1u);
    const long double step=(long double)width+spacing,base=(long double)dimension-width;
    const long double result=x87::sqrt((base-((long double)a+a)*step)*(base-((long double)b+b)*step));
    return (i>turns)!=(j>turns)?-result:result;
}
long double eddy_axial_length(const double* wire) {
    return wire_length(wire)-((long double)wire[6]+wire[6]);
}
int eddy_classify_pair(const double* a,const double* b,double* product) {
    const long double ax=(long double)a[3]-a[0],ay=(long double)a[4]-a[1],az=(long double)a[5]-a[2],
        bx=(long double)b[3]-b[0],by=(long double)b[4]-b[1],bz=(long double)b[5]-b[2];
    const long double dot=(ax*bx+by*ay)+az*bz;
    *product=binary64(dot);
    if(0x1.b7cdfd9d7bdbbp-34>x87::abs(dot))return 7;
    const double al=binary64(wire_length(a)),bl=binary64(wire_length(b)),
        d1=binary64(distance(a,b)),d2=binary64(distance(a,b+3)),d3=binary64(distance(a+3,b));
    const long double d4=distance(a+3,b+3);
    long double nearest=d1<d2?d1:d2;
    if(!(nearest<d3))nearest=d3;
    if(!(nearest<d4))nearest=d4;
    const double shorter=al<bl?al:bl;
    if((long double)shorter*0.5L<nearest)return 9;
    const long double cosine=(long double)*product/((long double)al*bl);
    *product=binary64(cosine);
    return 0x1.fe0ded288ce70p-1<=x87::abs(cosine)?0:9;
}
long double eddy_axial_term(const double* wire,double frequency,const EddyEnvironment& e) {
    const long double length=eddy_axial_length(wire);
    const double height=binary64(((long double)e.coordinate_scale+e.coordinate_scale)*wire[2]),
        width=binary64((long double)e.coordinate_scale*wire[6]);
    const long double value=integral(frequency,0,height,width,e);
    return (length*e.coordinate_scale)*value;
}
long double eddy_pair_term(const double* a,const double* b,double frequency,const EddyEnvironment& e) {
    double product;
    if(eddy_classify_pair(a,b,&product)!=0)return 0;
    const long double dx=(long double)a[0]-b[0],dz=x87::abs((long double)a[2]-b[2]);
    constexpr double threshold=0x1.b7cdfd9d7bdbbp-34,scale=0x1.0c6f7a0b5ed8dp-20;
    if(x87::abs(dx)<threshold && x87::abs(dz)<threshold)return 0;
    const long double distance_ab=distance(a,b),square=(distance_ab*distance_ab-dx*dx)-dz*dz;
    const long double separation=threshold>x87::abs(square)?0:x87::sqrt(square)*scale;
    const double al=binary64(eddy_axial_length(a)),bl=binary64(eddy_axial_length(b));
    const long double length=x87::sqrt((long double)al*bl)*scale;
    const double height=binary64(((long double)a[2]+b[2])*scale),
        width=binary64((long double)a[6]*e.coordinate_scale);
    const long double value=integral(frequency,binary64(separation),height,width,e);
    return (length*product)*value;
}
void assemble_eddy_matrix(RealMatrixView out,const SquareSpiralGeometry& g,double frequency,double height,const EddyEnvironment& e) {
    const int n=rounded_integer(g.turns,0x0800u),size=2*n;
    const double dimension=binary64((long double)e.coordinate_scale*g.dimension),
        width=binary64((long double)e.coordinate_scale*g.width);
    const long double spacing_extended=(long double)e.coordinate_scale*g.spacing,
        pitch_extended=spacing_extended+width;
    const double spacing=binary64(spacing_extended),pitch=binary64(pitch_extended),
        inner=binary64((long double)binary64((long double)dimension-((long double)(n-1)+(n-1))*pitch_extended)
            -((long double)width+width));
    const double diagonal=binary64(integral(frequency,0,height,width,e));
    for(int i=1;i<=size;++i)out.at(i-1,i-1)=binary64(eddy_folded_position(i,dimension,width,spacing,n)*diagonal);
    for(int i=1;i<=n;++i)for(int j=i+1;j<=n;++j) {
        const long double factor=eddy_signed_separation(i,j,dimension,width,spacing,n);
        if(i==1)out.at(0,j-1)=binary64(factor*integral(frequency,binary64((long double)pitch*(j-1)),height,width,e));
        else {
            const long double previous=eddy_signed_separation(i-1,j-1,dimension,width,spacing,n);
            out.at(i-1,j-1)=binary64((factor*out.at(i-2,j-2))/previous);
        }
    }
    for(int i=1;i<=n;++i) {
        const long double factor=eddy_signed_separation(i,n+1,dimension,width,spacing,n);
        out.at(i-1,n)=binary64(factor*integral(frequency,binary64((long double)pitch*(n-i)+inner),height,width,e));
    }
    for(int i=1;i<=n;++i)for(int j=n+2;j<=size;++j) {
        const long double factor=eddy_signed_separation(i,j,dimension,width,spacing,n);
        if(i==1)out.at(0,j-1)=binary64(factor*integral(frequency,binary64((long double)pitch*(j-2)+inner),height,width,e));
        else {
            const long double previous=eddy_signed_separation(i-1,j-1,dimension,width,spacing,n);
            out.at(i-1,j-1)=binary64((factor*out.at(i-2,j-2))/previous);
        }
    }
    for(int i=n+1;i<=size;++i)for(int j=n+2;j<=size;++j) {
        const long double factor=eddy_signed_separation(i,j,dimension,width,spacing,n),
            previous=eddy_signed_separation(i-n,j-n,dimension,width,spacing,n);
        out.at(i-1,j-1)=binary64((factor*out.at(i-n-1,j-n-1))/previous);
    }
    for(int i=1;i<=size;++i)for(int j=1;j<i;++j)out.at(i-1,j-1)=out.at(j-1,i-1);
}
void add_eddy_square(ComplexMatrixView out,const SquareSpiralGeometry& g,double frequency,
    const EddySquareOptions& options,const EddyEnvironment& e,SolverMemory memory,WarningSink warnings) {
    const int n=rounded_integer(g.turns,0x0800u),size=2*n;
    if(options.layer_count==0) { warnings.emit("Warning:  No valid eddy current layers defined.");return; }
    if(options.shape_kind!=0) {
        warnings.emit("Warning:  Eddy current matrix generation routine works with square spirals only.  Skipping calculations.");
        return;
    }
    auto* data=static_cast<double*>(memory.arrays.get(static_cast<unsigned>(size)*static_cast<unsigned>(size)*8u));
    if(!data)warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    const RealMatrixView matrix{data,size};
    assemble_eddy_matrix(matrix,g,frequency,binary64(0x1.0c6f7a0b5ed8dp-19L*options.metal_height),e);
    if(options.snapshot)options.snapshot("Eddy.out",matrix,size,size,options.snapshot_user);
    const int limit=rounded_integer((long double)g.turns*4.0L,0x0400u);
    for(int i=1;i<size;++i)for(int j=1;j<size;++j) {
        const int row=eddy_packed_index(i,n)-1,column=eddy_packed_index(j,n)-1;
        if(row<limit && column<limit) {
            auto& value=out.at(row,column);
            value.real=binary64((long double)matrix.at(i-1,j-1)+value.real);
            value.imag=binary64((long double)value.imag+0.0L);
        }
        if(row+1<limit && column+1<limit) {
            auto& value=out.at(row+1,column+1);
            value.real=binary64((long double)matrix.at(i-1,j-1)+value.real);
            value.imag=binary64((long double)value.imag+0.0L);
        }
    }
    memory.put(data);
}
void fold_eddy_segments(const ShapeFilamentList& shape,ComplexMatrixView out,unsigned rows,
    double frequency,int metal_count,const EddyEnvironment& e) {
    unsigned row=0;
    for(auto* segment=shape.first;segment;segment=next_segment(segment))if(segment->metal<metal_count) {
        double wire[7];read_wire(segment,wire);
        const long double term=eddy_axial_term(wire,frequency,e);
        auto& value=out.at(row,row);
        value.real=binary64((long double)value.real+term);
        value.imag=binary64((long double)value.imag+0.0L);++row;
    }
    row=0;
    for(auto* a=shape.first;a;a=next_segment(a))if(a->metal<metal_count) {
        unsigned column=row+1;
        for(auto* b=next_segment(a);b && column<rows;b=next_segment(b),++column) {
            double aw[7],bw[7];read_wire(a,aw);read_wire(b,bw);
            const long double term=eddy_pair_term(aw,bw,frequency,e);
            auto& upper=out.at(row,column);
            upper.real=binary64((long double)upper.real+term);
            upper.imag=binary64((long double)upper.imag+0.0L);
            auto& lower=out.at(column,row);
            lower.real=binary64((long double)lower.real+term);
            lower.imag=binary64((long double)lower.imag+0.0L);
        }
        ++row;
    }
}
}
