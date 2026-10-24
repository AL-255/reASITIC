#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfenv>
#include "pi_shunts.hpp"
extern "C" const char* probe_error_power(double);
extern "C" const char* probe_gmd(double,double,double,double,double,double);
extern "C" const char* probe_pi_shunts(const double*,const double*,int,const double*,int);
extern "C" const char* probe_extended_pi_shunts(const double*,const double*,int);
extern "C" const char* probe_extended_ports(const double*,const double*,int);
extern "C" const char* probe_folded_ports(const double*,const double*,int,const double*,int);
extern "C" const char* probe_pi_impedance(const double*);
extern "C" const char* loaded_port_impedance(const double*,double,double,int,double,double,double);
extern "C" const char* probe_scattering(const double*);
int main(int argc,char** argv){
    if(argc==3&&(!std::strcmp(argv[1],"--scattering")||!std::strcmp(argv[1],"--scattering-rounding"))){
        auto* file=fopen(argv[2],"rb");if(!file)return 2;double v[8];const bool read=fread(v,8,8,file)==8;fclose(file);if(!read)return 2;
        if(!std::strcmp(argv[1],"--scattering")){puts(probe_scattering(v));return 0;}
        const int saved=fegetround(),modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};double baseline[8],actual[8];int index=0;bool identical=true;
        for(int mode:modes){
            if(fesetround(mode)||!reasitic::two_port_scattering(v,actual)){identical=false;break;}
            if(index++==0)memcpy(baseline,actual,sizeof(actual));else if(memcmp(baseline,actual,sizeof(actual)))identical=false;
        }
        fesetround(saved);puts(identical?"{\"rounding_modes\":4,\"identical\":true}":"{\"error\":\"Host rounding mode changed scattering parameters\"}");return identical?0:1;
    }
    if(argc==3&&(!std::strcmp(argv[1],"--loaded-port")||!std::strcmp(argv[1],"--loaded-port-rounding"))){
        auto* file=fopen(argv[2],"rb");if(!file)return 2;double v[14];const bool read=fread(v,8,14,file)==14;fclose(file);if(!read)return 2;
        if(!std::strcmp(argv[1],"--loaded-port")){puts(loaded_port_impedance(v,v[8],v[9],static_cast<int>(v[10]),v[11],v[12],v[13]));return 0;}
        const int saved=fegetround(),modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};double baseline[2],actual[2];int baseline_status=0,status=0,index=0;bool identical=true;
        for(int mode:modes){
            if(fesetround(mode)||!reasitic::loaded_port_impedance(v,v+8,static_cast<int>(v[10]),v[11],v[12],v[13],actual,status)){identical=false;break;}
            if(index++==0){memcpy(baseline,actual,sizeof(actual));baseline_status=status;}else if(status!=baseline_status||memcmp(baseline,actual,sizeof(actual)))identical=false;
        }
        fesetround(saved);puts(identical?"{\"rounding_modes\":4,\"identical\":true}":"{\"error\":\"Host rounding mode changed loaded input impedance\"}");return identical?0:1;
    }
    if(argc==3&&(!std::strcmp(argv[1],"--pi-impedance")||!std::strcmp(argv[1],"--pi-impedance-rounding"))){
        auto* file=fopen(argv[2],"rb");if(!file)return 2;double values[6];const bool read=fread(values,8,6,file)==6;fclose(file);if(!read)return 2;
        if(!std::strcmp(argv[1],"--pi-impedance")){puts(probe_pi_impedance(values));return 0;}
        const int saved=fegetround(),modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};double baseline[7],actual[7];bool identical=true;int index=0;
        for(int mode:modes){
            if(fesetround(mode)||!reasitic::pi_branch_impedances(values,{values[2],values[3],values[4],values[5]},actual,actual[6])){identical=false;break;}
            if(index++==0)memcpy(baseline,actual,sizeof(actual));else if(memcmp(baseline,actual,sizeof(actual)))identical=false;
        }
        fesetround(saved);puts(identical?"{\"rounding_modes\":4,\"identical\":true}":"{\"error\":\"Host rounding mode changed Pi impedance\"}");return identical?0:1;
    }
    const bool extended=argc==3&&!std::strcmp(argv[1],"--extended-shunts");
    const bool ports=argc==3&&!std::strcmp(argv[1],"--extended-ports");
    const bool rounding=argc==3&&!std::strcmp(argv[1],"--extended-rounding");
    const bool folded_rounding=argc==3&&!std::strcmp(argv[1],"--folded-rounding");
    const bool folded=argc==3&&(!std::strcmp(argv[1],"--folded-ports")||folded_rounding);
    if(argc==3&&(!std::strcmp(argv[1],"--shunts")||extended||ports||rounding||folded)){
        auto* file=fopen(argv[2],"rb");if(!file)return 2;int sizes[2];
        if(fread(sizes,4,2,file)!=2||sizes[0]<1||sizes[0]>1024||sizes[1]<0||sizes[1]>1024||sizes[1]==1){fclose(file);return 2;}
        const int n=sizes[0],pads=sizes[1],count=4*n*n+2*pads*pads;
        auto* values=static_cast<double*>(malloc(count*8));if(!values){fclose(file);return 2;}
        if(fread(values,8,count,file)!=static_cast<unsigned>(count)){free(values);fclose(file);return 2;}
        fclose(file);
        if(rounding||folded_rounding){
            const int length=folded?16+2*n*n:8+4*(n+1),saved=fegetround();
            auto* baseline=static_cast<double*>(malloc(2*length*sizeof(double)));if(!baseline){free(values);return 2;}
            auto* actual=baseline+length;bool identical=true;int index=0;
            const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
            for(int mode:modes){
                if(fesetround(mode)!=0){identical=false;break;}
                const bool solved=folded?reasitic::folded_port_network(values,values+2*n*n,n,n,n,pads?values+4*n*n:nullptr,pads,actual,actual+8,actual+8+2*n*n):reasitic::extended_port_network(values,values+2*n*n,n,n,n,actual,actual+8,actual+8+2*(n+1));
                if(!solved){identical=false;break;}
                if(index++==0)memcpy(baseline,actual,length*sizeof(double));else if(memcmp(baseline,actual,length*sizeof(double)))identical=false;
            }
            fesetround(saved);free(baseline);free(values);puts(identical?"{\"rounding_modes\":4,\"identical\":true}":"{\"error\":\"Host rounding mode changed port states\"}");return identical?0:1;
        }
        puts(folded?probe_folded_ports(values,values+2*n*n,n,pads?values+4*n*n:nullptr,pads):ports?probe_extended_ports(values,values+2*n*n,n):extended?probe_extended_pi_shunts(values,values+2*n*n,n):probe_pi_shunts(values,values+2*n*n,n,pads?values+4*n*n:nullptr,pads));free(values);return 0;
    }
    if(argc==3){puts(probe_error_power(strtod(argv[2],nullptr)));return 0;}if(argc!=7)return 2;double p[6];for(int i=0;i<6;++i)p[i]=strtod(argv[i+1],nullptr);puts(probe_gmd(p[0],p[1],p[2],p[3],p[4],p[5]));return 0;
}
