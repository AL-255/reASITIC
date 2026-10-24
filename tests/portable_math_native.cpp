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
int main(int argc,char** argv){
    const bool extended=argc==3&&!std::strcmp(argv[1],"--extended-shunts");
    const bool ports=argc==3&&!std::strcmp(argv[1],"--extended-ports");
    const bool rounding=argc==3&&!std::strcmp(argv[1],"--extended-rounding");
    if(argc==3&&(!std::strcmp(argv[1],"--shunts")||extended||ports||rounding)){
        auto* file=fopen(argv[2],"rb");if(!file)return 2;int sizes[2];
        if(fread(sizes,4,2,file)!=2||sizes[0]<1||sizes[0]>1024||sizes[1]<0||sizes[1]>1024||sizes[1]==1){fclose(file);return 2;}
        const int n=sizes[0],pads=sizes[1],count=4*n*n+2*pads*pads;
        auto* values=static_cast<double*>(malloc(count*8));if(!values){fclose(file);return 2;}
        if(fread(values,8,count,file)!=static_cast<unsigned>(count)){free(values);fclose(file);return 2;}
        fclose(file);
        if(rounding){
            const int length=8+4*(n+1),saved=fegetround();
            auto* baseline=static_cast<double*>(malloc(2*length*sizeof(double)));if(!baseline){free(values);return 2;}
            auto* actual=baseline+length;bool identical=true;int index=0;
            const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
            for(int mode:modes){
                if(fesetround(mode)!=0||!reasitic::extended_port_network(values,values+2*n*n,n,n,n,actual,actual+8,actual+8+2*(n+1))){identical=false;break;}
                if(index++==0)memcpy(baseline,actual,length*sizeof(double));else if(memcmp(baseline,actual,length*sizeof(double)))identical=false;
            }
            fesetround(saved);free(baseline);free(values);puts(identical?"{\"rounding_modes\":4,\"identical\":true}":"{\"error\":\"Host rounding mode changed PIX states\"}");return identical?0:1;
        }
        puts(ports?probe_extended_ports(values,values+2*n*n,n):extended?probe_extended_pi_shunts(values,values+2*n*n,n):probe_pi_shunts(values,values+2*n*n,n,pads?values+4*n*n:nullptr,pads));free(values);return 0;
    }
    if(argc==3){puts(probe_error_power(strtod(argv[2],nullptr)));return 0;}if(argc!=7)return 2;double p[6];for(int i=0;i<6;++i)p[i]=strtod(argv[i+1],nullptr);puts(probe_gmd(p[0],p[1],p[2],p[3],p[4],p[5]));return 0;
}
