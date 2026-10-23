#include "asitic/analysis_commands.hpp"
#include "asitic/numerics.hpp"
#include "x87.hpp"
namespace asitic {

namespace {
long double ac_ind_load(const double& value) { long double result;asm volatile("fldl %1" : "=t"(result) : "m"(value));return result; }
long double ac_ind_subtract(const double& first,const double& second) { long double result;asm volatile("fldl %1; fsubl %2" : "=t"(result) : "m"(first),"m"(second));return result; }
long double ac_ind_multiply(long double first,const double& second) { long double result;asm volatile("fmull %2" : "=t"(result) : "0"(first),"m"(second));return result; }
long double ac_ind_add(long double first,const double& second) { long double result;asm volatile("faddl %2" : "=t"(result) : "0"(first),"m"(second));return result; }
const ConductorSegment* ac_ind_next(const ConductorSegment* p) { return reinterpret_cast<const ConductorSegment*>(p->next_address); }
void ac_ind_translate(ConductorSegment& segment,const ShapeFilamentList& shape,bool subtract) {
    const auto* origin=reinterpret_cast<const double*>(shape.prefix+0x54);const unsigned indexes[]={0,1,3,4};
    for(unsigned index:indexes) {
        long double result;
        if(subtract)asm volatile("fldl %1; fsubl %2" : "=t"(result) : "m"(segment.endpoints[index]),"m"(origin[index%3]));
        else asm volatile("fldl %1; faddl %2" : "=t"(result) : "m"(segment.endpoints[index]),"m"(origin[index%3]));
        segment.endpoints[index]=x87::binary64(result);
    }
}
int ac_ind_mutual(const ConductorSegment& first,const ConductorSegment& second,double& mutual,WarningSink warnings) {
    const char* message=nullptr;const int result=segment_mutual(first.endpoints,second.endpoints,&mutual,&message);if(message)warnings.emit(message);return result;
}
}
long double shape_inductance_estimate(const ShapeFilamentList& shape,const ResistanceTechnology& technology,WarningSink warnings) {
    constexpr long double scale=0x1.a36e2eb1c432dp-14L,epsilon=0x1.b7cdfd9d7bdbbp-34L;
    long double self=0,mutual=0;double pair=0;
    for(auto* segment=shape.first;segment;segment=ac_ind_next(segment)) {
        long double length,width,thickness;
        if(segment->metal<technology.metal_count) {
            const long double dx=ac_ind_subtract(segment->endpoints[3],segment->endpoints[0]),dy=ac_ind_subtract(segment->endpoints[4],segment->endpoints[1]),dz=ac_ind_subtract(segment->endpoints[5],segment->endpoints[2]);
            length=x87::sqrt((dx*dx+dy*dy)+dz*dz)*scale;
            width=ac_ind_load(segment->width)*scale;thickness=ac_ind_load(technology.metals[segment->metal].thickness)*scale;
        } else {
            length=x87::abs(ac_ind_subtract(segment->endpoints[5],segment->endpoints[2]))*scale;
            const auto* via=reinterpret_cast<const unsigned char*>(technology.vias+segment->metal-technology.metal_count);
            width=ac_ind_multiply(scale*segment->width_count,*reinterpret_cast<const double*>(via+0xa0));
            thickness=ac_ind_multiply(scale*segment->thickness_count,*reinterpret_cast<const double*>(via+0xa8));
        }
        if(length>epsilon) {
            const long double twice=length+length,sum=width+thickness;
            const long double logarithm=x87::log(twice/sum)+0x1.004039abf3387p-1L;
            const long double term=twice*((sum/(3.L*length))+logarithm);self=term+self;
        }
    }
    for(auto* first=shape.first;first;first=ac_ind_next(first))if(first->metal<technology.metal_count)
        for(auto* second=ac_ind_next(first);second;second=ac_ind_next(second))if(second->metal<technology.metal_count) {
            if(ac_ind_mutual(*first,*second,pair,warnings)==-1) { warnings.emit("101:  Encountered Errors");return 0; }
            mutual=ac_ind_add(mutual,pair);
        }
    return (mutual+mutual)+self;
}
long double shape_coupling_estimate(const ShapeFilamentList& first,const ShapeFilamentList& second,double& mutual,
    const ResistanceTechnology& technology,WarningSink warnings) {
    const double l1=x87::binary64(shape_inductance_estimate(first,technology,warnings)),l2=x87::binary64(shape_inductance_estimate(second,technology,warnings));
    double sum=0,pair=0;
    for(auto* a=first.first;a;a=ac_ind_next(a))if(a->metal<technology.metal_count)
        for(auto* b=second.first;b;b=ac_ind_next(b))if(b->metal<technology.metal_count) {
            ac_ind_translate(*const_cast<ConductorSegment*>(a),first,false);ac_ind_translate(*const_cast<ConductorSegment*>(b),second,false);
            if(ac_ind_mutual(*a,*b,pair,warnings)==-1) { warnings.emit("102:  Encountered Errors");return 0; }
            sum=x87::binary64(ac_ind_add(ac_ind_load(sum),pair));
            ac_ind_translate(*const_cast<ConductorSegment*>(a),first,true);ac_ind_translate(*const_cast<ConductorSegment*>(b),second,true);
        }
    // Native writes the output with an x87 load/store before threshold tests.
    asm volatile("fldl %1; fstpl %0" : "=m"(mutual) : "m"(sum) : "st","memory");
    constexpr long double epsilon=0x1.b7cdfd9d7bdbbp-34L;
    if(!(epsilon<l1) || !(epsilon<l2))return 0;
    const long double denominator=x87::sqrt(ac_ind_multiply(ac_ind_load(l1),l2));
    long double result;asm volatile("fdivrl %2" : "=t"(result) : "0"(denominator),"m"(sum));return result;
}

namespace {
long double ac_res_load(const double& value) { long double out;asm volatile("fldl %1" : "=t"(out) : "m"(value));return out; }
long double ac_res_quantize(long double midpoint,const double& step) {
    const long double half=0.5L*ac_res_load(step);
    long double reciprocal;asm volatile("fld1; fdivl %1" : "=t"(reciprocal) : "m"(step));
    const long double units=(midpoint+half)*reciprocal;long double rounded;unsigned short saved,down;
    asm volatile("fnstcw %0" : "=m"(saved));down=(saved&~0x0c00u)|0x0400u;
    asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(rounded) : "0"(units),"m"(down),"m"(saved));
    const double spilled=x87::binary64(rounded);long double out;asm volatile("fldl %1; fmull %2" : "=t"(out) : "m"(spilled),"m"(step));return out;
}
}
long double find_self_resonant_frequency(const GeometryShape& shape,double first,double second,double step,
    int differential,int fast,int ignored_iteration_limit,const GeometryShape& ground,const SelfResonanceRuntime& r) {
    (void)ignored_iteration_limit;
    long double low=ac_res_load(first),high=ac_res_load(second);
    if(high<low) { const long double old=low;low=high;high=old; }
    double lower=x87::binary64(low),upper=x87::binary64(high);
    long double midpoint=(high-low)*0.5L+ac_res_load(lower);
    r.analyze(shape,x87::binary64(midpoint),fast,ground,r.user);
    for(int iteration=0;;) {
        ++iteration;const long double imag=two_port_imaginary_impedance(*r.admittance,differential);
        if(0.L<imag)lower=x87::binary64(midpoint);else upper=x87::binary64(midpoint);
        constexpr long double ghz=0x1.12e0be826d695p-30L;
        r.progress("\nf1=%5.2lf f2=%5.2lf f_g=%5.2lf Zin.i=%lf",x87::binary64(ac_res_load(lower)*ghz),x87::binary64(ac_res_load(upper)*ghz),x87::binary64(midpoint*ghz),x87::binary64(imag));
        r.flush(r.output);
        long double next;
        if(x87::abs(imag)<0x1.47ae147ae147bp-7L)next=midpoint;
        else {
            next=(ac_res_load(upper)-ac_res_load(lower))*0.5L+ac_res_load(lower);
            if(fast)next=ac_res_quantize(next,step);
            if(!(x87::abs(next-midpoint)<ac_res_load(step))) {
                r.analyze(shape,x87::binary64(next),fast,ground,r.user);midpoint=next;
                if(iteration<100)continue;
                r.report.warnings.emit("Could not find self resonant frequency in given range.");return -1.L;
            }
        }
        r.report.format(r.report.message,"Self Resonant Frequency Occurs at %5.2lf (GHz).",x87::binary64(next*ghz));r.report.warnings.emit(r.report.message);return next;
    }
}
void report_input_impedance(const TwoPortAdmittance& admittance,bool differential,int port,const AnalysisReportRuntime& r) {
    const auto z=two_port_impedance(admittance,differential,port);
    r.format(r.message,"Zin = %5.3lf + j %5.3lf",z.real,z.imag);r.warnings.emit(r.message);
}

