#include "pi_shunts.hpp"
#include <cstdio>
#include <cstdlib>
extern "C" const char* probe_pi_impedance(const double* values){
    static char output[1024];double result[7];
    if(!reasitic::pi_branch_impedances(values,{values[2],values[3],values[4],values[5]},result,result[6]))return "{\"error\":\"Invalid Pi branches\"}";
    char* out=output;out+=sprintf(out,"{\"input_impedance\":[");
    const auto number=[&](double v){out+=__builtin_isfinite(v)?sprintf(out,"%.17g",v):sprintf(out,"null");};
    for(int i=0;i<3;++i){out+=sprintf(out,"%s[",i?",":"");number(result[2*i]);out+=sprintf(out,",");number(result[2*i+1]);out+=sprintf(out,"]");}
    out+=sprintf(out,"],\"quality_differential\":");number(result[6]);sprintf(out,"}");return output;
}
extern "C" const char* probe_folded_ports(const double* cap,const double* ind,int n,const double* pads,int pad_stride){
    static char* output=nullptr;free(output);output=nullptr;
    if(n<1||n>1024)return "{\"error\":\"Invalid folded dimension\"}";
    auto* node=static_cast<double*>(malloc(2*n*n*sizeof(double)));double ports[8],raw[8];
    if(!node)return "{\"error\":\"Allocation failed\"}";
    if(!reasitic::folded_port_network(cap,ind,n,n,n,pads,pad_stride,ports,node,raw)){free(node);return "{\"error\":\"Folded endpoint solve failed\"}";}
    output=static_cast<char*>(malloc(128*n*n+4096));if(!output){free(node);return "{\"error\":\"Allocation failed\"}";}char* out=output;
    out+=sprintf(out,"{\"matrix\":[[[%.17g,%.17g],[%.17g,%.17g]],[[%.17g,%.17g],[%.17g,%.17g]]],\"node_capacitance_matrix\":[",ports[0],ports[1],ports[4],ports[5],ports[2],ports[3],ports[6],ports[7]);
    for(int i=0;i<n;++i){out+=sprintf(out,"%s[",i?",":"");for(int j=0;j<n;++j){const auto* value=node+2*(i+j*n);out+=sprintf(out,"%s[%.17g,%.17g]",j?",":"",value[0],value[1]);}out+=sprintf(out,"]");}
    out+=sprintf(out,"]");
    if(pads)out+=sprintf(out,",\"with_pad_admittance_matrix\":[[[%.17g,%.17g],[%.17g,%.17g]],[[%.17g,%.17g],[%.17g,%.17g]]]",raw[0],raw[1],raw[4],raw[5],raw[2],raw[3],raw[6],raw[7]);
    sprintf(out,"}");free(node);return output;
}
extern "C" const char* probe_extended_ports(const double* cap,const double* impedance,int n){
    static char* output=nullptr;free(output);output=nullptr;
    if(n<1||n>1024)return "{\"error\":\"Invalid PIX dimension\"}";
    auto* states=static_cast<double*>(malloc(4*(n+1)*sizeof(double)));double ports[8];
    if(!states)return "{\"error\":\"Allocation failed\"}";
    if(!reasitic::extended_port_network(cap,impedance,n,n,n,ports,states,states+2*(n+1))){free(states);return "{\"error\":\"Reduced PIX solve failed\"}";}
    output=static_cast<char*>(malloc(160*(n+1)+1024));if(!output){free(states);return "{\"error\":\"Allocation failed\"}";}
    char* out=output;out+=sprintf(out,"{\"matrix\":[[[%.17g,%.17g],[%.17g,%.17g]],[[%.17g,%.17g],[%.17g,%.17g]]],",ports[0],ports[1],ports[4],ports[5],ports[2],ports[3],ports[6],ports[7]);
    for(int field=0;field<2;++field){out+=sprintf(out,"%s\"%s\":[",field?",":"",field?"voltages":"currents");for(int i=0;i<=n;++i){const auto* value=states+2*(field*(n+1)+i);out+=sprintf(out,"%s[[%.17g,%.17g]]",i?",":"",value[0],value[1]);}out+=sprintf(out,"]");}
    sprintf(out,"}");free(states);return output;
}
