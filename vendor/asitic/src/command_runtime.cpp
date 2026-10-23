#include "asitic/command_runtime.hpp"
namespace asitic {
namespace {
const CommandDefinition catalog[] = {
#include "command_catalog.inc"
};
bool equal(const char* a,const char* b) { while(*a && *a==*b){++a;++b;}return *a==*b; }
void copy(char* d,const char* s) { do { *d=*s; ++d; } while(*s++); }
float binary32(long double n) { volatile float out=static_cast<float>(n);return out; }
long double load(const double& n) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(n):"st");return out; }
void value(CommandArgument& a,float n) { __builtin_memcpy(&a.value,&n,4); }
double floor_spill(long double n) {
    unsigned short saved,down; double out;
    asm volatile("fnstcw %0":"=m"(saved));down=(saved&0xf3ffu)|0x400u;
    asm volatile("fldcw %1; fldt %2; frndint; fldcw %3; fstpl %0":"=m"(out):"m"(down),"m"(n),"m"(saved):"st");return out;
}
void warn(const CommandRuntime& r,const char* message) { r.warnings.emit(message); }
}
const CommandDefinition* native_command_catalog() { return catalog; }
int lookup_command_index(const char* text,const CommandDefinition* rows) {
    for(int i=0;rows[i].id!=-2;++i)
        for(const char* alias:rows[i].aliases)if(equal(text,alias))return i;
    return -1;
}
bool tokenize_command(char* line,CommandArguments& out,int& count,const CommandRuntime& r) {
    count=0;normalize_input_line(line,0x8e,r.characters);
    if(!*line || *line=='#')return false;
    char* token=r.token(line," ,");
    if(!line)return false; // Native tests the original pointer, not token.
    normalize_input_line(token,0x20,r.characters);copy(out.command.text,token);
    while(count<15) {
        token=r.token(nullptr," ,");if(!token)break;
        unsigned n=0;for(;n<256 && token[n];++n)out.arguments[count].text[n]=token[n];
        for(;n<256;++n)out.arguments[count].text[n]=0;
        ++count;
    }
    return true;
}
bool parse_command_argument(CommandArgument& a,unsigned type,const CommandRuntime& r,CommandParseHistory& h) {
    switch(type) {
    case 0:case 8: {
        normalize_input_line(a.text,0x20,r.characters);
        GeometryShape* shape=find_geometry_shape(r.shapes,r.selected,a.text);
        if(!shape) {
            r.format(r.message,type==0?"Error:  Could not Find Spiral <%s>.":"Error:  Could not Find Ground Spiral <%s>.",a.text);
            warn(r,r.message);return false;
        }
        a.value=reinterpret_cast<unsigned long>(shape);
        if(type==8)reset_geometry_mesh_counts(*shape);
        return true;
    }
    case 1:normalize_input_line(a.text,0x20,r.characters);return *a.text;
    case 3:return *a.text;
    case 2:
        normalize_input_line(a.text,0x20,r.characters);r.scan(a.text,"%c",&h.character);
        if(static_cast<unsigned char>(h.character-65)>25)return false;
        reinterpret_cast<unsigned char*>(&a.value)[0]=h.character;return true;
    case 4:r.scan(a.text,"%lf",&h.number);value(a,binary32(load(h.number)));return true;
    case 5: {
        r.scan(a.text,"%lf",&h.number);const long double n=load(h.number);
        value(a,binary32(n*load(r.frequency_scale)));
        if(n<=load(1e-10)) { warn(r,"Error 2102:  Frequency must be positive.");return false; }return true;
    }
    case 6:r.scan(a.text,"%d",&h.integer);__builtin_memcpy(&a.value,&h.integer,4);return true;
    case 7: {
        r.scan(a.text,"%lf",&h.number);
        float f=binary32(load(h.number)*load(r.length_scale)*1000000.L);value(a,f);
        if(r.snap) {
            const long double grid=load(r.grid_size);
            const double steps=floor_spill((grid*0.5L+static_cast<long double>(f))/grid);
            f=binary32(load(steps)*grid);value(a,f);
        }return true;
    }
    case 9: {
        normalize_input_line(a.text,0x20,r.characters);
        const char* yes[]={"TRUE","T","YES","ON","Y"};const char* no[]={"FALSE","F","NO","OFF","N"};
        for(const char* s:yes)if(equal(a.text,s)){reinterpret_cast<unsigned char*>(&a.value)[0]=1;return true;}
        for(const char* s:no)if(equal(a.text,s)){reinterpret_cast<unsigned char*>(&a.value)[0]=0;return true;}
        r.format(r.message,"Error:  <%s> is not a member of the set:  (true, yes, on, false, no, off)",a.text);
        warn(r,r.message);return false;
    }
    case 10: {
        r.scan(a.text,"%lf",&h.number);const long double n=load(h.number);float f=0;
        if(n>0)f=binary32(n-load(floor_spill(n/360.L))*360.L);
        else if(n<0)f=binary32(load(floor_spill(n/360.L))*360.L+n);
        value(a,f);value(a,binary32(static_cast<long double>(f)*load(0x1.1df46a2529d39p-6)));return true;
    }
    default:warn(r,"Error:  Unknown argument type.");return false;
    }
}
int parse_command_arguments(char* line,CommandArguments& out,const CommandRuntime& r,CommandParseHistory* histories,const CommandDefinition* rows) {
    int count;if(!tokenize_command(line,out,count,r))return -3;
    const int index=lookup_command_index(out.command.text,rows);if(index==-1)return -2;
    const auto& command=rows[index];
    if(count<command.minimum || count>command.maximum) {
        r.format(r.message,"Invalid Argument(s):  %s",command.usage);warn(r,r.message);return -3;
    }
    out.command.value=count;
    for(int i=0;i<count && i<command.maximum;++i)
        if(!parse_command_argument(out.arguments[i],command.types[i],r,histories[i]))return -3;
    return command.id;
}
void dispatch_command_line(char* line,CommandArguments& out,CommandParseHistory* histories,const CommandDispatchRuntime& r) {
    if(*line)r.history(line,r.user);
    normalize_input_line(line,0x8e,r.parser.characters);
    const int id=parse_command_arguments(line,out,r.parser,histories);
    if(id==-1) { r.signal(0,3,r.user);return; }
    r.dispatch(id,out,r.user);r.cleanup(r.user);
    if(!*r.dragging)r.redraw(r.user);
}
}
