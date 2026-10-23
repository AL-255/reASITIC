#include "asitic/quadrature.hpp"

namespace asitic::quadpack {
namespace {
void report(Diagnostic diagnostic,const char* message,int status) {
    if(status && diagnostic.emit)diagnostic.emit(diagnostic.context,message,26,status,status==6?1:0);
}
}
void integrate_infinite_packed(Integrand f,double bound,int direction,double absolute_tolerance,
    double relative_tolerance,int limit,PackedWorkspace w,IntegralResult& result,Diagnostic diagnostic) {
    result={0,0,0,6,0};
    // Unsigned products preserve the original 32-bit workspace-size arithmetic.
    if(limit>0 && w.real_length>=static_cast<int>(4u*static_cast<unsigned>(limit)))
        result=integrate_infinite(f,bound,direction,absolute_tolerance,relative_tolerance,
            {w.real,w.real+limit,w.real+2*limit,w.real+3*limit,w.integer,limit});
    report(diagnostic,"abnormal return from dqagi",result.status);
}
void integrate_fourier_packed(Integrand f,double start,double frequency,int selector,double absolute_tolerance,
    int cycle_limit,int moment_rows,PackedWorkspace w,FourierResult& result,
    double* history,Trigonometry math,Diagnostic diagnostic) {
    result.integral=0;result.error=0;result.evaluations=0;result.status=6;
    if(cycle_limit>2 && w.integer_length>=static_cast<int>(static_cast<unsigned>(cycle_limit)+2u)
        && moment_rows>0 && w.real_length>=static_cast<int>(2u*static_cast<unsigned>(w.integer_length)
            +25u*static_cast<unsigned>(moment_rows))) {
        const int limit=(w.integer_length-cycle_limit)/2;
        result=integrate_fourier(f,start,frequency,selector,absolute_tolerance,
            {{{w.real+2*cycle_limit,w.real+2*cycle_limit+limit,w.real+2*cycle_limit+2*limit,
                w.real+2*cycle_limit+3*limit,w.integer+cycle_limit,limit},w.integer+cycle_limit+limit},
             w.real,w.real+cycle_limit,w.integer,cycle_limit,w.real+2*cycle_limit+4*limit,moment_rows,history},math);
    }
    report(diagnostic,"abnormal return from dqawf",result.status);
}
}
