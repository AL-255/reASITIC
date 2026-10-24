// Included after the geometry transaction helpers in bridge.cpp.
#include "optimization_plan.hpp"
#include "asitic/optimization_radius.hpp"
namespace {
OptimizationSweepState grid_settings;GeometryShape grid_candidates[1000];unsigned grid_count=0,grid_limit=0;
bool safe_grid_keywords(const char* line){
    if(!line||!*line||__builtin_strlen(line)>255)return false;char copy[256];__builtin_strcpy(copy,line);bool found=false;
    for(char* token=strtok(copy,":");token;token=strtok(nullptr,":")){
        found=true;
        char* value=token;while(*value&&*value!='=')++value;if(!*value||!value[1])return false;*value++=0;
        for(char* p=token;*p;++p)if(*p>='a'&&*p<='z')*p-=32;
        if(__builtin_strcmp(token,"NAME")==0||__builtin_strcmp(token,"FNAME")==0){if(__builtin_strlen(value)>74)return false;continue;}
        if(__builtin_strcmp(token,"METAL")==0||__builtin_strcmp(token,"EXIT")==0||__builtin_strcmp(token,"TAP")==0)continue;
        char* end;double n=strtod(value,&end);if(end==value||!__builtin_isfinite(n)||n>1000000||n< -1000000)return false;
        if(__builtin_strcmp(token,"SIDES")==0&&(n<3||n>256||n!=static_cast<int>(n)))return false;
    }return found;
}
bool retain_grid_candidate(const GeometryShape& shape,unsigned,void*){
    if(grid_count>=grid_limit)return false;
    creator_state.arguments[0]=shape;guard_native_creation(integer(creator_state.arguments[0],0x50),creator_state);
    grid_candidates[grid_count++]=shape;return true;
}
char* grid_descriptor(char* out,GeometryShape& s){
    return out+sprintf(out,"[%d,%d,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g]",integer(s,0x50),integer(s,0x64),integer(s,0x68),integer(s,0x6c),number(s,0x70),number(s,0x88),number(s,0x90),number(s,0x98),number(s,0x54),number(s,0x5c));
}
}
static const char* plan_optimization(const char* keywords,double chip_x,double chip_y,double grid,int snap,int limit,bool multimetal){
    reasitic::clear_diagnostics();
    if(!metal_count||!safe_grid_keywords(keywords))return "{\"error\":\"Use valid SWEEP keywords of at most 255 bytes\"}";
    if(!__builtin_isfinite(chip_x)||!__builtin_isfinite(chip_y)||chip_x<=0||chip_y<=0||!__builtin_isfinite(grid)||grid<=0||limit<1||limit>1000)return "{\"error\":\"Use positive chip/grid settings and a candidate budget from 1 to 1000\"}";
    pending=nullptr;pending_bytes=pending_count=0;creator_active=true;
    if(setjmp(creator_jump)){creator_active=false;char* out=result;out+=sprintf(out,"{\"error\":");out=reasitic::json_string(out,warnings);sprintf(out,"}");return result;}
    char input[256];__builtin_strcpy(input,keywords);grid_settings={};OptimizationSweepHistory history{};
    CommandRuntime parser{character_tables(),sscanf,strtok,bounded_format,message,{warn,nullptr},head,selected,1e9,1e-6,grid,snap!=0};
    ShapePromptRuntime prompt{parser,{metals,vias,heights,metal_count},via_count,chip_x,chip_y,invalid,nullptr};
    parse_optimization_sweep(input,grid_settings,prompt,history);
    if(!multimetal&&grid_settings.simulation_type==1)fail_creator("The original FAST SWEEP model faults for conductor candidates; select SIMTYPE=2 or SIMTYPE=3");
    if(multimetal&&grid_settings.sides==4&&(grid_settings.exit_metal<0||grid_settings.exit_metal>=grid_settings.metal))fail_creator("SWEEPMM squares require a lower EXIT metal");
    edit_name(grid_settings.shape_name);grid_count=0;grid_limit=static_cast<unsigned>(limit);
    const auto enumerate=multimetal?reasitic::enumerate_multimetal_grid:reasitic::enumerate_spiral_grid;
    if(!enumerate(grid_settings,chip_x,chip_y,retain_grid_candidate,nullptr))fail_creator("SWEEP exceeds the candidate or range-iteration budget");
    creator_active=false;char* out=result;out+=sprintf(out,"{\"name\":");out=reasitic::json_string(out,grid_settings.shape_name);
    if(multimetal)out+=sprintf(out,",\"search\":\"SWEEPMM\"");
    out+=sprintf(out,",\"frequency_hz\":%.17g,\"inductance_min\":%.17g,\"inductance_max\":%.17g,\"minimum_quality\":%.17g,\"simulation_type\":%d,\"candidates\":[",grid_settings.frequency,grid_settings.inductance_min,grid_settings.inductance_max,grid_settings.minimum_quality,grid_settings.simulation_type);
    for(unsigned i=0;i<grid_count;++i){if(i)*out++=',';out=grid_descriptor(out,grid_candidates[i]);}sprintf(out,"]}");return result;
}
extern "C" const char* optimization_plan(const char* keywords,double x,double y,double grid,int snap,int limit){return plan_optimization(keywords,x,y,grid,snap,limit,false);}
extern "C" const char* optimization_multimetal_plan(const char* keywords,double x,double y,double grid,int snap,int limit){return plan_optimization(keywords,x,y,grid,snap,limit,true);}
extern "C" int optimization_accept(double inductance,double quality,double minimum,double maximum,double minimum_quality){
    return reasitic::accept_spiral_grid(inductance,quality,minimum,maximum,minimum_quality)?1:0;
}
extern "C" const char* optimization_create(const char* name,const double* values,int append){
    reasitic::clear_diagnostics();
    if(!name||!values)return "{\"error\":\"Supply an optimizer name and descriptor\"}";
    for(int i=0;i<10;++i)if(!__builtin_isfinite(values[i])||values[i]>1000000||values[i]< -1000000)return "{\"error\":\"Use a finite optimizer descriptor within the supported range\"}";
    if(values[4]<=0||values[5]<=0||values[6]<0)return "{\"error\":\"Use positive optimizer dimensions and nonnegative spacing\"}";
    for(int i=0;i<4;++i)if(values[i]!=static_cast<int>(values[i]))return "{\"error\":\"Use integer optimizer shape/layer fields\"}";
    if((values[0]!=0&&values[0]!=1&&values[0]!=18&&values[0]!=20)||values[3]< -1||values[3]>=metal_count)return "{\"error\":\"Use a supported optimizer descriptor and valid exit metal\"}";
    original_head=head;auto* old_selected=selected;head=append?head:nullptr;selected=append?selected:nullptr;
    pending=nullptr;pending_bytes=pending_count=0;creator_active=true;
    if(setjmp(creator_jump)){creator_active=false;while(pending){auto* next=pending->next;free(pending);pending=next;}head=original_head;selected=old_selected;char* out=result;out+=sprintf(out,"{\"error\":");out=reasitic::json_string(out,warnings);sprintf(out,"}");return result;}
    edit_name(name);if(find_geometry_shape(head,selected,name))fail_creator("A shape already uses the optimizer candidate name");
    creator_state={};auto& a=creator_state.arguments[0];__builtin_strcpy(reinterpret_cast<char*>(&a),name);
    integer(a,0x50)=static_cast<int>(values[0]);integer(a,0x64)=static_cast<int>(values[1]);integer(a,0x68)=static_cast<int>(values[2]);integer(a,0x6c)=static_cast<int>(values[3]);
    const unsigned offsets[]={0x70,0x78,0x80,0x88,0x90,0x98,0x54,0x5c};const int fields[]={4,4,4,5,6,7,8,9};for(int i=0;i<8;++i)number(a,offsets[i])=values[fields[i]];
    guard_native_creation(integer(a,0x50),creator_state);
    if(values[0]==18){
        if(__builtin_strlen(name)>74)fail_creator("Use at most 74 characters for a multi-metal candidate name");
        for(int i=1;i<=values[2]-values[3];++i){char temporary[80];sprintf(temporary,"%s-%d",name,i);if(find_geometry_shape(head,selected,temporary))fail_creator("A multi-metal temporary name is already in use");}
    }
    if(values[7]<1||values[7]>x87::binary64(maximum_spiral_turns(values[4],values[5],values[6],integer(a,0x64),integer(a,0x50))))fail_creator("Optimizer descriptor exceeds the original turn range");
    const GeometryTechnology technology{metals,vias,heights,metal_count};
    SpiralConstructionRuntime runtime{geometry,via_count,{warn,nullptr},bounded_format,message,invalid,nullptr};const SquareConstructionHistory history{};
    if(values[0]==18){const GeometryCommandRuntime commands{runtime,selected,geometry.memory,random,nullptr};build_multimetal_square(head,a,technology,commands,history);}
    else if(values[0]==0)build_square_spiral(head,a,3,technology,runtime,history);else build_polygon_spiral(head,a,technology,runtime,history.mesh);
    if(serialize_geometry()!=result)fail_creator("Optimizer candidate is nonfinite or exceeds the export size limit");
    creator_active=false;while(pending){pending->tracked=false;pending=pending->next;}
    if(!append)release_edit_layout(original_head);reasitic::release_simulation_result();reasitic::release_capacitance_result();return result;
}