namespace {
long double ac_basic_number(const CommandArgument& value) { long double out;asm volatile("flds %1":"=t"(out):"m"(value.value));return out; }
long double ac_basic_multiply(long double value,const double& other) { long double out;asm volatile("fmull %2":"=t"(out):"0"(value),"m"(other));return out; }
const GeometryShape& ac_basic_shape(const CommandArgument& value) { return *reinterpret_cast<const GeometryShape*>(value.value); }
bool ac_basic_same_name(const GeometryShape& first,const GeometryShape& second) {
    const auto* a=reinterpret_cast<const char*>(&first);const auto* b=reinterpret_cast<const char*>(&second);while(*a && *a==*b) { ++a;++b; }return *a==*b;
}
void ac_basic_analytical(const GeometryShape& shape,double frequency,SimpleAnalysisCommandContext& c) {
    auto& context=*c.twoport;auto options=context.analytical_options;
    options.kind=*reinterpret_cast<const int*>(shape.filaments.prefix+0x50);options.sides=*reinterpret_cast<const int*>(shape.filaments.prefix+0x64);
    options.spacing=*reinterpret_cast<const double*>(shape.filaments.prefix+0x90);options.turns=*reinterpret_cast<const double*>(shape.filaments.prefix+0x98);
    const bool result=c.backends.analytical(shape.filaments,frequency,options,*context.analytical_history,*context.analytical_result,c.report.warnings);
    *c.admittance=context.analytical_result->admittance;*c.pi=context.analytical_result->pi;
    if(result && context.analytical_report)context.analytical_report(context.analytical_result->pi,frequency,context.analytical_report_user);
}
void ac_basic_configure(double frequency,SimpleAnalysisCommandContext& c) {
    auto& context=*c.twoport;
    if(context.automatic_cells)update_frequency_geometry(frequency,reinterpret_cast<const TechnologyMetal*>(context.analytical_options.metals),context.analytical_options.metal_count,
        context.alpha,context.beta,context.limits,context.progress!=0,context.report);
}
bool ac_basic_execute(int id,CommandArguments& arguments,SimpleAnalysisCommandContext& c) {
    if(id!=500 && id!=501 && id!=502 && id!=503 && id!=504 && id!=505 && id!=527 && id!=531)return false;
    const auto& a=arguments.arguments;const int count=static_cast<int>(arguments.command.value);const auto& first=ac_basic_shape(a[0]);const auto& r=c.report;
    if(id==500) {
        const auto& second=ac_basic_shape(a[1]);
        if(ac_basic_same_name(first,second))r.warnings.emit("Error:  Cannot find self-coupling.  Use inductance instead.");
        else {
            double mutual=c.history.coupling_mutual;
            const long double coefficient=c.backends.coupling(first.filaments,second.filaments,mutual,c.resistance,r.warnings);c.history.coupling_mutual=mutual;
            r.format(r.message,"Coupling coefficient of %s and %s: k = %7.2lf and M = %7.2lf (nH).",reinterpret_cast<const char*>(&first),reinterpret_cast<const char*>(&second),x87::binary64(coefficient),mutual);r.warnings.emit(r.message);
        }
    } else if(id==501 || id==502) {
        const long double frequency=count>1?ac_basic_number(a[1]):-1.L;
        if(0x1.b7cdfd9d7bdbbp-34L<frequency) {
            ac_basic_analytical(first,x87::binary64(frequency),c);
            const double ghz=x87::binary64(frequency*0x1.12e0be826d695p-30L);
            if(id==502)r.format(r.message,"%s: at %lf GHz L = %lg (nH).",reinterpret_cast<const char*>(&first),ghz,x87::binary64(ac_basic_multiply(1000000000.L,c.pi->inductance)));
            else r.format(r.message,"%s: at %lf GHz R = %lf.",reinterpret_cast<const char*>(&first),ghz,c.pi->resistance);
        } else {
            if(id==502)r.format(r.message,"Inductance of %s = %lg (nH).",reinterpret_cast<const char*>(&first),x87::binary64(c.backends.inductance(first.filaments,c.resistance,r.warnings)));
            else r.format(r.message,"Resistance of %s = %lf (Ohms).",reinterpret_cast<const char*>(&first),x87::binary64(c.backends.resistance_dc(first.filaments,c.resistance)));
        }
        r.warnings.emit(r.message);
    } else if(id==527) {
        const long double frequency=ac_basic_number(a[1]);const long double resistance=c.backends.resistance_hf(first.filaments,x87::binary64(frequency),c.resistance);
        r.format(r.message,"%s: at %lf GHz R = %lf.",reinterpret_cast<const char*>(&first),x87::binary64(frequency*0x1.12e0be826d695p-30L),x87::binary64(resistance));r.warnings.emit(r.message);
    } else if(id==504) {
        const double frequency=x87::binary64(ac_basic_number(a[1]));
        const long double inductance=c.backends.inductance(first.filaments,c.resistance,r.warnings);
        const long double numerator=ac_basic_multiply(0x1.afc6f70213be5p-28L,frequency)*inductance;
        const long double resistance=c.backends.resistance_dc(first.filaments,c.resistance);
        r.format(r.message,"LF Extrapolated Q of %s = %lf.",reinterpret_cast<const char*>(&first),x87::binary64(numerator/resistance));r.warnings.emit(r.message);r.warnings.emit("Use \"Pi\" or \"Pi2\" for HF Q.");
    } else if(id==505)ac_basic_analytical(first,x87::binary64(ac_basic_number(a[1])),c);
    else if(id==503) {
        ac_basic_configure(x87::binary64(ac_basic_number(a[1])),c);
        const double frequency=x87::binary64(ac_basic_number(a[1]));const auto& device=ac_basic_shape(a[0]);const auto& ground=static_cast<int>(arguments.command.value)==2?device:ac_basic_shape(a[2]);
        auto& context=*c.twoport;auto options=*context.capacitive_options;options.print=true;options.capacitance.contacts.length_step=context.limits.capacitance_max_length;options.capacitance.contacts.width_step=context.limits.capacitance_max_width;
        c.backends.capacitive(device,ground,frequency,*context.capacitive_state,options,context.geometry.memory);*c.admittance=context.capacitive_state->ports;
    } else {
        const double frequency=x87::binary64(ac_basic_number(a[1]));ac_basic_configure(frequency,c);
        auto options=*c.matrix_options;const auto& limits=c.twoport->limits;
        options.discretization={limits.max_length,limits.capacitance_max_length};options.inductance.filaments.edge_cell_size=limits.max_width;options.inductance.thickness_cell=limits.max_thickness;
        c.backends.matrix_report(first,frequency,static_cast<int>(arguments.command.value)<3?nullptr:a[2].text,*c.shapes,options);
    }
    return true;
}
}

