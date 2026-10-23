#include "asitic/technology_text.hpp"
#include "x87.hpp"

namespace asitic {
char* normalize_input_line(char* line,unsigned flags,const CharacterTables& t) {
    char* out=line;const char* in=line;bool space=false,content=false,quoted=false;char quote=0;
    while(const char value=*in++) {
        char c=value;
        if(quoted) { *out++=c;if(c==quote)quoted=false;continue; }
        if((flags&16) && (c=='\"' || c=='\'')) { *out++=c;space=false;content=true;quoted=true;quote=c;continue; }
        const int index=static_cast<signed char>(c);
        const auto classification=static_cast<const volatile unsigned short*>(t.classification)[index];
        if((classification&0x2000) && index>=0) {
            if((flags&1) || ((flags&2) && !content))continue;
            if(flags&8) { if(!space) { *out++=' ';space=true; }continue; }
            if((flags&128) && (classification&2))continue;
        } else {
            space=false;content=true;
            if((classification&2) && index>=0) { if(flags&128)continue; }
            else if(index>=0) {
                if(flags&32)c=static_cast<char>(t.uppercase[index]);
                if(flags&64)c=static_cast<char>(t.lowercase[static_cast<signed char>(c)]);
            }
        }
        *out++=c;
    }
    *out=0;
    if(flags&4)for(;;) {
        --out;const int c=*reinterpret_cast<const volatile signed char*>(out);
        const auto classification=reinterpret_cast<const volatile unsigned char*>(t.classification)[c*2+1];
        if(!(classification&0x20) || c<0 || reinterpret_cast<unsigned>(out)<reinterpret_cast<unsigned>(line))break;
        *out=0;
    }
    return line;
}
namespace {
void floating_copy(double& out,const double& in) { asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st"); }
void copy(char* out,const char* in) { do { *out++=*in; }while(*in++); }
bool prefix(const char* text,const char* key) { while(*key)if(*text++!=*key++)return false;return true; }
unsigned length(const char* p) { unsigned n=0;while(p[n])++n;return n; }
bool contains(const char* text,const char* needle) { while(*text) { if(prefix(text,needle))return true;++text; }return false; }
long double ceiling(long double value) {
    unsigned short saved,temporary;long double result;
    asm volatile("fnstcw %0" : "=m"(saved));temporary=(saved&0xf3ffu)|0x0800u;
    asm volatile("fldcw %2; frndint; fldcw %3" : "=t"(result) : "0"(value),"m"(temporary),"m"(saved));return result;
}
double power_two(long double exponent) {
    long double result=1.;for(int i=0;i<static_cast<int>(exponent);++i)result*=2.;return x87::binary64(result);
}
int fft_power(int n,bool spill,bool increment) {
    const double numerator=x87::binary64(x87::log(static_cast<long double>(n)));
    long double quotient=static_cast<long double>(numerator)/x87::log(2.L);
    if(spill)quotient=x87::binary64(quotient);
    const double exponent=x87::binary64(ceiling(quotient));
    return x87::truncate_int(power_two(static_cast<long double>(exponent)+(increment?1.L:0.L)));
}
}
int parse_technology(void* file,TechnologyInput& input,TechnologyRecords& records,
    TechnologyParseHistory& history,const TechnologyRuntime& r,const TechnologyTextRuntime& text) {
    input.directory[0]=input.name[0]=0;input.fft_y=input.fft_x=-271;input.height=input.width=-271.;
    TechnologySubstrate substrate;TechnologyMetal metal;TechnologyVia via;
    initialize_technology_substrate(substrate);initialize_technology_metal(metal);initialize_technology_via(via);
    char preserved[256],key[256],value[256],mixed[256];int state=4,line_number=0;
    const auto flush=[&]() {
        if(state==1)copy_technology_substrate(records.substrate,substrate,r);
        else if(state==2)copy_technology_metal(records.metals,metal,r);
        else if(state==3)copy_technology_via(records.vias,via,r);
    };
    const auto error=[&](const char* format) { r.format(r.line,format,line_number);r.warnings.emit(r.line); };
    while(text.read_line(r.line,256,file)) {
        ++line_number;copy(preserved,r.line);normalize_input_line(r.line,0xae,text.characters);normalize_input_line(preserved,0x8e,text.characters);
        if(!r.line[0] || r.line[0]==';')continue;
        const bool layer=prefix(r.line,"<LAYER>"),conductor=prefix(r.line,"<METAL>"),contact=prefix(r.line,"<VIA>"),chip=prefix(r.line,"<CHIP>");
        if(layer || conductor || contact || chip) {
            flush();if(state==1)initialize_technology_substrate(substrate);else if(state==2)initialize_technology_metal(metal);else if(state==3)initialize_technology_via(via);
        }
        if(layer || conductor || contact) {
            int* index=layer?&substrate.index:conductor?&metal.index:&via.index;
            text.scan(r.line+(contact?5:7),"%d",index);
            if(*index<0)error("Error in line %-5d:  Index must be non-negative.");
            state=layer?1:conductor?2:3;continue;
        }
        if(chip) { state=0;continue; }
        if(state==4) { error("Error in line %-5d:  Unknown Command.");continue; }
        const char* equals=preserved;while(*equals && *equals!='=')++equals;
        if(!*equals) { error("Error in line %-5d:  Syntax error, rhs cannot be null.  Skipping this line.");continue; }
        unsigned n=equals-preserved;for(unsigned i=0;i<n;++i)key[i]=preserved[i];key[n]=0;
        ++equals;n=0;while(equals[n] && equals[n]!=';') { value[n]=equals[n];++n; }value[n]=0;copy(mixed,value);
        normalize_input_line(key,0xae,text.characters);normalize_input_line(value,0xae,text.characters);normalize_input_line(mixed,0x8e,text.characters);
        text.scan(value,"%lf",&history.number);
        if(state==2) {
            if(prefix(key,"LAYER"))metal.substrate=x87::truncate_int(history.number);
            else if(prefix(key,"D"))floating_copy(metal.z,history.number);
            else if(prefix(key,"RSH"))floating_copy(metal.resistance,history.number);
            else if(prefix(key,"SMIN"))floating_copy(metal.minimum_spacing,history.number);
            else if(prefix(key,"WMIN"))floating_copy(metal.minimum_width,history.number);
            else if(prefix(key,"T"))floating_copy(metal.thickness,history.number);
            else if(prefix(key,"NAME"))copy(metal.name,value);
            else if(prefix(key,"COLOR"))copy(metal.color,value);
        } else if(state==1) {
            if(prefix(key,"EPS"))floating_copy(substrate.permittivity,history.number);
            else if(prefix(key,"RHO"))substrate.resistivity=x87::binary64(static_cast<long double>(history.number)*10000.L);
            else if(prefix(key,"T"))floating_copy(substrate.thickness,history.number);
        } else if(state==0) {
            if(prefix(key,"CHIPX"))floating_copy(input.width,history.number);
            else if(prefix(key,"CHIPY"))floating_copy(input.height,history.number);
            else if(prefix(key,"FFTX"))input.fft_x=x87::truncate_int(history.number);
            else if(prefix(key,"FFTY"))input.fft_y=x87::truncate_int(history.number);
            else if(prefix(key,"TECHFILE"))copy(input.name,mixed);
            else if(prefix(key,"TECHPATH"))copy(input.directory,mixed);
            else if(prefix(key,"FREQ"))input.frequency=x87::binary64(static_cast<long double>(history.number)*1000000000.L);
            else if(prefix(key,"EDDY")) {
                if(input.eddy_count<2) { input.eddy_indices[input.eddy_count]=x87::truncate_int(history.number);++input.eddy_count; }
                else r.warnings.emit("Warning:  Only 2 layers can be specified for eddy-current analysis.");
            }
        } else if(state==3) {
            if(prefix(key,"BOTTOM"))via.bottom=x87::truncate_int(history.number);
            else if(prefix(key,"TOP"))via.top=x87::truncate_int(history.number);
            else if(prefix(key,"R"))floating_copy(via.resistance,history.number);
            else if(prefix(key,"WIDTH"))floating_copy(via.width,history.number);
            else if(prefix(key,"SPACE"))floating_copy(via.spacing,history.number);
            else if(prefix(key,"OVERPLOT1"))floating_copy(via.overplot1,history.number);
            else if(prefix(key,"OVERPLOT2"))floating_copy(via.overplot2,history.number);
            else if(prefix(key,"NAME"))copy(via.name,value);
            else if(prefix(key,"COLOR"))copy(via.color,value);
        } else error("Error in line %-5d:  Ignoring unrecognized parameter.");
    }
    flush();return 0;
}
bool validate_technology_input(const char* source_name,TechnologyInput& input,const TechnologyRuntime& r) {
    if(!input.directory[0]) { r.warnings.emit("Warning:  Path for technology file not specified.");r.warnings.emit("          Assuming current path.");
        input.directory[0]='.';input.directory[1]=0; }
    if(!input.name[0]) {
        r.warnings.emit("Warning:  Technology data file not specified.");r.warnings.emit("          Assuming root name of this file.");
        if(!contains(source_name,".tek") && !contains(source_name,".TEK"))copy(input.name,source_name);
        else { const unsigned n=length(source_name)-4;for(unsigned i=0;i<n;++i)input.name[i]=source_name[i]; }
    }
    bool error=false;
    if(input.height==-271.) { r.warnings.emit("Warning:  Chip y-dimension not defined.");r.warnings.emit("          Using 1000um as default.");input.height=1000.; }
    if(input.height<0.) { r.warnings.emit("Error:  Chip y-dimension must be positive quantity.");error=true; }
    if(input.width==-271.) { r.warnings.emit("Warning:  Chip x-dimension not defined.");r.warnings.emit("          Using 1000um as default.");input.width=1000.; }
    if(input.width<0.) { r.warnings.emit("Error:  Chip x-dimension must be positive quantity.");error=true; }
    if(input.fft_x==-271) { r.warnings.emit("Warning:  xFFTSize not specified.");r.warnings.emit("          Using 512 as default.");input.fft_x=512; }
    if(input.fft_x<1) { input.fft_x=512;r.warnings.emit("Error:  xFFTSize must be a positive power of 2.");error=true; }
    if(input.fft_y==-271) { r.warnings.emit("Warning:  yFFTSize not specified.");r.warnings.emit("          Using 512 as default.");input.fft_y=512; }
    if(input.fft_y<1) { input.fft_y=512;r.warnings.emit("Error:  yFFTSize must be a positive power of 2.");error=true; }
    if(input.fft_x!=fft_power(input.fft_x,false,false)) {
        r.warnings.emit("Warning:  x-FFT size not a power of 2.");input.fft_x=fft_power(input.fft_x,true,true);
        r.format(r.line,"Will use xFFTSize = %d instead.",input.fft_x);r.warnings.emit(r.line);
    }
    if(input.fft_y!=fft_power(input.fft_y,true,false)) {
        r.warnings.emit("Warning:  y-FFT size not a power of 2.");input.fft_y=fft_power(input.fft_y,true,true);
        r.format(r.line,"Will use yFFTSize = %d instead.",input.fft_y);r.warnings.emit(r.line);
    }
    return error;
}
namespace {
__attribute__((noinline)) ComplexValue* coefficients(int count,ArrayAllocator allocator) {
    auto* result=static_cast<ComplexValue*>(allocator.get(static_cast<unsigned>(count)*16u));
    for(int i=0;i<count;++i)result[i]={0,0};
    return result;
}
}
bool load_technology(const char* source_name,TechnologyInitialization& s,TechnologyParseHistory& history,
    const TechnologyRuntime& r,const TechnologyTextRuntime& text,const TechnologyFiles& files) {
    char filename[80];
    if(source_name) { unsigned i=0;for(;i<80 && source_name[i];++i)filename[i]=source_name[i];for(;i<80;++i)filename[i]=0; }
    else { files.prompt_output.emit("Technology File Name? ");files.prompt(filename,0,files.user); }
    if(!contains(filename,".tek"))copy(filename+length(filename),".tek");
    void* file=files.open(filename,"r",files.user);
    if(!file) { r.warnings.emit("Unable to open technology file.");return false; }
    if(s.tables.metals && s.tables.substrate) {
        r.warnings.emit("Error:  Technology file already in place.");files.close(file,files.user);return false;
    }
    parse_technology(file,s.input,s.records,history,r,text);files.close(file,files.user);
    const bool input_error=validate_technology_input(filename,s.input,r);
    const int substrate=validate_technology_substrate(s.records.substrate,r),metal=validate_technology_metals(s.records.metals,s.colors,r),
        via=validate_technology_vias(s.records.vias,s.colors,r);
    s.tables.substrate_count=static_cast<int>(static_cast<unsigned>(substrate)+1u);
    s.tables.metal_count=static_cast<int>(static_cast<unsigned>(metal)+1u);
    s.tables.via_count=static_cast<int>(static_cast<unsigned>(via)+1u);
    if(!s.tables.substrate_count || !s.tables.metal_count || input_error)return false;
    if(!build_technology_tables(s.tables,s.records,s.green,s.cached_eddy,s.input.eddy_indices,s.input.eddy_count,r))return false;
    if(!validate_technology_assignments(s.tables,r))return false;
    for(int i=s.tables.substrate_count-2;i>=0;--i)s.tables.substrate[i].height=x87::binary64(static_cast<long double>(s.tables.substrate[i].height)+s.tables.substrate[i+1].height);
    for(int i=0;i<s.tables.metal_count;++i) {
        auto& m=s.tables.metals[i];m.z=x87::binary64(static_cast<long double>(s.tables.substrate[m.substrate].height)-m.z);
    }
    s.depths=static_cast<double*>(r.arrays.arrays.get((static_cast<unsigned>(s.tables.substrate_count)+1u)*8u));
    s.coefficients=coefficients(s.tables.substrate_count,r.arrays.arrays);
    if(!s.depths || !s.coefficients)r.fatal.emit("104:  Could not allocate memory");
    for(int i=0;i<s.tables.substrate_count;++i)floating_copy(s.depths[i],s.tables.substrate[i].height);
    s.depths[s.tables.substrate_count]=0;
    s.metal_heights=static_cast<double*>(r.arrays.arrays.get(static_cast<unsigned>(s.tables.metal_count)*8u));
    s.group_sizes=static_cast<int*>(r.arrays.arrays.get((static_cast<unsigned>(s.tables.metal_count)+2u)*4u));
    if(!s.metal_heights || !s.group_sizes)r.fatal.emit("Could not Allocate Sufficient Memory");
    for(int i=0;i<s.tables.metal_count;++i) {
        const auto& m=s.tables.metals[i];s.metal_heights[i]=x87::binary64(static_cast<long double>(s.tables.substrate[m.substrate].height)-m.z);s.group_sizes[i]=1;
    }
    s.group_sizes[s.tables.metal_count]=1;
    if(s.tables.via_count)release_technology_vias(s.records.vias,r.arrays);
    release_technology_metals(s.records.metals,r.arrays);release_technology_substrate(s.records.substrate,r.arrays);s.records={};
    s.half_width=x87::binary64(static_cast<long double>(s.input.width)*0.5L);s.half_height=x87::binary64(static_cast<long double>(s.input.height)*0.5L);
    s.thickness_cell=10.;s.length_step=s.width_step=s.half_width;return true;
}
}
