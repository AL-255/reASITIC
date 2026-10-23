#pragma once
#include "asitic/green.hpp"
#include "asitic/quadrature.hpp"

namespace asitic {
enum class GreenKernel { oscillating,reflection };
struct GreenIntegrationState {
    GreenLayers layers;
    double frequency,separation,height,z_offset;
    GreenKernel kernel;
};
struct GreenIntegrationWorkspace {
    double real[5250];
    int integer[2000];
    // Initial bytes are observable on some Fourier nonconvergence paths.
    double history[3];
};
struct GreenIntegrationRuntime {
    quadpack::Trigonometry math;
    quadpack::Diagnostic diagnostic;
    int (*print)(const char*,...);
    // Optional instrumentation, called before each kernel evaluation.
    void (*observe)(void*,double);
    void* observer;
    // The original global kernel is configurable. Null selects state.kernel;
    // a nonnull value supplies another integrand, as with the original pointer.
    quadpack::Integrand integrand;
};
// These wrappers update the explicit counterparts of the four original globals.
// Calls use a scoped active callback context; like the original global API they
// require external serialization between threads. The state retains final values.
long double green_integrate_infinite(double frequency,double separation,double height,double z_offset,
    GreenIntegrationState& state,GreenIntegrationWorkspace& workspace,GreenIntegrationRuntime runtime);
long double green_integrate_fourier(double frequency,double separation,double height,double z_offset,
    GreenIntegrationState& state,GreenIntegrationWorkspace& workspace,GreenIntegrationRuntime runtime);
// Original selector and final -integral * binary64(mu0) * current frequency.
long double green_integral(double frequency,double separation,double height,double z_offset,
    GreenIntegrationState& state,GreenIntegrationWorkspace& workspace,GreenIntegrationRuntime runtime);
}
