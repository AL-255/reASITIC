#include "asitic/port_output.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
inline void raw_copy(void* to,const void* from,unsigned n) { auto* a=static_cast<unsigned char*>(to);const auto* b=static_cast<const unsigned char*>(from);for(unsigned i=0;i<n;++i)a[i]=b[i]; }
inline unsigned word(const void* value,unsigned offset) { unsigned out;raw_copy(&out,static_cast<const unsigned char*>(value)+offset,4);return out; }
inline long double load(double value) { long double out;asm volatile("fldl %1":"=t"(out):"m"(value));return out; }
}
void write_two_port_header(void* file,int parameter,unsigned char polar,const PortOutputRuntime& runtime) {
    if(parameter==1 && !polar)runtime.print(file,"\n%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s","# GHZ S RI R 50\n! f (GHz)","S11r","S11i","S12r","S12i","S21r","S21i","S22r","S22i");
    else if(parameter==1 && polar)runtime.print(file,"\n%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s","# GHZ S MA R 50\n! f (GHz)","S11 mag","S11 phase","S12 mag","S12 phase","S21 mag","S21 phase","S22 mag","S22 phase");
    else if(parameter==2 && !polar)runtime.print(file,"\n%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s","# GHZ Y RI R 1\n! f (GHz)","Y11r","Y11i","Y12r","Y12i","Y21r","Y21i","Y22r","Y22i");
    else if(parameter==2 && polar)runtime.print(file,"\n%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s","# GHZ Y MA R 1\n! f (GHz)","Y11 mag","Y11 phase","Y12 mag","Y12 phase","Y21 mag","Y21 phase","Y22 mag","Y22 phase");
    else if(parameter==4)runtime.print(file,"\n%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s","# GHZ PI\n! f (GHz)","L (nH)","R (ohm)","C1 (fF)","C2 (fF)","R1s (ohm)","R2s (ohm)","Qin","Qout","Qdiff");
    else if(parameter==5)runtime.print(file,"\n%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s%-11.11s","# GHZ TRANS\n! f (GHz)","L1 (nH)","R1 (ohm)","L2 (nH)","R2 (ohm)","M  (nH)","k-factor","RG");
}
void write_three_port_header(void* file,int parameter,unsigned char polar,const PortOutputRuntime& runtime) {
    const int letter=(parameter==2?'Y':parameter==3?'Z':'S')&0x5b;
    const int first=polar?'m':'r',second=polar?'p':'i';
    for(int i=1;i<=3;++i) {
        runtime.print(file,"\n!");
        for(int j=1;j<=3;++j)runtime.print(file,"\t%c%d%d%c\t%c%d%d%c",letter,i,j,first,letter,i,j,second);
    }
}
void write_three_port_record(void* file,double frequency,int parameter,unsigned char polar,
    ThreePortOutputState& state,const PortOutputRuntime& runtime) {
    const auto* frequency_words=reinterpret_cast<const volatile unsigned*>(&frequency);
    const unsigned frequency_low=frequency_words[0],frequency_high=frequency_words[1];
    ComplexValue cells[9];
    if(parameter==1) {
        convert_three_port_scattering(state.impedance,state.scattering,runtime.memory,runtime.inverse);
        const unsigned order[]={4,5,5,1,2,2,7,8,8};
        const auto* values=reinterpret_cast<const ComplexValue*>(&state.scattering);
        for(unsigned i=0;i<9;++i)raw_copy(cells+i,values+order[i],16);
    } else if(parameter==2) {
        convert_three_port_admittance(state.impedance,state.admittance,runtime.memory,runtime.inverse);
        const unsigned order[]={4,5,5,1,2,2,7,8,8};
        const auto* values=reinterpret_cast<const ComplexValue*>(&state.admittance);
        for(unsigned i=0;i<9;++i)raw_copy(cells+i,values+order[i],16);
    } else if(parameter==3) {
        const unsigned order[]={0,2,2,3,5,5,6,8,8};
        const auto* values=reinterpret_cast<const ComplexValue*>(&state.impedance);
        for(unsigned i=0;i<9;++i)raw_copy(cells+i,values+order[i],16);
    } else {
        runtime.warnings.emit("Error:  Only support S, Y, or Z 3-Port output parameters");return;
    }
    cells[7]={0.,0.};
    if(polar)for(unsigned i=0;i<9;++i) {
        const long double real=load(cells[i].real),imag=load(cells[i].imag);
        const long double sum=real*real+imag*imag;
        const double magnitude=x87::binary64(x87::sqrt(sum));
        const double phase=x87::binary64(x87::atan2(imag,real));
        cells[0].real=magnitude;
        cells[0].imag=x87::binary64(load(phase)*0x1.ca5dc1a63c1f8p+5L);
    }
    runtime.print(file,"\n%-20.15e",frequency_low,frequency_high);
    for(unsigned row=0;row<3;++row) {
        const auto* z=cells+row*3;
        runtime.print(file,"\n%-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e  %-20.15e",
            word(z,0),word(z,4),word(z,8),word(z,12),word(z,16),word(z,20),word(z,24),word(z,28),word(z,32),word(z,36),word(z,40),word(z,44));
    }
}

