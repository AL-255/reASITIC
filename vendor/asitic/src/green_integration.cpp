#include "asitic/green_integration.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
using x87::binary64;
constexpr char fourier_error[]="\nError:  Quadpack DQAWF function did not converge.";
constexpr char arguments[]="\nCalled with:  freq = %lg, sep = %lg, h = %lg";
constexpr char max_cycles[]="\nMax no. of cycles exceeded.";
constexpr char bad_table[]="\n\tThe extrapolation table not good.";
constexpr char invalid[]="\n\tThe input is invalid.";
constexpr char bad_integrand[]="\n\tBad integrand behavior.";
constexpr char unknown[]="\n\tAn unknown error=%d occurred.";
constexpr char infinite_error[]="\nError:  Quadpack DQAGI function did not converge.";
constexpr char cycle_short_1[]="\nCycle %d:  max no. of sub-divs acheived";
constexpr char cycle_short_2[]="\nCycle %d:  round-off error";
constexpr char cycle_short_3[]="\nCycle %d:  exrememely bad behavior here";
constexpr char cycle_short_4[]="\nCycle %d:  no convergence due to roundoff in extrapolation procedure";
constexpr char cycle_short_5[]="\nCycle %d:  slow convergence or divergence";
constexpr char cycle_long_1[]="\nThe maximum number of subdivisions (=(leniw-limlst)/2 has been achieved on the %d th cycle.";
constexpr char cycle_long_2[]="\nOccurrence of roundoff error is detected and prevents the tolerance imposed on the %d th cycle, from being achieved on this cycle.";
constexpr char cycle_long_3[]="\nExtremely bad integrand behaviour occurs at some points of the %d th cycle.";
constexpr char cycle_long_4[]="\nThe integration procedure over the %d th cycle does not converge (to within the required accuracy) due to roundoff in the extrapolation procedure invoked on this cycle. it is assumed that the result on this interval is the best which can be obtained.";
constexpr char cycle_long_5[]="\nThe integral over the %d th cycle is probably divergent or slowly convergent. it must be noted that divergence can occur with any other value of iwork(k).";
constexpr char infinite_status_1[]="\n\tMaximum number of subdivisions allowed\n\thas been achieved. one can allow more\n\tsubdivisions by increasing the value of\n\tlimit (and taking the according dimension\n\tadjustments into account). however, if\n\tthis yields no improvement it is advised\n\tto analyze the integrand in order to\n\tdetermine the integration difficulties. if\n\tthe position of a local difficulty can be\n\tdetermined (e.g. singularity,\n\tdiscontinuity within the interval) one\n\twill probably gain from splitting up the\n\tinterval at this point and calling the\n\tintegrator on the subranges. if possible,\n\tan appropriate special-purpose integrator\n\tshould be used, which is designed for\n\thandling the type of difficulty involved.";
constexpr char infinite_status_2[]="\n\tThe occurrence of roundoff error is\n\tdetected, which prevents the requested\n\ttolerance from being achieved.\n\tthe error may be under-estimated.";
constexpr char infinite_status_3[]="\n\tExtremely bad integrand behaviour occurs\n\tat some points of the integration interval.";
constexpr char infinite_status_4[]="\n\tThe algorithm does not converge.\n\troundoff error is detected in the\n\textrapolation table.\n\tit is assumed that the requested tolerance\n\tcannot be achieved, and that the returned\n\tresult is the best which can be obtained.";
constexpr char infinite_status_5[]="\n\tThe integral is probably divergent, or\n\tslowly convergent. it must be noted that\n\tdivergence can occur with any other value\n\tof ier.";
constexpr char infinite_status_6[]="\n\tThe input is invalid, because\n\t(epsabs.le.0 and\n\t epsrel.lt.max(50*rel.mach.acc.,0.5d-28))\n\t or limit.lt.1 or leniw.lt.limit*4.\n\tresult, abserr, neval, last are set to\n\tzero. exept when limit or leniw is\n\tinvalid, iwork(1), work(limit*2+1) and\n\twork(limit*3+1) are set to zero, work(1)\n\tset to a and work(limit+1) to b.";
GreenIntegrationState* active_state=nullptr;
GreenIntegrationRuntime* active_runtime=nullptr;
struct CallbackScope {
    GreenIntegrationState* previous_state;
    GreenIntegrationRuntime* previous_runtime;
    CallbackScope(GreenIntegrationState& state,GreenIntegrationRuntime& runtime)
        :previous_state(active_state),previous_runtime(active_runtime) {
        active_state=&state;active_runtime=&runtime;
    }
    ~CallbackScope() { active_state=previous_state;active_runtime=previous_runtime; }
};
long double kernel(const double* point) {
    auto& s=*active_state;auto& r=*active_runtime;
    if(r.observe)r.observe(r.observer,*point);
    if(r.integrand)return r.integrand(point);
    if(s.kernel==GreenKernel::reflection)return green_reflection_kernel(*point,s.height);
    return green_oscillating_kernel(*point,s.frequency,s.layers,s.height,s.separation);
}
void configure(GreenIntegrationState& state,double frequency,double separation,double height,double z_offset) {
    state.frequency=frequency;state.separation=separation;state.height=height;state.z_offset=z_offset;
}
}
long double green_integrate_infinite(double frequency,double separation,double height,double z_offset,
    GreenIntegrationState& state,GreenIntegrationWorkspace& workspace,GreenIntegrationRuntime runtime) {
    configure(state,frequency,separation,height,z_offset);
    CallbackScope scope(state,runtime);
    double sample=0;
    for(;;) {
        kernel(&sample);
        const long double next=(long double)sample+0x1.a36e2eb1c432dp-14;
        sample=binary64(next);
        if(!(next<0x1.47ae147ae147bp-7))break;
    }
    quadpack::IntegralResult result;
    quadpack::integrate_infinite_packed(kernel,0,1,0,0x1.0c6f7a0b5ed8dp-20,200,
        {workspace.real,800,workspace.integer,200},result,runtime.diagnostic);
    if(result.status && runtime.print) {
        runtime.print(infinite_error);
        runtime.print(arguments,state.frequency,state.separation,state.height);
        const char* const messages[]={infinite_status_1,infinite_status_2,infinite_status_3,
            infinite_status_4,infinite_status_5,infinite_status_6};
        if(result.status>=1 && result.status<=6)runtime.print(messages[result.status-1]);
    }
    return result.integral;
}
long double green_integrate_fourier(double frequency,double separation,double height,double z_offset,
    GreenIntegrationState& state,GreenIntegrationWorkspace& workspace,GreenIntegrationRuntime runtime) {
    configure(state,frequency,separation,height,z_offset);
    CallbackScope scope(state,runtime);
    quadpack::FourierResult result{};
    quadpack::integrate_fourier_packed(kernel,0,separation,1,0x1.47ae147ae147bp-7,1000,50,
        {workspace.real,5250,workspace.integer,2000},result,workspace.history,runtime.math,runtime.diagnostic);
    if(result.status && runtime.print) {
        runtime.print(fourier_error);
        runtime.print(arguments,state.frequency,state.separation,state.height);
        if(result.status==4)runtime.print(bad_table);
        else if(result.status==6)runtime.print(invalid);
        else if(result.status==1 || result.status==7) {
            runtime.print(result.status==1?max_cycles:bad_integrand);
            const char* const short_messages[]={cycle_short_1,cycle_short_2,cycle_short_3,cycle_short_4,cycle_short_5};
            const char* const long_messages[]={cycle_long_1,cycle_long_2,cycle_long_3,cycle_long_4,cycle_long_5};
            for(int i=0;i<result.cycles;++i) {
                const int status=workspace.integer[i];
                if(status>=1 && status<=5)runtime.print(result.status==1?short_messages[status-1]:long_messages[status-1],i);
            }
        } else runtime.print(unknown,result.status);
    }
    return result.integral;
}
long double green_integral(double frequency,double separation,double height,double z_offset,
    GreenIntegrationState& state,GreenIntegrationWorkspace& workspace,GreenIntegrationRuntime runtime) {
    const long double integral=x87::abs(separation)<0x1.b7cdfd9d7bdbbp-34
        ?green_integrate_infinite(frequency,separation,height,z_offset,state,workspace,runtime)
        :green_integrate_fourier(frequency,separation,height,z_offset,state,workspace,runtime);
    return (-integral*0x1.515370f99f6cbp-20)*state.frequency;
}
}
