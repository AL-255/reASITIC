// Included by bridge.cpp to share its allocator and atomic creation boundary.
namespace {
GeometryShape* edit_original_head=nullptr;GeometryShape* edit_original_selected=nullptr;
struct EditCopySlot { unsigned from,to,kind; };
struct EditCopyMap {
    EditCopySlot* slots;unsigned size;
    EditCopySlot& slot(unsigned key){unsigned i=((key>>4)*2654435761u)&(size-1);while(slots[i].from&&slots[i].from!=key)i=(i+1)&(size-1);return slots[i];}
    unsigned lookup(unsigned key){if(!key)return 0;auto& s=slot(key);if(!s.from)fail_creator("Invalid geometry link in the current layout");return s.to;}
    void add(const void* source,unsigned bytes,unsigned kind){auto& s=slot(reinterpret_cast<unsigned>(source));if(s.from)return;s.from=reinterpret_cast<unsigned>(source);s.to=reinterpret_cast<unsigned>(allocate(bytes,nullptr));s.kind=kind;__builtin_memcpy(reinterpret_cast<void*>(s.to),source,bytes);}
};
ConductorSegment* edit_segment(unsigned p){return reinterpret_cast<ConductorSegment*>(p);}
void release_edit_layout(GeometryShape* list){while(list){auto* next=list->next;list->next=nullptr;if(list->filaments.first)release_geometry_shapes(list,geometry.memory);else release(list,nullptr);list=next;}}
bool edit_contains(GeometryShape* p){for(auto* s=head;s;s=s->next)if(s==p)return true;return false;}
// Native solver clones may discretize records and reverse branch order. An
// edit transaction instead copies every owned record first, then rebases its
// links. A secondary record's next link can alias another shape's main chain
// after SPLIT, so copying independent trees would change later transforms.
void copy_edit_layout(){
    unsigned count=0,shapes=0;
    for(auto* s=head;s;s=s->next){if(++shapes>4096)fail_creator("Layout has too many shape records to edit");++count;
        for(auto* p=s->filaments.first;p;p=edit_segment(p->next_address)){if(++count>60000)fail_creator("Layout has too many segment records to edit");for(auto* b=edit_segment(p->group_next_address);b;b=edit_segment(b->group_next_address))if(++count>60000)fail_creator("Layout has too many parallel records to edit");}}
    unsigned size=2;while(size<count*2)size*=2;
    EditCopyMap map{static_cast<EditCopySlot*>(allocate(size*sizeof(EditCopySlot),nullptr)),size};
    for(auto* s=head;s;s=s->next){map.add(s,sizeof(*s),1);for(auto* p=s->filaments.first;p;p=edit_segment(p->next_address)){map.add(p,sizeof(*p),2);for(auto* b=edit_segment(p->group_next_address);b;b=edit_segment(b->group_next_address))map.add(b,sizeof(*b),2);}}
    for(unsigned i=0;i<size;++i){auto& entry=map.slots[i];if(entry.kind==1){auto* s=reinterpret_cast<GeometryShape*>(entry.to);s->next=reinterpret_cast<GeometryShape*>(map.lookup(reinterpret_cast<unsigned>(s->next)));s->auxiliary=map.lookup(s->auxiliary);s->filaments.first=reinterpret_cast<const ConductorSegment*>(map.lookup(reinterpret_cast<unsigned>(s->filaments.first)));}
        else if(entry.kind==2){auto* p=edit_segment(entry.to);p->next_address=map.lookup(p->next_address);p->group_next_address=map.lookup(p->group_next_address);}}
    head=reinterpret_cast<GeometryShape*>(map.lookup(reinterpret_cast<unsigned>(head)));selected=reinterpret_cast<GeometryShape*>(map.lookup(reinterpret_cast<unsigned>(selected)));release(map.slots,nullptr);
}
bool edit_supported(int id){return id==400||id==401||id==402||(id>=404&&id<=409)||id==411||id==412||(id>=414&&id<=418)||(id>=420&&id<=424);}
void edit_name(const char* text){unsigned n=0;for(;text[n];++n)if(n>=79||static_cast<unsigned char>(text[n])<33||static_cast<unsigned char>(text[n])>126)fail_creator("Use a name of 1–79 printable ASCII characters");if(!n||__builtin_strcmp(text,".")==0)fail_creator("The name '.' is reserved for the selected shape");}
void validate_edit_arguments(int id,CommandArguments& a){
    auto shape=[&](int i){return reinterpret_cast<GeometryShape*>(a.arguments[i].value);};const int n=a.command.value;
    if(id==400||id==402||id==416){const char* name=a.arguments[id==416?2:1].text;edit_name(name);if(find_geometry_shape(head,selected,name))fail_creator("A shape already uses the requested name");}
    if(id==401||id==409||id==421)for(int i=0;i<n;++i)for(int j=i+1;j<n;++j)if(shape(i)==shape(j))fail_creator("Use each shape only once in an edit command");
    if(id==422){auto* target=n?find_geometry_shape(head,selected,a.arguments[0].text):head;if(!target)fail_creator("Select an existing shape");}
    if(id==416){int count=0;for(auto* p=shape(0)->filaments.first;p;p=edit_segment(p->next_address))++count;int index=static_cast<int>(a.arguments[1].value);if(count<2||integer(*shape(0),0x64)==0||index>count-1||index< -count)fail_creator("Split needs a nonempty tail and a valid side count");}
    if(id==421){int target=0;for(auto* p=shape(0)->filaments.first;p;p=edit_segment(p->next_address))++target;for(int i=1;i<n;++i){int source=0;for(auto* p=shape(i)->filaments.first;p;p=edit_segment(p->next_address))++source;if(source>target)fail_creator("JOINSHUNT needs at least as many target segments as source segments");}}
    if(id==424&&__builtin_strlen(reinterpret_cast<const char*>(shape(0)))>65)fail_creator("Use a name of at most 65 characters before SPTOWIRE");
    if(id==417){auto* p=shape(0);auto* q=shape(1);if(p==q||(p->auxiliary&&p->auxiliary!=reinterpret_cast<unsigned>(q))||(q->auxiliary&&q->auxiliary!=reinterpret_cast<unsigned>(p)))fail_creator("Unfriend an existing pair before befriending another shape");}
    if(id==418&&(!shape(0)->auxiliary||shape(0)->auxiliary!=a.arguments[1].value||shape(1)->auxiliary!=a.arguments[0].value))fail_creator("The specified shapes are not friends");
}
int resolve_edit_metal(char* text,void* user){auto& prompt=*static_cast<ShapePromptRuntime*>(user);int history=0;return find_shape_prompt_metal(text,prompt,history);}
// Native destructive edits can retain selected/friend pointers to a removed
// record. These relationships are not physical conductors; drop stale links.
void validate_edit_result(){
    if(selected&&!edit_contains(selected))selected=nullptr;
    unsigned total=0,shapes=0;
    for(auto* s=head;s;s=s->next){if(++shapes>4096)fail_creator("Edited layout has too many shapes");if(s->auxiliary&&!edit_contains(reinterpret_cast<GeometryShape*>(s->auxiliary)))s->auxiliary=0;
        edit_name(reinterpret_cast<const char*>(s));for(auto* other=s->next;other;other=other->next)if(__builtin_strcmp(reinterpret_cast<const char*>(s),reinterpret_cast<const char*>(other))==0)fail_creator("The edit created duplicate shape names");
        if(!s->filaments.first)fail_creator("The edit created an empty shape");
        for(auto* p=s->filaments.first;p;p=edit_segment(p->next_address))for(auto* b=p;b;b=edit_segment(b->group_next_address)){
            if(++total>60000)fail_creator("Edited layout has too many segments");const double* points[]={b->endpoints,b->endpoints+3,b->outer_start,b->outer_end,b->inner_start,b->inner_end};
            for(const double* point:points)for(int k=0;k<3;++k)if(!__builtin_isfinite(point[k]))fail_creator("The edit produced nonfinite geometry");
            for(const double* point:points)for(int k=0;k<2;++k){const Real coordinate=Real(point[k])+number(*s,k?0x5c:0x54);if(coordinate>Real(2147483.647)||coordinate<Real(-2147483.648))fail_creator("Edited geometry exceeds the GDS coordinate range");}
        }
    }
}
}

