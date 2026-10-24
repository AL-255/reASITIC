#include "two_port_records.hpp"
#include "asitic/port_output.hpp"
#include "json.hpp"
extern "C" int sprintf(char*,const char*,...);
extern "C" int vsnprintf(char*,unsigned,const char*,__builtin_va_list);
namespace {
struct Buffer { char data[4096]{};unsigned used=0; };
int print(void* user,const char* format,...) {
    auto& b=*static_cast<Buffer*>(user);__builtin_va_list args;__builtin_va_start(args,format);
    const int n=vsnprintf(b.data+b.used,sizeof(b.data)-b.used,format,args);__builtin_va_end(args);
    if(n>0){const unsigned remaining=sizeof(b.data)-b.used;b.used+=static_cast<unsigned>(n)<remaining?static_cast<unsigned>(n):remaining-1;}return n;
}
char* complex_json(char* out,asitic::ComplexValue z) {
    out+=sprintf(out,"[");out+=__builtin_isfinite(z.real)?sprintf(out,"%.17g",z.real):sprintf(out,"null");out+=sprintf(out,",");out+=__builtin_isfinite(z.imag)?sprintf(out,"%.17g",z.imag):sprintf(out,"null");return out+sprintf(out,"]");
}
}
namespace reasitic {
char* two_port_json(char* out,const asitic::TwoPortAdmittance& y,const double* bank,bool transformer,double frequency) {
    asitic::TwoPortOutputState state{};state.admittance=y;
    __builtin_memcpy(state.model,bank,sizeof(state.model));
    state.scattering=asitic::convert_two_port_scattering(y);
    const auto& s=state.scattering;
    out+=sprintf(out,"{\"reference_ohm\":50,\"scattering_matrix\":[[");out=complex_json(out,s.s11);out+=sprintf(out,",");out=complex_json(out,s.s12);out+=sprintf(out,"],[");out=complex_json(out,s.s21);out+=sprintf(out,",");out=complex_json(out,s.s22);out+=sprintf(out,"]]");
    const int parameters[]={1,2,transformer?5:4};const char* keys[]={"s","y",transformer?"trans":"pi"};
    // Keep headers separately from numerical rows: ASITIC's header strings and
    // output ordering contain legacy quirks and are not rewritten as Touchstone.
    for(int polar=0;polar<2;++polar){
        out+=sprintf(out,polar?",\"polar_headers\":{":",\"headers\":{");
        for(int i=0;i<3;++i){Buffer b;const asitic::PortOutputRuntime runtime{print,{},{},{}};asitic::write_two_port_header(&b,parameters[i],polar,runtime);out+=sprintf(out,"%s\"%s\":",i?",":"",keys[i]);out=reasitic::json_string(out,b.data);}out+=sprintf(out,"}");
        out+=sprintf(out,polar?",\"polar_records\":{":",\"records\":{");
        for(int i=0;i<3;++i){Buffer b;asitic::write_two_port_record(&b,frequency,parameters[i],polar,state,print);out+=sprintf(out,"%s\"%s\":",i?",":"",keys[i]);out=reasitic::json_string(out,b.data);}out+=sprintf(out,"}");
    }
    return out+sprintf(out,"}");
}
}
extern "C" const char* probe_two_port_output(const double* values,const double* bank,int transformer,double frequency) {
    static char output[16384];
    if(!values || !bank || !__builtin_isfinite(frequency) || frequency<=0)return "{\"error\":\"Use a positive finite two-port frequency and valid arrays\"}";
    for(int i=0;i<8;++i)if(!__builtin_isfinite(values[i]))return "{\"error\":\"Original node equations produced nonfinite admittance\"}";
    asitic::TwoPortAdmittance y;__builtin_memcpy(&y,values,sizeof(y));
    char* end=reasitic::two_port_json(output,y,bank,transformer!=0,frequency);*end=0;return output;
}
