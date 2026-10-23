#include "asitic/console_geometry.hpp"
extern "C" void* malloc(unsigned);
extern "C" void free(void*);
extern "C" int rand();
extern "C" [[noreturn]] void exit(int);
namespace asitic {
namespace {
void* allocate(unsigned bytes,void*) { return malloc(bytes); }
void release(void* address,void*) { free(address); }
void terminate(int code,void*) { exit(code); }
int random(void*) { return rand(); }
void zero(void* address,unsigned bytes) { auto* p=static_cast<volatile unsigned char*>(address);for(unsigned i=0;i<bytes;++i)p[i]=0; }
double number(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const double*>(reinterpret_cast<const unsigned char*>(&s)+offset); }
int integer(const GeometryShape& s,unsigned offset) { return *reinterpret_cast<const int*>(reinterpret_cast<const unsigned char*>(&s)+offset); }
int resolve(char* text,void* user) { auto& s=*static_cast<ConsoleGeometryState*>(user);return find_shape_prompt_metal(text,s.prompt,s.layer_history); }
void structure_violation(void* user) { auto& s=*static_cast<ConsoleGeometryState*>(user);s.invalid(s.user);__builtin_trap(); }
void sync(ConsoleGeometryState& s,CommandRuntime& parser) { parser.shapes=s.head;parser.selected=s.selected;s.commands.selected=s.selected; }
void rewrite_wire(CommandArguments& a,const GeometryShape& s,const CommandRuntime& r) {
    a.command.value=1;
    r.format(a.arguments[0].text,"NAME=%s:LEN=%.2g:WID=%.2g:METAL=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-2d",
        reinterpret_cast<const char*>(&s),number(s,0x70),number(s,0x88),integer(s,0x68),number(s,0x54),number(s,0x5c),number(s,0xa0),static_cast<int>(s.attributes));
}
void rewrite_square(CommandArguments& a,const GeometryShape& s,const CommandRuntime& r,int id) {
    a.command.value=1;const char* format;
    if(id==16)format="NAME=%s:LEN=%.2g:ILEN=%.2g:W=%.2g:S=%.2g:N=%.2f:METAL=%d:EXIT=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-2d";
    else if(id==18)format="NAME=%s:LEN=%.2g:WID=%.2g:W=%.2g:S=%.2g:N=%.2f:METAL=%d:NMET=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-2d";
    else format="NAME=%s:LEN=%.2g:WID=%.2g:W=%.2g:S=%.2g:N=%.2f:METAL=%d:EXIT=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-22d";
    r.format(a.arguments[0].text,format,reinterpret_cast<const char*>(&s),number(s,0x70),number(s,0x78),number(s,0x88),number(s,0x90),number(s,0x98),integer(s,0x68),integer(s,0x6c),number(s,0x54),number(s,0x5c),number(s,0xa0),static_cast<int>(s.attributes));
}
bool create(int id,CommandArguments& a,ConsoleGeometryState& s,bool& suppressed) {
    const bool keyword=a.command.value==1;const bool interactive=a.command.value==0;
    auto& first=s.arguments[0];auto& second=s.arguments[1];auto& h=s.keyword_history;h.call_index=0;
    auto& ch=s.creation_history;ch.layer_index=0;
    switch(id) {
    case 0:
        if(keyword)parse_square_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_square_creation(first,s.creation,ch);
        build_square_spiral(s.head,first,3,s.technology,s.commands.construction,s.square_history);
        if(interactive)rewrite_square(a,*s.head,s.prompt.command,id);
        break;
    case 1:
        if(keyword)parse_polygon_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_polygon_creation(first,s.creation,ch);
        build_polygon_spiral(s.head,first,s.technology,s.commands.construction,s.square_history.mesh);
        if(interactive) { a.command.value=1;const auto& q=*s.head;s.prompt.command.format(a.arguments[0].text,
            "NAME=%s:RAD=%.2g:SIDES=%d:W=%.2g:S=%.2g:N=%.2f:METAL=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-2d",reinterpret_cast<const char*>(&q),number(q,0x80),integer(q,0x64),number(q,0x88),number(q,0x90),number(q,0x98),integer(q,0x68),number(q,0x54),number(q,0x5c),number(q,0xa0),static_cast<int>(q.attributes)); }break;
    case 2:
        if(keyword)parse_wire_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_wire_creation(first,s.creation,ch);
        build_wire_shape(s.head,first,s.technology,s.geometry,s.square_history.mesh);if(interactive)rewrite_wire(a,*s.head,s.prompt.command);break;
    case 3:
        if(keyword)parse_transformer_shape_arguments(a.arguments[0].text,first,second,s.prompt,h);else prompt_transformer_creation(first,second,s.creation,ch);
        build_planar_transformer(s.head,first,second,s.technology,s.commands.construction,s.transformer_history);
        if(interactive) { const auto& p=*s.head->next;const auto& q=*s.head;a.command.value=1;s.prompt.command.format(a.arguments[0].text,
            "PNAME=%s:SNAME=%s:LEN=%.2g:WID=%.2g:W=%.2g:S=%.2g:NP=%.2f:NS=%.2f:METAL=%d:EXIT=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-2d",reinterpret_cast<const char*>(&p),reinterpret_cast<const char*>(&q),number(p,0x70),number(p,0x78),number(p,0x88),(number(p,0x90)-number(p,0x88))*.5,number(p,0x98),number(q,0x98),integer(p,0x68),integer(p,0x6c),number(p,0x54),number(p,0x5c),number(p,0xa0),static_cast<int>(p.attributes)); }break;
    case 4:
        if(keyword)parse_capacitor_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_capacitor_creation(first,s.creation,ch);
        build_wire_shape(s.head,first,s.technology,s.geometry,s.square_history.mesh);
        *reinterpret_cast<unsigned short*>(&first)=0x2e;
        *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&first)+0x68)=integer(first,0x6c);
        build_wire_shape(s.head,first,s.technology,s.geometry,s.square_history.mesh);
        join_geometry_shapes(s.head,*s.head->next,*s.head,s.technology,s.commands);
        if(interactive) { const auto& q=*s.head;a.command.value=1;s.prompt.command.format(a.arguments[0].text,
            "NAME=%s:LEN=%.2g:WID=%.2g:METAL1=%d:METAL2=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f",reinterpret_cast<const char*>(&q),number(q,0x70),number(q,0x88),integer(q,0x68),integer(q,0x6c),number(q,0x54),number(q,0x5c),number(q,0xa0)); }break;
    case 5:
        if(keyword)parse_balun_shape_arguments(a.arguments[0].text,first,second,s.prompt,h);else prompt_balun_creation(first,second,s.creation,ch);
        build_symmetric_square(s.head,first,s.technology,s.commands.construction,s.symmetric_history);
        build_symmetric_square(s.head,second,s.technology,s.commands.construction,s.symmetric_history);
        reflect_geometry_x(*s.head);reverse_geometry_directions(*s.head);
        s.head->auxiliary=reinterpret_cast<unsigned>(s.head->next);s.head->next->auxiliary=reinterpret_cast<unsigned>(s.head);
        if(interactive) {
            const auto& p=*s.head->next;const auto& q=*s.head;char name[80];
            unsigned i=0;const char* input=reinterpret_cast<const char*>(&p);for(;i<80 && input[i];++i)name[i]=input[i];for(;i<80;++i)name[i]=0;
            char* end=name;while(!(end[0]=='-' && end[1]=='P'))++end;*end=0;
            a.command.value=1;s.prompt.command.format(a.arguments[0].text,
                "NAME=%s:LEN=%.2g:W1=%.2g:W2=%.2g:S=%.2g:N=%.2f:METAL=%d:METAL2=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f",name,number(p,0x70),number(p,0x88),number(q,0x88),(number(p,0x90)-number(q,0x88))*.5,number(q,0x98)+number(p,0x98),integer(p,0x68),integer(p,0x6c),number(p,0x54),number(p,0x5c),number(p,0xa0));
        }break;
    case 10:
        prompt_3d_transformer_creation(first,second,s.creation,ch);suppressed=true;
        build_3d_transformer(s.head,first,second,s.technology,s.commands.construction,s.transformer3d_history);break;
    case 14:
        if(s.prompt.via_count==0)s.prompt.command.warnings.emit("Error:  No Via layers defined in technology file.");
        else { if(keyword)parse_via_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_via_creation(first,s.creation,ch);
            build_via_shape(s.head,first,s.technology,s.geometry,s.square_history.via); }
        if(interactive) { const auto& q=*s.head;a.command.value=1;s.prompt.command.format(a.arguments[0].text,
            "NAME=%s:NX=%-2d:NY=%-2d:VIA=%d:XORG=%.2g:YORG=%.2g:PHASE=%-2d",reinterpret_cast<const char*>(&q),integer(q,0x64),integer(q,0x6c),integer(q,0x68),number(q,0x54),number(q,0x5c),static_cast<int>(q.attributes)); }break;
    case 16:
        if(keyword)parse_symmetric_square_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_symmetric_square_creation(first,s.creation,ch);
        build_symmetric_square(s.head,first,s.technology,s.commands.construction,s.symmetric_history);
        if(interactive)rewrite_square(a,*s.head,s.prompt.command,id);
        break;
    case 17:
        if(keyword)parse_symmetric_polygon_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_symmetric_polygon_creation(first,s.creation,ch);
        build_symmetric_polygon(s.head,first,s.technology,s.commands.construction,s.symmetric_history);
        if(interactive) { const auto& q=*s.head;a.command.value=1;s.prompt.command.format(a.arguments[0].text,
            "NAME=%s:RAD=%.2g:ILEN=%.2g:W=%.2g:S=%.2g:N=%.2f:METAL=%d:EXIT=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-2d",reinterpret_cast<const char*>(&q),number(q,0x80),number(q,0x78),number(q,0x88),number(q,0x90),number(q,0x98),integer(q,0x68),integer(q,0x6c),number(q,0x54),number(q,0x5c),number(q,0xa0),static_cast<int>(q.attributes)); }break;
    case 18:
        if(keyword)parse_square_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_multimetal_square_creation(first,s.creation,ch);
        build_multimetal_square(s.head,first,s.technology,s.commands,s.square_history);
        if(interactive)rewrite_square(a,*s.head,s.prompt.command,id);
        break;
    case 22:
        if(keyword)parse_ring_shape_arguments(a.arguments[0].text,first,s.prompt,h);else prompt_ring_creation(first,s.creation,ch);
        build_ring_shape(s.head,first,s.technology,s.commands.construction,s.square_history.mesh);
        if(interactive) { const auto& q=*s.head;a.command.value=1;s.prompt.command.format(a.arguments[0].text,
            "NAME=%s:RAD=%.2g:W=%.2g:GAP=%.2g:METAL=%d:XORG=%.2g:YORG=%.2g:ORIENT=%.2f:PHASE=%-2d",reinterpret_cast<const char*>(&q),number(q,0x80),number(q,0x88),number(q,0x90),integer(q,0x68),number(q,0x54),number(q,0x5c),number(q,0xa0),static_cast<int>(q.attributes)); }break;
    default:return false;
    }return true;
}
}
void initialize_console_geometry(ConsoleGeometryState& s,const TechnologyInitialization& technology,CommandRuntime parser,
    void (*read)(const char*,char*,void*),void (*invalid)(void*),void* user) {
    zero(&s,sizeof(s));s.geometry={{{allocate,nullptr},release},parser.warnings,terminate,nullptr};
    s.invalid=invalid;s.user=user;
    s.technology={reinterpret_cast<const MetalLayer*>(technology.tables.metals),reinterpret_cast<const ViaLayer*>(technology.tables.vias),technology.metal_heights,technology.tables.metal_count};
    s.prompt={parser,s.technology,technology.tables.via_count,technology.input.width,technology.input.height,invalid,user};
    s.creation={s.prompt,read,user,technology.input.fft_x,technology.input.fft_y};
    s.commands={{s.geometry,technology.tables.via_count,parser.warnings,parser.format,parser.message,structure_violation,&s},nullptr,s.geometry.memory,random,nullptr};
    s.view.visible_metals=static_cast<int*>(malloc(static_cast<unsigned>(technology.tables.metal_count)*4));
    for(int i=0;i<technology.tables.metal_count;++i)s.view.visible_metals[i]=1;
}
void refresh_console_geometry(ConsoleGeometryState& s,const TechnologyInitialization& technology,CommandRuntime& parser,const NativeDispatchState& native) {
    sync(s,parser);s.technology={reinterpret_cast<const MetalLayer*>(technology.tables.metals),reinterpret_cast<const ViaLayer*>(technology.tables.vias),technology.metal_heights,technology.tables.metal_count};
    s.prompt.command=parser;s.prompt.technology=s.technology;s.prompt.via_count=technology.tables.via_count;s.prompt.chip_x=native.chip_width;s.prompt.chip_y=native.chip_height;
    s.creation.shape=s.prompt;s.creation.cells_x=technology.input.fft_x;s.creation.cells_y=technology.input.fft_y;
    s.commands.construction.geometry=s.geometry;s.commands.construction.via_count=technology.tables.via_count;
    s.commands.construction.warnings=parser.warnings;s.commands.construction.format=parser.format;s.commands.construction.message=parser.message;
}
bool execute_console_geometry_command(int id,CommandArguments& arguments,ConsoleGeometryState& s,const TechnologyInitialization& technology,CommandRuntime& parser,NativeDispatchState& native,bool* suppress_recording) {
    refresh_console_geometry(s,technology,parser,native);bool suppressed=false;
    if(create(id,arguments,s,suppressed)) { sync(s,parser);if(suppress_recording)*suppress_recording=suppressed;return true; }
    GeometryCommandState state{s.head,s.selected,native.chip_width,native.chip_height};
    const GeometryDispatchRuntime edits{s.commands,s.technology,parser.characters,resolve,&s};
    if(execute_geometry_command(id,arguments,state,edits,&suppressed)) {
        s.head=state.head;s.selected=state.selected;sync(s,parser);if(suppress_recording)*suppress_recording=suppressed;return true;
    }
    if(execute_geometry_reporting_command(id,arguments,s.head,s.technology,parser))return true;
    if(execute_view_command(id,arguments,native,s.view,s.prompt,s.view_history,&suppressed)) { if(suppress_recording)*suppress_recording=suppressed;return true; }
    return false;
}
void release_console_geometry(ConsoleGeometryState& s) {
    if(s.head)release_geometry_shapes(s.head,s.geometry.memory);
    s.head=s.selected=nullptr;
    free(s.view.visible_metals);s.view.visible_metals=nullptr;
}
}