extern "C" const char* edit_geometry(const char* commands,double chip_x,double chip_y,double grid,int snap){
    if(!commands||!*commands||__builtin_strlen(commands)>32768)return "{\"error\":\"Use a geometry edit batch of 1–32768 bytes\"}";
    if(!head)return "{\"error\":\"Generate a layout before editing geometry\"}";
    if(!__builtin_isfinite(chip_x)||!__builtin_isfinite(chip_y)||chip_x<=0||chip_y<=0||!__builtin_isfinite(grid)||grid<=0)return "{\"error\":\"Use positive finite chip and grid settings\"}";
    reasitic::clear_diagnostics();edit_original_head=head;edit_original_selected=selected;pending=nullptr;pending_bytes=pending_count=0;creator_active=true;editing_active=true;edit_rejected=false;
    if(setjmp(creator_jump)){
        creator_active=editing_active=false;while(pending){auto* next=pending->next;free(pending);pending=next;}head=edit_original_head;selected=edit_original_selected;
        char* out=result;out+=sprintf(out,"{\"error\":");out=reasitic::json_string(out,warnings);sprintf(out,"}");return result;
    }
    copy_edit_layout();const unsigned bytes=__builtin_strlen(commands)+1;auto* script=static_cast<char*>(allocate(bytes,nullptr));__builtin_memcpy(script,commands,bytes);
    unsigned count=0;char* cursor=script;
    while(*cursor){auto* line=cursor;while(*cursor&&*cursor!='\n'&&*cursor!='\r')++cursor;if(*cursor)*cursor++=0;while(*line==' '||*line=='\t')++line;if(!*line||*line=='#')continue;
        if(++count>128||__builtin_strlen(line)>4095)fail_creator("Use at most 128 edit commands with lines below 4096 bytes");
        char first[80];unsigned i=0;while(line[i]&&line[i]!=' '&&line[i]!=','&&line[i]!='\t'){if(i>=79)fail_creator("Geometry command name is too long");first[i]=line[i]>='a'&&line[i]<='z'?line[i]-32:line[i];++i;}first[i]=0;
        const int index=lookup_command_index(first);if(index<0||!edit_supported(native_command_catalog()[index].id))fail_creator("Use an original geometry editing command");const auto& definition=native_command_catalog()[index];
        unsigned tokens=0,length=0;for(char* p=line;;++p){if(!*p||*p==' '||*p==','||*p=='\t'){if(length){if(length>255)fail_creator("Geometry argument exceeds 255 bytes");if(tokens>=16)fail_creator("Geometry commands accept at most fifteen arguments");if(tokens){const unsigned type=definition.types[tokens-1];if(type==6||type==7||type==10){char* end;const char* start=p-length;const double value=strtod(start,&end);if(end!=p||!__builtin_isfinite(value)||value>1000000||value< -1000000||(type==6&&value!=static_cast<int>(value)))fail_creator("Use finite numeric geometry arguments within the supported range");}}++tokens;length=0;}if(!*p)break;}else ++length;}if(tokens>16)fail_creator("Geometry commands accept at most fifteen arguments");
        CommandArguments arguments{};CommandParseHistory history[15]{};
        CommandRuntime parser{character_tables(),sscanf,strtok,bounded_format,message,{warn,nullptr},head,selected,1e9,1e-6,grid,snap!=0};
        const int id=parse_command_arguments(line,arguments,parser,history);if(id<0)fail_creator("Invalid geometry edit arguments");
        validate_edit_arguments(id,arguments);
        ShapePromptRuntime prompt{parser,{metals,vias,heights,metal_count},via_count,chip_x,chip_y,invalid,nullptr};
        const GeometryCommandRuntime runtime{{geometry,via_count,{warn,nullptr},bounded_format,message,invalid,nullptr},selected,geometry.memory,random,nullptr};
        GeometryCommandState state{head,selected,chip_x,chip_y};GeometryDispatchRuntime dispatch{runtime,{metals,vias,heights,metal_count},character_tables(),resolve_edit_metal,&prompt};
        if(!execute_geometry_command(id,arguments,state,dispatch))fail_creator("Unsupported geometry edit command");head=state.head;selected=state.selected;
        if(edit_rejected)fail_creator("Original geometry edit rejected these arguments");validate_edit_result();
    }
    if(!count)fail_creator("Enter at least one geometry edit command");release(script,nullptr);
    const char* serialized=serialize_geometry();if(serialized!=result)fail_creator("Edited layout is nonfinite or exceeds the export buffer");
    creator_active=editing_active=false;while(pending){pending->tracked=false;pending=pending->next;}
    release_edit_layout(edit_original_head);reasitic::release_simulation_result();reasitic::release_capacitance_result();return serialized;
}
