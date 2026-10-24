extern "C" void reset_technology();
extern "C" void set_metal(int,double,double,double,int);
extern "C" void set_substrate(int,double);
extern "C" void set_substrate_material(int,double,double);
extern "C" const char* simulate_capacitance(int,int,double,double,double,double,double,int,int,int,int);
extern "C" void finalize_technology();
extern "C" const char* analyze(double);
extern "C" const char* probe_runtime_trig(int,double);
extern "C" const char* probe_math(int,double,double,double);
extern "C" double normalize_value(double,int,double,int);
extern "C" int printf(const char*,...);
extern "C" const char* probe_green(int,int,int,double,double,double,double,int);
extern "C" const char* simulate_eddy(int,double,double,double,double,double,int,int,int,int,int,int);
extern "C" const char* simulate(int,double,double,double,double,double,int);
extern "C" void set_via(int,int,int,double,double,double,double,double);
extern "C" const char* generate(int,const double*);
extern "C" const char* create_geometry(int,const char*,const char*,double,double,int,int,double,int,int);
extern "C" void set_geometry_options(double,double,int);
extern "C" double strtod(const char*,char**);
extern "C" int puts(const char*);
extern "C" int atoi(const char*);
extern "C" int main(int argc,char** argv,char**) {
    if(argc==4 && argv[1][2]=='t'){puts(probe_runtime_trig(atoi(argv[2]),strtod(argv[3],nullptr)));return 0;}
    if(argc==6 && argv[1][0]=='-' && argv[1][2]=='i') { printf("%.17g\n",normalize_value(strtod(argv[2],nullptr),atoi(argv[3]),strtod(argv[4],nullptr),atoi(argv[5])));return 0; }
    if(argc==6 && argv[1][0]=='-') { puts(probe_math(atoi(argv[2]),strtod(argv[3],nullptr),strtod(argv[4],nullptr),strtod(argv[5],nullptr)));return 0; }
    if(argc!=10 && argc!=12 && argc!=7 && argc!=14 && argc!=15 && argc!=19 && argc!=20 && argc!=21 && argc!=23)return 2;
    reset_technology();set_substrate(0,700.);set_substrate(1,50.);set_substrate_material(0,11.9,1e-3);set_substrate_material(1,3.9,1e10);finalize_technology();
    double height[]={1.3,2.4,3.5,4.5,6},sheet[]={65,55,55,55,30};
    for(int i=0;i<5;++i)set_metal(i,height[i],i==4?1.:.5,sheet[i],1);
    for(int i=0;i<4;++i)set_via(i,i,i+1,i==0?.5:.4,i==3?1.2:1.1,.2,.2,4);
    if(argc==7 && argv[1][2]=='c') { puts(create_geometry(atoi(argv[2]),argv[3],argv[4],512,512,128,128,strtod(argv[5],nullptr),atoi(argv[6]),0));return 0; }
    if(argc==12 && argv[1][2]=='c') {
        create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);const bool ground=*argv[5];
        if(ground)create_geometry(atoi(argv[4]),argv[5],"",512,512,128,128,.1,1,1);
        puts(simulate_capacitance(ground?1:0,ground?0:-1,strtod(argv[6],nullptr),strtod(argv[7],nullptr),strtod(argv[8],nullptr),512,512,atoi(argv[9]),atoi(argv[10]),512,atoi(argv[11])));return 0;
    }
    if(argc==10 && argv[1][2]=='g') { puts(probe_green(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),strtod(argv[5],nullptr),strtod(argv[6],nullptr),strtod(argv[7],nullptr),strtod(argv[8],nullptr),atoi(argv[9])));return 0; }
    if(argc==14 && argv[1][0]=='-') {
        create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);
        puts(simulate_eddy(0,strtod(argv[4],nullptr),strtod(argv[5],nullptr),strtod(argv[6],nullptr),strtod(argv[7],nullptr),strtod(argv[8],nullptr),1024,atoi(argv[9]),atoi(argv[10]),atoi(argv[11]),atoi(argv[12]),atoi(argv[13])));return 0;
    }
    double p[12];for(int i=0;i<12;++i)p[i]=strtod(argv[i+2],nullptr);
    if(argc>=21)set_geometry_options(argc==23?strtod(argv[21],nullptr):0.,argc==23?strtod(argv[22],nullptr):0.,atoi(argv[20]));
    const char* layout=generate(atoi(argv[1]),p);if(argc>=19)puts(simulate(argc>=20?atoi(argv[19]):0,strtod(argv[14],nullptr),strtod(argv[15],nullptr),strtod(argv[16],nullptr),strtod(argv[17],nullptr),strtod(argv[18],nullptr),1024));
    else puts(argc==15?analyze(strtod(argv[14],nullptr)):layout);return 0;
}
asm(".text\n.global _start\n_start:\n"
    "xorl %ebp,%ebp\npopl %esi\nmovl %esp,%ecx\nandl $-16,%esp\n"
    "pushl %eax\npushl %esp\npushl %edx\npushl $0\npushl $0\npushl %ecx\npushl %esi\npushl $main\n"
    "call __libc_start_main\nhlt\n");
