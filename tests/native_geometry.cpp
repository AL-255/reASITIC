extern "C" const char* port_frequency_schedule(double,double,double,int);
extern "C" void reset_technology();
extern "C" void set_metal(int,double,double,double,int);
extern "C" const char* set_metal_rules(int,double,double);
extern "C" void set_substrate(int,double);
extern "C" void set_substrate_material(int,double,double);
extern "C" const char* simulate_capacitance(int,int,double,double,double,double,double,int,int,int,int);
extern "C" void finalize_technology();
extern "C" const char* analyze(double);
extern "C" const char* probe_runtime_trig(int,double);
extern "C" const char* probe_retained_math(int,unsigned,unsigned,int);
extern "C" const char* probe_math(int,double,double,double);
extern "C" const char* probe_two_port_output(const double*,const double*,int,double);
extern "C" double normalize_value(double,int,double,int);
extern "C" int printf(const char*,...);
extern "C" const char* probe_green(int,int,int,double,double,double,double,int);
extern "C" const char* simulate_eddy(int,double,double,double,double,double,int,int,int,int,int,int);
extern "C" const char* simulate_network(int,int,double,double,double,double,double,double,double,double,int,int,int,int,int,int,int,int,int);
extern "C" const char* simulate_extended_network(int,int,double,double,double,double,double,double,double,double,int,int,int,int,int,int,int,int,int);
extern "C" const char* simulate_pad_network(int,int,int,int,double,double,double,double,double,double,double,double,int,int,int,int,int,int,int,int,int);
extern "C" const char* simulate_transformer(int,int,int,double,double,double,double,double,double,double,double,int,int,int,int,int,int,int,int,double,int);
extern "C" const char* simulate_three_port(int,int,int,int,double,double,double,double,double,double,double,double,int,int,int,int,int,int,int,int,int);
extern "C" const char* probe_three_port(int,int,int,int,double,double,double,double,double,double,double,double,int,int,int,int,int,int,int,int,int,const void*,const void*,int,const unsigned*);
extern "C" void* fopen(const char*,const char*);
extern "C" unsigned fread(void*,unsigned,unsigned,void*);
extern "C" int fclose(void*);
extern "C" void* calloc(unsigned,unsigned);
extern "C" void free(void*);
extern "C" const char* simulate(int,double,double,double,double,double,int);
extern "C" void set_via(int,int,int,double,double,double,double,double);
extern "C" const char* generate(int,const double*);
extern "C" const char* create_geometry(int,const char*,const char*,double,double,int,int,double,int,int);
extern "C" void set_geometry_options(double,double,int);
extern "C" const char* getenv(const char*);
extern "C" double strtod(const char*,char**);
extern "C" int puts(const char*);
extern "C" int atoi(const char*);
extern "C" const char* automatic_cell_limits(double,double,double);
extern "C" int set_max_width_cells(int);
extern "C" const char* edit_geometry(const char*,double,double,double,int);
extern "C" const char* self_resonance_plan(double,double,double,int,int,int,int,const double*,int);
extern "C" int main(int argc,char** argv,char**) {
    if(argc==10 && argv[1][2]=='s') { auto* values=static_cast<double*>(calloc(102,72));auto* file=fopen(argv[9],"rb");if(!file)return 2;const int count=fread(values,72,102,file);fclose(file);puts(self_resonance_plan(strtod(argv[2],nullptr),strtod(argv[3],nullptr),strtod(argv[4],nullptr),atoi(argv[5]),atoi(argv[6]),atoi(argv[7]),atoi(argv[8]),values,count));free(values);return 0; }
    if(argc==21 && argv[1][0]=='-' && argv[1][1]=='-' && argv[1][2]=='o'){double values[17];for(int i=0;i<17;++i)values[i]=strtod(argv[i+4],nullptr);puts(probe_two_port_output(values,values+8,atoi(argv[3]),strtod(argv[2],nullptr)));return 0;}
    if(argc==6 && argv[1][0]=='-' && argv[1][1]=='-' && argv[1][2]=='s'){puts(port_frequency_schedule(strtod(argv[2],nullptr),strtod(argv[3],nullptr),strtod(argv[4],nullptr),atoi(argv[5])));return 0;}
    if(argc==4 && argv[1][2]=='t'){puts(probe_runtime_trig(atoi(argv[2]),strtod(argv[3],nullptr)));return 0;}
    if(argc==6 && argv[1][2]=='x'){puts(probe_retained_math(atoi(argv[2]),static_cast<unsigned>(strtod(argv[3],nullptr)),static_cast<unsigned>(strtod(argv[4],nullptr)),atoi(argv[5])));return 0;}
    if(argc==6 && argv[1][0]=='-' && argv[1][2]=='i') { printf("%.17g\n",normalize_value(strtod(argv[2],nullptr),atoi(argv[3]),strtod(argv[4],nullptr),atoi(argv[5])));return 0; }
    if(argc==6 && argv[1][0]=='-') { puts(probe_math(atoi(argv[2]),strtod(argv[3],nullptr),strtod(argv[4],nullptr),strtod(argv[5],nullptr)));return 0; }
    if(argc==5 && (argv[1][0]!='-' || argv[1][1]!='-' || argv[1][2]!='a'))return 2;
    if(argc==8 && (argv[1][0]!='-' || argv[1][1]!='-' || argv[1][2]!='m'))return 2;
    if(argc!=8 && argc!=5 && argc!=26 && argc!=22 && argc!=10 && argc!=12 && argc!=7 && argc!=14 && argc!=15 && argc!=19 && argc!=20 && argc!=21 && argc!=23)return 2;
    reset_technology();set_substrate(0,700.);set_substrate(1,50.);set_substrate_material(0,11.9,1e-3);set_substrate_material(1,3.9,1e10);finalize_technology();
    double height[]={1.3,2.4,3.5,4.5,6},sheet[]={65,55,55,55,30};
    for(int i=0;i<5;++i)set_metal(i,height[i],i==4?1.:.5,sheet[i],1);
    for(int i=0;i<4;++i)set_via(i,i,i+1,i==0?.5:.4,i==3?1.2:1.1,.2,.2,4);
    if(const char* maximum=getenv("REASITIC_MAXNW"))if(!set_max_width_cells(atoi(maximum)))return 2;
    if(const char* metal=getenv("REASITIC_RULE_METAL")){
        const auto* spacing=getenv("REASITIC_SMIN");const auto* width=getenv("REASITIC_WMIN");if(!spacing||!width)return 2;
        const auto* rules=set_metal_rules(atoi(metal),strtod(spacing,nullptr),strtod(width,nullptr));if(__builtin_strncmp(rules,"{\"error\":",9)==0){puts(rules);return 2;}
    }
    if(argc==7 && argv[1][2]=='e') { create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);puts(edit_geometry(argv[4],512,512,strtod(argv[5],nullptr),atoi(argv[6])));return 0; }
    if(argc==8 && argv[1][2]=='m') { create_geometry(2,argv[2],"",512,512,128,128,.1,1,0);if(!set_max_width_cells(atoi(argv[7])))return 2;puts(simulate(0,strtod(argv[3],nullptr),strtod(argv[4],nullptr),strtod(argv[5],nullptr),strtod(argv[6],nullptr),strtod(argv[4],nullptr),1024));return 0; }
    if((argc==5 || argc==7) && argv[1][2]=='a') { if(argc==7)set_metal(4,6.,strtod(argv[5],nullptr),strtod(argv[6],nullptr),1);puts(automatic_cell_limits(strtod(argv[2],nullptr),strtod(argv[3],nullptr),strtod(argv[4],nullptr)));return 0; }
    if(argc==7 && argv[1][2]=='c') { puts(create_geometry(atoi(argv[2]),argv[3],argv[4],512,512,128,128,strtod(argv[5],nullptr),atoi(argv[6]),0));return 0; }
    if(argc==12 && argv[1][2]=='c') {
        create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);const bool ground=*argv[5];
        if(ground)create_geometry(atoi(argv[4]),argv[5],"",512,512,128,128,.1,1,1);
        puts(simulate_capacitance(ground?1:0,ground?0:-1,strtod(argv[6],nullptr),strtod(argv[7],nullptr),strtod(argv[8],nullptr),512,512,atoi(argv[9]),atoi(argv[10]),512,atoi(argv[11])));return 0;
    }
    if(argc==10 && argv[1][2]=='g') { puts(probe_green(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),strtod(argv[5],nullptr),strtod(argv[6],nullptr),strtod(argv[7],nullptr),strtod(argv[8],nullptr),atoi(argv[9])));return 0; }
    if(argc==22 && argv[1][2]=='p') {
        create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);create_geometry(atoi(argv[4]),argv[5],"",512,512,128,128,.1,1,1);create_geometry(atoi(argv[6]),argv[7],"",512,512,128,128,.1,1,1);
        int device=2,pad1=1,pad2=0,ground=-1;
        if(argv[8][0]=='@')ground=argv[8][1]=='1'?pad1:argv[8][1]=='2'?pad2:device;
        else if(*argv[8]){create_geometry(2,argv[8],"",512,512,128,128,.1,1,1);++device;++pad1;++pad2;ground=0;}
        puts(simulate_pad_network(device,pad1,pad2,ground,strtod(argv[9],nullptr),strtod(argv[10],nullptr),strtod(argv[11],nullptr),strtod(argv[12],nullptr),strtod(argv[13],nullptr),strtod(argv[14],nullptr),512,512,atoi(argv[15]),atoi(argv[16]),512,1024,atoi(argv[17]),atoi(argv[18]),atoi(argv[19]),atoi(argv[20]),atoi(argv[21])));return 0;
    }
    if((argc==22 || argc==26) && argv[1][0]=='-') {
        create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);create_geometry(atoi(argv[4]),argv[5],"",512,512,128,128,.1,1,1);create_geometry(atoi(argv[6]),argv[7],"",512,512,128,128,.1,1,1);
        int first=2,second=1,third=0,ground=-1;
        if(argv[8][0]=='@')ground=argv[8][1]=='1'?first:argv[8][1]=='2'?second:third;
        else if(*argv[8]){create_geometry(2,argv[8],"",512,512,128,128,.1,1,1);++first;++second;++third;ground=0;}
        if(argc==26){const int cells=atoi(argv[24]);if(cells<1 || cells>512*512)return 2;void* cap=calloc(cells,16),*ind=calloc(cells,16),*a=fopen(argv[22],"rb"),*b=fopen(argv[23],"rb");if(!cap || !ind || !a || !b)return 2;
            if(fread(cap,16,cells,a)!=static_cast<unsigned>(cells) || fread(ind,16,cells,b)!=static_cast<unsigned>(cells))return 2;fclose(a);fclose(b);
            unsigned addresses[5];void* meta=fopen(argv[25],"rb");if(!meta || fread(addresses,4,5,meta)!=5)return 2;fclose(meta);
            puts(probe_three_port(first,second,third,ground,strtod(argv[9],nullptr),strtod(argv[10],nullptr),strtod(argv[11],nullptr),strtod(argv[12],nullptr),strtod(argv[13],nullptr),strtod(argv[14],nullptr),512,512,atoi(argv[15]),atoi(argv[16]),512,1024,atoi(argv[17]),atoi(argv[18]),atoi(argv[19]),atoi(argv[20]),atoi(argv[21]),cap,ind,cells,addresses));free(cap);free(ind);return 0;}
        puts(simulate_three_port(first,second,third,ground,strtod(argv[9],nullptr),strtod(argv[10],nullptr),strtod(argv[11],nullptr),strtod(argv[12],nullptr),strtod(argv[13],nullptr),strtod(argv[14],nullptr),512,512,atoi(argv[15]),atoi(argv[16]),512,1024,atoi(argv[17]),atoi(argv[18]),atoi(argv[19]),atoi(argv[20]),atoi(argv[21])));return 0;
    }
    if(argc==23 && argv[1][0]=='-' && argv[1][2]=='c') {
        create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);
        if(atoi(argv[4])>=0)create_geometry(atoi(argv[4]),argv[5],"",512,512,128,128,.1,1,1);
        int primary=atoi(argv[21]),secondary=atoi(argv[22]),ground=-1;
        if(argv[6][0]=='@')ground=argv[6][1]=='1'?primary:secondary;
        else if(*argv[6]){create_geometry(2,argv[6],"",512,512,128,128,.1,1,1);++primary;++secondary;ground=0;}
        puts(simulate_transformer(primary,secondary,ground,strtod(argv[7],nullptr),strtod(argv[8],nullptr),strtod(argv[9],nullptr),strtod(argv[10],nullptr),strtod(argv[11],nullptr),strtod(argv[12],nullptr),512,512,atoi(argv[13]),atoi(argv[14]),512,1024,atoi(argv[15]),atoi(argv[16]),atoi(argv[17]),atoi(argv[18]),strtod(argv[19],nullptr),atoi(argv[20])));return 0;
    }
    if(argc==21 && argv[1][0]=='-') {
        create_geometry(atoi(argv[2]),argv[3],"",512,512,128,128,.1,1,0);const bool self=argv[5][0]=='@',ground=*argv[5] && !self;
        if(ground)create_geometry(atoi(argv[4]),argv[5],"",512,512,128,128,.1,1,1);
        const auto solver=argv[1][2]=='p'?simulate_extended_network:simulate_network;
        puts(solver(ground?1:0,ground?0:self?0:-1,strtod(argv[6],nullptr),strtod(argv[7],nullptr),strtod(argv[8],nullptr),strtod(argv[9],nullptr),strtod(argv[10],nullptr),strtod(argv[11],nullptr),512,512,atoi(argv[12]),atoi(argv[13]),atoi(argv[14]),atoi(argv[15]),atoi(argv[16]),atoi(argv[17]),atoi(argv[18]),atoi(argv[19]),atoi(argv[20])));return 0;
    }
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
