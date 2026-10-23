#include "asitic/analysis_reports.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
using x87::binary64;
long double load(const double& value) { long double out;asm volatile("fldl %1" : "=t"(out) : "m"(value));return out; }
long double multiply_memory(long double left,const double& right) { long double out;asm volatile("fmull %2" : "=t"(out) : "0"(left),"m"(right));return out; }
long double divide_memory(long double denominator,const double& numerator) { long double out;asm volatile("fdivrl %2" : "=t"(out) : "0"(denominator),"m"(numerator));return out; }
long double add_memory(long double left,const double& right) { long double out;asm volatile("faddl %2" : "=t"(out) : "0"(left),"m"(right));return out; }
void destroy(const ComplexMatrix& matrix,SolverMemory memory) { if(!matrix.vector_reference)memory.put(matrix.data); }
}
void update_frequency_geometry(double frequency,const TechnologyMetal* metals,int metal_count,double alpha,double beta,
    FrequencyGeometryLimits& limits,bool print,const AnalysisReportRuntime& r) {
    const long double f=load(frequency),wavelength=300000000.L/(4.L*f);
    const auto& metal=metals[metal_count-1];
    const long double product=multiply_memory(load(metal.thickness),metal.resistance);
    const long double depth=x87::sqrt((product*0x1.0c6f7a0b5ed8dp-20L)/(f*0x1.08ef4a5975084p-18L));
    const double length=binary64((load(alpha)*wavelength)*1000000.L),width=binary64((load(beta)*depth)*1000000.L);
    limits.max_length=limits.capacitance_max_length=length;
    limits.max_width=limits.max_thickness=limits.capacitance_max_width=width;
    if(print) {
        r.format(r.message,"\nlambda = %5.2lf, delta = %5.2lf",binary64(wavelength*1000000.L),binary64(depth*1000000.L));r.warnings.emit(r.message);
        r.format(r.message,"\nmaxL = %5.2lf, maxT = %5.2lf, maxW = %5.2lf",limits.max_length,limits.max_thickness,limits.max_width);r.warnings.emit(r.message);
    }
}
void report_inductance_matrix(const GeometryShape& source,double frequency,const char* filename,GeometryShape*& shapes,
    const InductanceMatrixReportOptions& o) {
    const auto memory=o.geometry.memory;const auto& r=o.report;
    auto* clone=clone_geometry_shape(shapes,source,o.discretization,o.geometry);ComplexMatrix matrix{};
    auto options=o.inductance;options.impedance=true;
    if(!solve_inductance(clone->filaments,matrix,frequency,options,o.eddy,memory,r.warnings)) {
        r.warnings.emit("Error:  Could not generate inductance matrix.");remove_geometry_shape(shapes,clone,memory);destroy(matrix,memory);return;
    }
    remove_geometry_shape(shapes,clone,memory);void* file=nullptr;
    if(filename) { file=r.open(filename,"w",r.user);if(!file)r.warnings.emit("Error:  Unable to open file."); }
    const int n=matrix.rows;auto* rows=static_cast<double*>(memory.arrays.get(static_cast<unsigned>(n)*8u));
    if(!rows)r.warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    double real=0,imag=0;
    for(int i=0;i<n;++i) {
        rows[i]=0;
        for(int j=0;j<n;++j) {
            const auto z=matrix.view().at(i,j);real=binary64(add_memory(load(real),z.real));imag=binary64(add_memory(load(imag),z.imag));
            rows[i]=binary64(add_memory(load(rows[i]),z.real));
            if(file)r.file_format(file,"\n%d\t%d\t%10.20lf\t%10.20lf",i,j,z.real,z.imag);
        }
    }
    if(file) { for(int i=0;i<n;++i)r.file_format(file,"\n%8.5lg",rows[i]);r.close(file,r.user); }
    const long double omega=multiply_memory(0x1.921fb54442d18p+2L,frequency);
    r.format(r.message,"L = %10.4lg\t\tR = %10.4f",binary64(divide_memory(omega,imag)),real);r.warnings.emit(r.message);
    memory.put(rows);destroy(matrix,memory);
}
}
