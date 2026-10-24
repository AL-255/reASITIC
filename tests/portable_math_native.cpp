extern "C" double strtod(const char*,char**);
extern "C" const char* probe_error_power(double);
extern "C" int puts(const char*);
extern "C" const char* probe_gmd(double,double,double,double,double,double);
int main(int argc,char** argv){if(argc==3){puts(probe_error_power(strtod(argv[2],nullptr)));return 0;}if(argc!=7)return 2;double p[6];for(int i=0;i<6;++i)p[i]=strtod(argv[i+1],nullptr);puts(probe_gmd(p[0],p[1],p[2],p[3],p[4],p[5]));return 0;}
