extern "C" void reset_technology();
extern "C" void set_metal(int,double,double,double);
extern "C" void set_via(int,int,int,double,double,double,double,double);
extern "C" const char* generate(int,const double*);
extern "C" double strtod(const char*,char**);
extern "C" int puts(const char*);
extern "C" int atoi(const char*);
extern "C" int main(int argc,char** argv,char**) {
    if(argc!=14)return 2;
    reset_technology();
    double height[]={1.3,2.4,3.5,4.5,6},sheet[]={.065,.055,.055,.055,.03};
    for(int i=0;i<5;++i)set_metal(i,height[i],i==4?1.:.5,sheet[i]);
    for(int i=0;i<4;++i)set_via(i,i,i+1,i==0?.5:.4,i==3?1.2:1.1,.2,.2,4);
    double p[12];for(int i=0;i<12;++i)p[i]=strtod(argv[i+2],nullptr);
    puts(generate(atoi(argv[1]),p));return 0;
}
asm(".text\n.global _start\n_start:\n"
    "xorl %ebp,%ebp\npopl %esi\nmovl %esp,%ecx\nandl $-16,%esp\n"
    "pushl %eax\npushl %esp\npushl %edx\npushl $0\npushl $0\npushl %ecx\npushl %esi\npushl $main\n"
    "call __libc_start_main\nhlt\n");
