#include "asitic/extended_unfolded.hpp"
#include "asitic/legacy_complex.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& x) { long double out;asm("fldl %1":"=t"(out):"m"(x));return out; }
void copy(ComplexValue& dst,const ComplexValue& src) { __builtin_memcpy(&dst,&src,sizeof(dst)); }
void destroy(const ComplexMatrix& m,SolverMemory memory) { if(!m.vector_reference)memory.put(m.data); }
constexpr long double two_pi=0x1.921fb54442d18p+2L,femto=0x1.203af9ee75616p-50L;
}
bool solve_capacitive_analysis(const GeometryShape& shape,const GeometryShape& ground,double frequency,
    CapacitiveAnalysisState& state,const CapacitiveAnalysisOptions& options,SolverMemory memory) {
    ComplexMatrix cap{};auto cap_options=options.capacitance;cap_options.verbose=true;
    if(!solve_capacitance(shape.filaments,ground.filaments,cap,frequency,state.capacitance,cap_options,memory)) {
        options.warnings.emit("Error:  Could not generate capacitance matrix.");destroy(cap,memory);return false;
    }
    const auto* bytes=shape.filaments.prefix;
    if(*reinterpret_cast<const int*>(bytes+0x50)==4) {
        ComplexValue a,b,d;copy(a,cap.data[0]);copy(b,cap.data[1]);copy(d,cap.data[3]);
        const long double br=load(b.real),bi=load(b.imag);
        state.ports.y22={x87::binary64(load(a.real)+br),x87::binary64(load(a.imag)+bi)};
        state.ports.y11={x87::binary64(load(d.real)+br),x87::binary64(load(d.imag)+bi)};
        state.ports.y21={x87::binary64(-br),x87::binary64(-bi)};copy(state.ports.y12,state.ports.y21);
        if(options.print) {
            const double ra=legacy::divide(1.,a).real,rd=legacy::divide(1.,d).real;
            const ComplexValue reciprocal=legacy::divide(1.,b);
            const int first=*reinterpret_cast<const int*>(bytes+0x68),second=*reinterpret_cast<const int*>(bytes+0x6c);
            const long double resistance=(load(options.metals[first].resistance)+load(options.metals[second].resistance))
                *load(*reinterpret_cast<const double*>(bytes+0x70))/(3.L*load(*reinterpret_cast<const double*>(bytes+0x88)))+load(reciprocal.real);
            const double r=x87::binary64(resistance);
            const long double omega=(two_pi*load(frequency))*femto;
            const double ca=x87::binary64(load(a.imag)/omega),cd=x87::binary64(load(d.imag)/omega),cb=x87::binary64(load(b.imag)/omega);
            const double qa=x87::binary64(load(state.ports.y22.imag)/load(state.ports.y22.real));
            const double qd=x87::binary64(load(state.ports.y11.imag)/load(state.ports.y11.real));
            const long double ar=load(a.real),ai=load(a.imag),dr=load(d.real),di=load(d.imag);
            const ComplexValue sum{x87::binary64(ar+dr),x87::binary64(ai+di)};
            const ComplexValue product{x87::binary64(ar*dr-ai*di),x87::binary64(ar*di+dr*ai)};
            const auto parallel=legacy::divide(product,sum);
            const double q=x87::binary64((load(parallel.imag)+load(b.imag))/(load(parallel.real)+load(b.real)));
            const double ghz=x87::binary64(0x1.12e0be826d695p-30L*load(frequency));
            char message[1024];
            if(options.format) {
                options.format(message,"Narrow-band model at f = %5.2lf GHz:  Q=%lf,%4.3lf,%4.3lf",ghz,qa,qd,q);options.warnings.emit(message);
                options.format(message,"C=%4.3lf R=%4.3lf Cs1 = %4.3lf Rs1 = %4.3lf Cs2 = %4.3lf Rs2 = %4.3lf",cb,r,ca,ra,cd,rd);options.warnings.emit(message);
            }
        }
    } else if(static_cast<unsigned>(cap.rows)<=9u) {
        options.warnings.emit("Capacitance matrix:");
        for(unsigned row=0;row<static_cast<unsigned>(cap.rows);++row)for(unsigned column=row;column<static_cast<unsigned>(cap.columns);++column) {
            const auto& cell=cap.data[column*cap.stride+row];
            const double real=legacy::divide(1.,cell).real;
            const auto temporary=legacy::divide(1.,cell);
            const double capacitance=x87::binary64(-1.L/(((two_pi*load(frequency))*femto)*load(temporary.imag)));
            const double imag=legacy::divide(1.,cell).imag;
            if(options.table)options.table("\nZ(%d,%d) = %7.4g + j %7.4g (%7.4f fF)",row,column,real,imag,capacitance);
        }
    }
    destroy(cap,memory);return true;
}
}
