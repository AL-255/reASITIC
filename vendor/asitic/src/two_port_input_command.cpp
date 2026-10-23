#include "asitic/two_port_input_command.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double input_load(const double& value) { long double out;asm volatile("fldl %1" : "=t"(out) : "m"(value));return out; }
double input_number(const CommandArgument& value) { long double out;asm volatile("flds %1" : "=t"(out) : "m"(value.value));return x87::binary64(out); }
double input_capacitor(const double& frequency,const CommandArgument& capacitance) {
    long double product;asm volatile("fmuls %2" : "=t"(product) : "0"(input_load(frequency)),"m"(capacitance.value));
    return x87::binary64(0x1.921fb54442d18p+2L*product);
}
void input_add(double& cell,const double& value) { long double out;asm volatile("fldl %1; faddl %2" : "=t"(out) : "m"(cell),"m"(value));cell=x87::binary64(out); }
void input_subtract(double& cell,const double& value) { long double out;asm volatile("fldl %1; fsubl %2" : "=t"(out) : "m"(cell),"m"(value));cell=x87::binary64(out); }
}
bool execute_two_port_input_command(int id,CommandArguments& arguments,TwoPortAdmittance& y,const double& frequency,
    const AnalysisReportRuntime& runtime) {
    if(id!=537)return false;
    const auto& a=arguments.arguments;int count=static_cast<int>(arguments.command.value),port=1;
    double real=50.,imag=0.;if(count>0)real=input_number(a[0]);if(count>1)imag=input_number(a[1]);
    if(count>2) {
        if(a[2].value==2)port=2;
        else if(a[2].value!=1) { runtime.warnings.emit("Warning:  Must specify port 1 or 2.");count=static_cast<int>(arguments.command.value); }
    }
    const double zero=0.;double first=0.,second=0.;
    if(count>3) { first=input_capacitor(frequency,a[3]);count=static_cast<int>(arguments.command.value); }
    if(count>4)second=input_capacitor(frequency,a[4]);
    input_add(y.y22.real,zero);input_add(y.y22.imag,first);input_add(y.y11.real,zero);input_add(y.y11.imag,second);
    // Native performs another floating load/store of each termination cell.
    const ComplexValue load{x87::binary64(input_load(real)),x87::binary64(input_load(imag))};
    const auto impedance=terminated_two_port_impedance(y,load,port);
    input_subtract(y.y22.real,zero);input_subtract(y.y22.imag,first);input_subtract(y.y11.real,zero);input_subtract(y.y11.imag,second);
    runtime.format(runtime.message,"Zin = %5.3lf + j %5.2lf",impedance.real,impedance.imag);runtime.warnings.emit(runtime.message);return true;
}
}