void write_port_shape_summary(void* file,const GeometryShape& shape,const PortOutputRuntime& r) {
    r.print(file,"\n!# Spiral Data:\tName = <%s>",reinterpret_cast<const char*>(&shape));
    const int kind=static_cast<int>(word(&shape,0x50));
    const char* description=nullptr;
    switch(kind) {
        case 0:description=", Square Spiral Inductor";break;
        case 1:description=", Polygon Spiral Inductor";break;
        case 2:description=", Wire";break;
        case 3:description=", Transformer";break;
        case 4:description=", Capacitor";break;
        case 5:description=", Balun";break;
        case 10:description=", 3D Mirror Transformer";break;
        case 14:description=", Via";break;
        case 15:description=", Pad";break;
        case 16:description=", Symmetric Square Inductor";break;
        case 17:description=", Symmetric Polygon Inductor";break;
        case 18:description=", Multi-Metal Square Spiral Inductor";break;
        default:description=", User-defined Structure";break;
    }
    r.print(file,description);
    if(kind==1 || kind==17)r.print(file,", Radius = %5.2lf",word(&shape,0x80),word(&shape,0x84));
    else if(kind==2)r.print(file,"\n!# L1 = %5.2lf",word(&shape,0x70),word(&shape,0x74));
    else if(kind==0 || kind==3 || kind==4 || kind==5 || kind==10 || kind==14 || kind==15 || kind==16 || kind==18)
        r.print(file,"\n!# L1 = %5.2lf, L2 = %5.2lf",word(&shape,0x70),word(&shape,0x74),word(&shape,0x78),word(&shape,0x7c));
    r.print(file,", Origin = <%5.2lf,%5.2lf>",word(&shape,0x54),word(&shape,0x58),word(&shape,0x5c),word(&shape,0x60));
    r.print(file,"\n!# Sides = %3d",word(&shape,0x64));
    r.print(file,", Metal = %3d, Exit Metal = %3d",word(&shape,0x68),word(&shape,0x6c));
    r.print(file,"\n!# W = %5.2lf, S = %5.2lf, N = %5.2lf",word(&shape,0x88),word(&shape,0x8c),word(&shape,0x90),word(&shape,0x94),word(&shape,0x98),word(&shape,0x9c));
    r.print(file,"\n!# Theta = %5.2lf, Phase = %3d",word(&shape,0xa0),word(&shape,0xa4),word(&shape,0xac));
}
void write_port_technology_summary(void* file,const PortFileHeaderState& s,const PortOutputRuntime& r) {
    r.print(file,"\n!# Technolog File Data <%s> @ <%s>",s.technology_name,s.technology_directory);
    if(s.automatic_cells)r.print(file,"\n!# Auto-Cell Mode:  <alpha> = %lf, <beta> = %lf",word(&s.alpha,0),word(&s.alpha,4),word(&s.beta,0),word(&s.beta,4));
    else {
        r.print(file,"\n!# Max Inductor Cell Size: <length> = %lf, <width> %lf, <thickness> = %lf",word(&s.inductance_length,0),word(&s.inductance_length,4),word(&s.inductance_width,0),word(&s.inductance_width,4),word(&s.inductance_thickness,0),word(&s.inductance_thickness,4));
        r.print(file,"\n!# Max Capacitor Cell Size: <length> = %lf,<width> %lf",word(&s.capacitance_length,0),word(&s.capacitance_length,4),word(&s.capacitance_width,0),word(&s.capacitance_width,4));
    }
    r.print(file,"\n!# Metal Layers");
    for(int i=0;i<s.tables.metal_count;++i) {
        const auto& m=s.tables.metals[i];
        r.print(file,"\n!# Name=%-8s  Met=%d  Lyr=%3d  Mz=%4.2lf  T=%4.2lf  RS=%4.2lf  Col=%-8s",m.name,i,m.substrate,
            word(&m,0xa8),word(&m,0xac),word(&m,0xb0),word(&m,0xb4),word(&m,0xb8),word(&m,0xbc),m.color);
    }
    r.print(file,"\n!# Substrate Layers");
    for(int i=0;i<s.tables.substrate_count;++i) {
        const auto& t=s.tables.substrate[i];
        r.print(file,"\n!# Lyr=%5d  Eps=%4.2lg  Rho=%4.2lg  DZ=%4.2lg  T=%4.2lg",i,
            word(&t,4),word(&t,8),word(&t,12),word(&t,16),word(&t,20),word(&t,24),word(&t,28),word(&t,32));
    }
}
void write_port_file_header(void* file,unsigned char fast,const GeometryShape& shape,const char* command,
    const PortFileHeaderState& s,PortFileHeaderHistory& history,const PortFileHeaderRuntime& r) {
    const auto output=r.output.print;
    output(file,"\n!*************************************************************************");
    output(file,"\n!! ASITIC Ver %s\n!!  Data generated by the <%s> command in","10.20.99.0.0.0",command);
    output(file,fast?" <fast> mode.":" <slow> mode.");
    char* calendar=r.calendar(&history.timestamp);
    normalize_input_line(calendar,128,r.characters);
    output(file,"\n!! Generated on %s",calendar);
    write_port_shape_summary(file,shape,r.output);
    write_port_technology_summary(file,s,r.output);
    if(!s.automatic_cells) {
        output(file,"\n!! Cell size:  L = %4.2lf \t W = %4.2lf \t T = %4.2lf",word(&s.inductance_length,0),word(&s.inductance_length,4),word(&s.inductance_width,0),word(&s.inductance_width,4),word(&s.inductance_thickness,0),word(&s.inductance_thickness,4));
        output(file,"\n!! Cap Cell size:  L = %4.2lf \t W = %4.2lf",word(&s.capacitance_length,0),word(&s.capacitance_length,4),word(&s.capacitance_width,0),word(&s.capacitance_width,4));
    }
    output(file,"\n!*************************************************************************");
}
}