namespace {
long double ac_cmd_number(const CommandArgument& value) { long double out;asm volatile("flds %1" : "=t"(out) : "m"(value.value));return out; }
const GeometryShape& ac_cmd_shape(const CommandArgument& value) { return *reinterpret_cast<const GeometryShape*>(value.value); }
bool ac_cmd_equal(const char* text,const char* expected) { while(*text && *text==*expected) { ++text;++expected; }return *text==*expected; }
bool ac_cmd_contains(const char* text,const char* expected) { for(;*text;++text) { const char* a=text;const char* b=expected;while(*b && *a==*b) { ++a;++b; }if(!*b)return true; }return !*expected; }
void ac_cmd_grounded(const GeometryShape& shape,double frequency,int fast,const GeometryShape& ground,void* user) {
    auto& c=*static_cast<SimpleAnalysisCommandContext*>(user);auto& t=*c.twoport;const int kind=*reinterpret_cast<const int*>(shape.filaments.prefix+0x50);
    if(kind==4)t.capacitive_state->ports=*c.admittance;
    else if(fast) { t.analytical_result->admittance=*c.admittance;t.analytical_result->pi=*c.pi; }
    else { t.combined_state->ports=*c.admittance;t.combined_state->pi=*c.pi; }
    c.backends.grounded(shape,frequency,fast,ground,1,t);
    if(kind==4)*c.admittance=t.capacitive_state->ports;
    else if(fast) { *c.admittance=t.analytical_result->admittance;*c.pi=t.analytical_result->pi; }
    else { *c.admittance=t.combined_state->ports;*c.pi=t.combined_state->pi; }
}
int ac_cmd_fast_option(char* text,const char* warning,SimpleAnalysisCommandContext& c) {
    normalize_input_line(text,0x20,c.characters);
    if(ac_cmd_equal(text,"FAST"))return 1;
    if(ac_cmd_equal(text,"SLOW"))return 0;
    c.report.warnings.emit(warning);return 1;
}
}
bool execute_simple_analysis_command(int id,CommandArguments& arguments,SimpleAnalysisCommandContext& c) {
    if(ac_basic_execute(id,arguments,c))return true;
    auto& a=arguments.arguments;const int count=static_cast<int>(arguments.command.value);
    if(id==533 || id==535) {
        bool differential=false;int port=1,fast=1;
        if(count>0) {
            normalize_input_line(a[0].text,0x20,c.characters);const char mode=a[0].text[0];
            if(mode=='D')differential=true;
            else if(id==535 && mode=='X')port=2;
            else if(mode!='S')c.report.warnings.emit("Warning:  Did not understand argument.  Using single-ended default.");
        }
        const int parsed_count=static_cast<int>(arguments.command.value);
        const GeometryShape* shape=nullptr;const GeometryShape* ground=nullptr;double frequency;
        __builtin_memcpy(&frequency,&c.history.zin_frequency,8);
        if(parsed_count>1) { shape=&ac_cmd_shape(a[1]);ground=shape; }
        if(parsed_count>2) { frequency=x87::binary64(ac_cmd_number(a[2]));if(id==535)__builtin_memcpy(&c.history.zin_frequency,&frequency,8); }
        if(parsed_count>3)fast=ac_cmd_fast_option(a[3].text,id==533?"Warning:  Did not understand 4th argument.  Defaulting to fast.":"Did not understand 4th argument.  Defaulting to fast.",c);
        if(static_cast<int>(arguments.command.value)>4)ground=&ac_cmd_shape(a[4]);
        const int final_count=static_cast<int>(arguments.command.value);
        if((id==533 && final_count>2) || (id==535 && final_count>1))ac_cmd_grounded(*shape,frequency,fast,*ground,&c);
        if(id==533) { const double resistance=x87::binary64(c.backends.shunt(*c.pi,differential));c.report.format(c.report.message,"The shunt resistance is %lf Ohms.",resistance);c.report.warnings.emit(c.report.message); }
        else c.backends.input_impedance(*c.admittance,differential,port,c.report);
        return true;
    }
    if(id==534) {
        const auto& shape=ac_cmd_shape(a[0]);const GeometryShape* ground=&shape;double first=x87::binary64(ac_cmd_number(a[1])),second=x87::binary64(ac_cmd_number(a[2])),step=x87::binary64(ac_cmd_number(a[3]));
        if(second<first) { const double saved=first;first=second;second=saved; }
        if(step<0x1.b7cdfd9d7bdbbp-34L)step=0.1;
        int differential=0,fast=1,iterations=10;
        if(count>4) { const char mode=a[4].text[0];if(mode=='D' || mode=='d')differential=1;else if(mode!='S' && mode!='s')c.report.warnings.emit("Warning:  Cannot understand 5th argument, defaulting to Single-Ended."); }
        if(static_cast<int>(arguments.command.value)>5) {
            char* text=normalize_input_line(a[5].text,0x20,c.characters);
            if(!ac_cmd_contains(text,"FAST")) {
                text=normalize_input_line(a[5].text,0x20,c.characters);
                if(ac_cmd_contains(text,"SLOW"))fast=0;
                else c.report.warnings.emit("Warning:  Cannot understand 6th argument, defaulting to FAST.");
            }
        }
        if(static_cast<int>(arguments.command.value)>6)iterations=static_cast<int>(a[6].value);
        if(static_cast<int>(arguments.command.value)>7)ground=&ac_cmd_shape(a[7]);
        if(static_cast<unsigned>(iterations)>1000u) { c.report.warnings.emit("Max iterations out of range.  Using 10 by default.");iterations=10; }
        auto runtime=c.resonance;if(!runtime.analyze) { runtime.analyze=ac_cmd_grounded;runtime.user=&c;runtime.admittance=c.admittance; }runtime.report=c.report;
        c.backends.self_resonance(shape,first,second,step,differential,fast,iterations,*ground,runtime);return true;
    }
    return false;
}

}
