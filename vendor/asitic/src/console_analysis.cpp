#include "asitic/console_analysis.hpp"
#include "asitic/console_diagnostics.hpp"
extern "C" void* fopen(const char*,const char*);
extern "C" int fclose(void*);
extern "C" int fflush(void*);
extern "C" unsigned fread(void*,unsigned,unsigned,void*);
extern "C" unsigned fwrite(const void*,unsigned,unsigned,void*);
extern "C" int fprintf(void*,const char*,...);
extern "C" int printf(const char*,...);
extern "C" int sprintf(char*,const char*,...);
extern "C" int time(int*);
extern "C" double difftime(int,int);
extern "C" int gettimeofday(int*,int*);
extern "C" char* ctime(const int*);
extern "C" void* realloc(void*,unsigned);
extern "C" void free(void*);
extern "C" double sin(double);
extern "C" double cos(double);
extern "C" void* stdout;

namespace asitic {
namespace {
void* open(const char* path,const char* mode,void*) { return ::fopen(path,mode); }
int close(void* file,void*) { return ::fclose(file); }
int flush(void* file,void*) { return ::fflush(file); }
unsigned read(void* out,unsigned size,unsigned count,void* file,void*) { return ::fread(out,size,count,file); }
unsigned write(const void* in,unsigned size,unsigned count,void* file,void*) { return ::fwrite(in,size,count,file); }
int clock(int* value,void*) { return ::time(value); }
int timer(int* value,int* zone,void*) { return ::gettimeofday(value,zone); }
double difference(int last,int first,void*) { return ::difftime(last,first); }
long double sine(double x) { return ::sin(x); }
long double cosine(double x) { return ::cos(x); }
double rounded(long double x) { volatile double out=static_cast<double>(x);return out; }
long double load(const double& value) { long double out;asm volatile("fldl %1":"=t"(out):"m"(value));return out; }
void elapsed(double seconds,void* user) {
    (void)user;::printf("\nAbout %lg second(s) end-to-end.",seconds);
}
void pi_report(const PiEquivalent& pi,double frequency,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);const auto& r=s.report;
    r.format(r.message,"Pi Model at f=%-4.2lf GHz:  Q = %4.3lf,%4.3lf,%4.3lf",rounded(load(frequency)/1000000000.L),pi.quality_first,pi.quality_second,pi.quality_differential);r.warnings.emit(r.message);
    r.format(r.message,"L  =%6.3lg nH\tR  =%6.3lg\nCs1=%6.3lg fF\tRs1=%6.3lg\nCs2=%6.3g fF\tRs2=%6.3lg\tf_res = %5.2lfGHz",
        rounded(load(pi.inductance)/load(1e-9)),pi.resistance,rounded(load(pi.capacitance_second)/load(1e-15)),pi.resistance_second,
        rounded(load(pi.capacitance_first)/load(1e-15)),pi.resistance_first,rounded(pi_resonant_frequency(pi)*load(1e-9)));r.warnings.emit(r.message);
}
void transformer_report(const TransformerEquivalent& model,double frequency,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);const auto& r=s.report;
    r.format(r.message,"Narrowband Model at f=%-4.2lf GHz:",rounded(load(frequency)/1000000000.L));r.warnings.emit(r.message);
    r.format(r.message,"L1=%6.3lg  R1=%6.3lg  L2=%6.3lg R2=%6.3lg M=%6.3lg (k=%6.3lg) RG = %6.3lg",
        rounded(load(model.first_inductance)/load(1e-9)),model.first_resistance,rounded(load(model.second_inductance)/load(1e-9)),model.second_resistance,
        rounded(load(model.mutual)/load(1e-9)),model.coupling,model.ground_resistance);r.warnings.emit(r.message);
}
void three_report(const ThreePortEquivalent& m,double frequency,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);const auto& r=s.report;
    r.format(r.message,"Narrowband Model at f=%-4.2lf GHz:",rounded(load(frequency)/1000000000.L));r.warnings.emit(r.message);
    r.format(r.message,"L1  = %6.3lg nH\tR1  = %6.3lg\nL2  = %6.3lg nH\tR2  = %6.3lg\nL3  = %6.3lg nH\tR3  = %6.3lg\nM12 = %6.3lg nH\tk12 = %6.3lg\tRe(Z12) = %6.3lg\nM13 = %6.3lg nH\tk13 = %6.3lg\tRe(Z13) = %6.3lg\nM23 = %6.3lg nH\tk23 = %6.3lg\tRe(Z23) = %6.3lg",
        rounded(load(m.inductance[0])*1000000000.L),m.resistance[0],rounded(load(m.inductance[1])*1000000000.L),m.resistance[1],rounded(load(m.inductance[2])*1000000000.L),m.resistance[2],
        rounded(load(m.mutual[0])*1000000000.L),m.coupling[0],m.cross_resistance[0],rounded(load(m.mutual[1])*1000000000.L),m.coupling[1],m.cross_resistance[1],rounded(load(m.mutual[2])*1000000000.L),m.coupling[2],m.cross_resistance[2]);r.warnings.emit(r.message);
}
void matrix_dump(const char* path,const ComplexMatrix& matrix,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);if(!s.controls.dump_matrices)return;
    void* file=::fopen(path,"w");
    for(int i=0;i<matrix.rows;++i)for(int j=0;j<matrix.rows;++j) { const auto z=matrix.view().at(i,j);::fprintf(file,"\n%10d %10d %20.10lg %20.10lg",i+1,j+1,z.real,z.imag); }
    ::fclose(file);
}
void node_dump(const char* path,ComplexMatrixView matrix,int size,void* user) {
    ComplexMatrix out{matrix.data,size*size,1,size,size,matrix.stride,1};matrix_dump(path,out,user);
}
void real_dump(const char* path,RealMatrixView matrix,int rows,int columns,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);if(!s.controls.dump_matrices)return;
    void* file=::fopen(path,"w");for(int i=0;i<rows;++i)for(int j=0;j<columns;++j)::fprintf(file,"\n%10d %10d %20.10lg",i+1,j+1,matrix.at(i,j));::fclose(file);
}
void cache_status(const char* text,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);
    if(text[0]=='.' && !text[1])s.progress_output.emit(text);
    else s.status.emit(text);
}
void copy_capacitance(ConsoleAnalysisState& s) {
    s.combined_state.capacitance=s.capacitance;s.transformer_state.capacitance=s.capacitance;s.extended_state.network.capacitance=s.capacitance;
    s.capacitive_state.capacitance=s.capacitance;s.three_port_state.capacitance=s.capacitance;
    s.combined_state.shapes=s.transformer_state.shapes=s.extended_state.network.shapes=s.three_port_state.shapes=*s.shapes;
}
bool same_cache(const PotentialCapacitanceState& a,const PotentialCapacitanceState& b) {
    for(unsigned i=0;i<8;++i)if(reinterpret_cast<const unsigned char*>(&a.cache.frequency)[i]!=reinterpret_cast<const unsigned char*>(&b.cache.frequency)[i])return false;
    return a.groups==b.groups && a.cache.enabled==b.cache.enabled && a.cache.dirty==b.cache.dirty;
}
void retain_capacitance(ConsoleAnalysisState& s) {
    PotentialCapacitanceState* banks[]={&s.combined_state.capacitance,&s.transformer_state.capacitance,&s.extended_state.network.capacitance,&s.capacitive_state.capacitance,&s.three_port_state.capacitance};
    for(auto* bank:banks)if(!same_cache(*bank,s.capacitance)) { s.capacitance=*bank;break; }
    GeometryShape* heads[]={s.combined_state.shapes,s.transformer_state.shapes,s.extended_state.network.shapes,s.three_port_state.shapes};
    for(auto* head:heads)if(head!=*s.shapes) { *s.shapes=head;break; }
}
PortOutputRuntime port_output(ConsoleAnalysisState& s) { return {::fprintf,s.geometry.memory,{s.report.warnings,nullptr,nullptr},s.report.warnings}; }
void header(void* file,unsigned char fast,const GeometryShape* shape,const char* command,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);const PortFileHeaderRuntime runtime{port_output(s),s.characters,::ctime};
    write_port_file_header(file,fast,*shape,command,s.port_header,s.port_header_history,runtime);
}
void two_columns(void* file,int format,unsigned char polar,void* user) { write_two_port_header(file,format,polar,port_output(*static_cast<ConsoleAnalysisState*>(user))); }
void three_columns(void* file,void* user) { auto& s=*static_cast<ConsoleAnalysisState*>(user);write_three_port_header(file,s.port_settings.parameter_format,s.port_settings.polar,port_output(s)); }
void frequency_update(double frequency,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);update_frequency_geometry(frequency,s.technology->tables.metals,s.technology->tables.metal_count,s.controls.alpha,s.controls.beta,s.controls.limits,s.controls.progress!=0,s.report);refresh_console_analysis(s);
}
void store_twoport(ConsoleAnalysisState& s,const GeometryShape& shape,int analytical) {
    const int kind=*reinterpret_cast<const int*>(shape.filaments.prefix+0x50);
    if(kind==4)s.admittance=s.capacitive_state.ports;
    else if(analytical) { s.admittance=s.analytical_result.admittance;s.pi=s.analytical_result.pi; }
    else { s.admittance=s.combined_state.ports;s.pi=s.combined_state.pi; }
}
void store_transformer(ConsoleAnalysisState& s) {
    const auto& t=s.transformer_state.model;s.admittance=s.transformer_state.ports;
    // The original Pi and transformer reports share the scalar bank starting
    // at0x80d8828. Preserve those overlapping writes across command families.
    s.pi.inductance=t.first_inductance;s.pi.resistance=t.second_inductance;s.pi.capacitance_second=t.coupling;s.pi.capacitance_first=t.ground_resistance;
    s.pi.resistance_second=t.first_resistance;s.pi.resistance_first=t.second_resistance;s.pi.quality_differential=t.mutual;
}
bool analyze(PortSweepKind kind,const GeometryShape* const* shapes,double frequency,double termination,unsigned char option,bool print,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);copy_capacitance(s);bool result;
    if(kind==PortSweepKind::Ground || kind==PortSweepKind::Extended) {
        result=kind==PortSweepKind::Ground?analyze_grounded_twoport(*shapes[0],frequency,option,*shapes[1],1,s.twoport):analyze_extended_twoport(*shapes[0],frequency,option,*shapes[1],1,s.twoport);
        s.controls.limits=s.twoport.limits;
        store_twoport(s,*shapes[0],option);
        const int shape_kind=*reinterpret_cast<const int*>(shapes[0]->filaments.prefix+0x50);
        if(kind==PortSweepKind::Extended && !option && (shape_kind==0 || shape_kind==1 || shape_kind==16 || shape_kind==17 || shape_kind==18)) { s.admittance=s.extended_state.network.ports;s.pi=s.extended_state.network.pi; }
    } else if(kind==PortSweepKind::Transformer) {
        auto options=s.transformer_options;options.print=print;result=solve_transformer_network(*shapes[0],*shapes[1],*shapes[2],frequency,termination,s.transformer_state,options);store_transformer(s);
    } else if(kind==PortSweepKind::Pad) {
        auto options=s.pad_options;options.print=print;result=solve_pad_deembedded_network(*shapes[0],*shapes[1],*shapes[2],*shapes[3],frequency,s.combined_state,options);s.admittance=s.combined_state.ports;s.pi=s.combined_state.pi;
    } else {
        auto options=s.three_port_options;options.print=print;result=solve_three_port_network(*shapes[0],*shapes[1],*shapes[2],*const_cast<GeometryShape*>(shapes[3]),frequency,s.three_port_state,options);
    }
    retain_capacitance(s);return result;
}
void two_record(void* file,double frequency,int parameter,unsigned char polar,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);auto& out=s.two_port_output;out.admittance=s.admittance;
    const auto& p=s.pi;out.model[0]=p.inductance;out.model[1]=p.resistance;out.model[2]=p.capacitance_second;out.model[3]=p.capacitance_first;out.model[4]=p.resistance_second;out.model[5]=p.resistance_first;out.model[6]=p.quality_first;out.model[7]=p.quality_second;out.model[8]=p.quality_differential;
    write_two_port_record(file,frequency,parameter,polar,out,::fprintf);
}
void three_record(void* file,double frequency,void* user) {
    auto& s=*static_cast<ConsoleAnalysisState*>(user);s.three_port_output.impedance=s.three_port_state.impedance;
    write_three_port_record(file,frequency,s.port_settings.parameter_format,s.port_settings.polar,s.three_port_output,port_output(s));
}
void replace_filename(PortSweepFilename& name,unsigned position,unsigned remove,const char* text,unsigned length,void*) {
    if(position>name.length)position=name.length;
    if(remove>name.length-position)remove=name.length-position;
    const unsigned size=name.length-remove+length;char* out=static_cast<char*>(::realloc(name.data,size+1));
    if(length>remove)for(unsigned i=name.length-position-remove;i;--i)out[position+length+i-1]=out[position+remove+i-1];
    else for(unsigned i=0;i<name.length-position-remove;++i)out[position+length+i]=out[position+remove+i];
    for(unsigned i=0;i<length;++i)out[position+i]=text[i];
    out[size]=0;name.data=out;name.length=size;
}
}

bool initialize_console_analysis(ConsoleAnalysisState& s,TechnologyInitialization& technology,GeometryShape*& shapes,
    const GeometryRuntime& geometry,const GeometryCommandRuntime& commands,const AnalysisReportRuntime& report,CharacterTables characters) {
    for(unsigned i=0;i<sizeof(s);++i)reinterpret_cast<volatile unsigned char*>(&s)[i]=0;
    s.technology=&technology;s.shapes=&shapes;s.geometry=geometry;s.commands=commands;s.report=report;s.status=s.progress_output=report.warnings;s.characters=characters;
    s.controls={{technology.half_width,technology.half_height,technology.thickness_cell,technology.length_step,technology.width_step},.05,.8,500,0,1,1,0,0,1,1};
    s.capacitance.coefficients=technology.coefficients;
    s.cache_allocated=allocate_potential_cache(s.capacitance.cache,technology.input.fft_x,technology.input.fft_y,technology.tables.metal_count,geometry.memory,report.warnings);
    if(!s.cache_allocated)return false;
    const auto metals=reinterpret_cast<const MetalLayer*>(technology.tables.metals);const auto vias=reinterpret_cast<const ViaLayer*>(technology.tables.vias);
    const auto substrate=reinterpret_cast<const SubstrateLayer*>(technology.tables.substrate);
    const GeometryTechnology layers{metals,vias,technology.metal_heights,technology.tables.metal_count};
    const PotentialCacheRuntime cache_runtime{{open,read,write,close,::sprintf,nullptr},technology.input.directory,technology.input.name,report.warnings,{cache_status,&s}};
    const PotentialCacheTechnology cache_technology{substrate,technology.tables.substrate_count,metals,technology.tables.metal_count};
    const PotentialContactOptions contacts{technology.input.fft_x,technology.input.fft_y,technology.tables.metal_count,technology.input.width,technology.input.height,technology.length_step,technology.width_step};
    const PotentialCapacitanceOptions cap{contacts,cache_technology,technology.depths,cache_runtime,false,false,matrix_dump,&s,{0}};
    InductanceRunOptions ind{{technology.half_height,3,metals},technology.thickness_cell,technology.tables.metal_count,vias,false,matrix_dump,&s};
    ind.reporting={false,false,timer,report.format,report.warnings,report.message,&s};
    const GreenIntegrationRuntime green{{sine,cosine},{console_quadpack_diagnostic,&s},::printf,nullptr,nullptr,nullptr};
    const EddyRunOptions eddy{0,technology.cached_eddy,technology.metal_heights,{1e-6,&technology.green,&s.green_workspace,green},real_dump,&s};
    const NodeEquationOptions nodes{false,report.format,report.warnings,node_dump,&s};
    s.combined_options={{technology.half_width,technology.length_step},cap,ind,eddy,nodes,report.warnings,false,true,matrix_dump,&s,{clock,difference,elapsed,pi_report,&s}};
    s.transformer_options={{technology.half_width,technology.length_step},layers,cap,ind,eddy,nodes,commands,report.warnings,true,transformer_report,&s};
    s.three_port_options={{technology.half_width,technology.length_step},layers,cap,ind,eddy,nodes,commands,true,three_report,&s};
    s.extended_options={s.combined_options,{0.,false,report.format,report.message,report.warnings,matrix_dump,&s,nullptr},geometry,timer,report.format,report.message,&s};
    s.capacitive_options={cap,metals,true,report.format,::printf,report.warnings};
    s.pad_options={layers,commands,s.combined_options,true,{0}};
    s.matrix_options={{technology.half_width,technology.length_step},ind,eddy,geometry,report};
    s.twoport={0,0,s.controls.limits,.05,.8,report,geometry,&s.combined_state,&s.combined_options,&s.extended_state,&s.extended_options,&s.capacitive_state,&s.capacitive_options,&s.analytical_history,&s.analytical_result,{metals,vias,substrate,technology.metal_heights,technology.tables.metal_count,0,4,0.,0.,0.},pi_report,&s};
    s.simple={{metals,vias,technology.tables.metal_count},&s.twoport,&s.admittance,&s.pi,&shapes,&s.matrix_options,characters,report,{nullptr,nullptr,&s.admittance,::printf,::fflush,stdout,report},{0.,0.},{}};
    s.analysis={0,{s.controls.limits,.05,.8,0},{metals,technology.tables.metal_count,report.format,report.message,report.warnings},&s.combined_state,&s.combined_options,&s.transformer_state,&s.transformer_options,geometry,{}};
    s.port_settings={{nullptr,0},1.,1.,1.,1,1,1,1,&s.controls.automatic_cells};replace_filename(s.port_settings.filename,0,0,"2port.data",10,nullptr);
    const PortSweepRuntime sweep{open,flush,close,header,two_columns,three_columns,frequency_update,analyze,two_record,three_record,report.warnings,&s};
    s.ports={{characters,report.format,report.message,report.warnings},replace_filename,run_port_frequency_sweep,sweep,&s};
    s.extended_analysis={0,{s.controls.limits,.05,.8,0},s.analysis.cell_runtime,&s.combined_state,&s.pad_options,&s.extended_state,&s.extended_options,&s.port_settings,&s.ports,{}};
    copy_capacitance(s);refresh_console_analysis(s);return true;
}
void refresh_console_analysis(ConsoleAnalysisState& s) {
    auto& c=s.controls;const auto limits=c.limits;auto& t=*s.technology;
    s.capacitance.cache.enabled=c.technology_io!=0;
    auto& combined=s.combined_options;combined.discretization={limits.max_length,limits.capacitance_max_length};
    combined.capacitance.contacts.length_step=limits.capacitance_max_length;combined.capacitance.contacts.width_step=limits.capacitance_max_width;
    combined.capacitance.contacts.width=t.input.width;combined.capacitance.contacts.height=t.input.height;
    combined.capacitance.progress=c.progress!=0;combined.capacitance.verbose=c.verbose!=0;
    combined.capacitance.runtime.status={cache_status,&s};
    combined.inductance.filaments.edge_cell_size=limits.max_width;combined.inductance.filaments.max_width_cells=c.maximum_width_cells;
    combined.inductance.thickness_cell=limits.max_thickness;combined.inductance.reporting.progress=c.progress!=0;combined.inductance.reporting.timing=c.timing!=0;
    combined.inductance.reporting.status=s.progress_output;
    combined.eddy.mode=c.eddy_mode;combined.eddy.layer_count=t.cached_eddy;combined.nodes.progress=c.progress!=0;combined.timing=c.timing!=0;
    s.transformer_options.discretization=combined.discretization;s.transformer_options.capacitance=combined.capacitance;s.transformer_options.inductance=combined.inductance;s.transformer_options.eddy=combined.eddy;s.transformer_options.nodes=combined.nodes;s.transformer_options.commands=s.commands;
    s.three_port_options.discretization=combined.discretization;s.three_port_options.capacitance=combined.capacitance;s.three_port_options.inductance=combined.inductance;s.three_port_options.eddy=combined.eddy;s.three_port_options.nodes=combined.nodes;s.three_port_options.commands=s.commands;
    s.extended_options.network=combined;s.extended_options.nodes.dump=c.dump_matrices!=0;s.capacitive_options.capacitance=combined.capacitance;
    s.pad_options.combined=combined;s.pad_options.commands=s.commands;s.matrix_options.discretization=combined.discretization;s.matrix_options.inductance=combined.inductance;s.matrix_options.eddy=combined.eddy;
    s.twoport.automatic_cells=c.automatic_cells;s.twoport.progress=c.progress;s.twoport.limits=limits;s.twoport.alpha=c.alpha;s.twoport.beta=c.beta;
    s.analysis.automatic_cells=c.automatic_cells;s.analysis.cells={limits,c.alpha,c.beta,c.progress};
    s.extended_analysis.automatic_cells=c.automatic_cells;s.extended_analysis.cells={limits,c.alpha,c.beta,c.progress};
    s.port_header={t.input.name,t.input.directory,t.tables,limits.max_length,limits.max_width,limits.max_thickness,limits.capacitance_max_length,limits.capacitance_max_width,c.alpha,c.beta,c.automatic_cells};s.port_settings.three_port_option=c.verbose;
}
bool execute_console_analysis(int id,CommandArguments& arguments,ConsoleAnalysisState& s,GeometryShape*& shapes,const CommandRuntime& parser) {
    switch(id) {
    case 500:case 501:case 502:case 503:case 504:case 505:case 515:case 517:case 518:case 523:case 524:
    case 527:case 528:case 529:case 530:case 531:case 533:case 534:case 535:case 536:case 538:case 539:break;
    default:return false;
    }
    s.shapes=&shapes;s.simple.shapes=&shapes;s.simple.characters=parser.characters;s.commands.selected=parser.selected;refresh_console_analysis(s);copy_capacitance(s);
    const bool simple=execute_simple_analysis_command(id,arguments,s.simple);bool handled=simple;
    if(!handled)handled=execute_analysis_command(id,arguments,s.analysis);
    if(!handled)handled=execute_extended_analysis_command(id,arguments,s.extended_analysis);
    if(!handled)handled=execute_port_command(id,arguments,s.port_settings,s.ports);
    if(!handled)return false;
    if(simple)s.controls.limits=s.twoport.limits;else if(id==515 || id==517 || id==523)s.controls.limits=s.analysis.cells.limits;
    else if(id==518 || id==538)s.controls.limits=s.extended_analysis.cells.limits;
    if(id==515 || id==517) { s.admittance=s.combined_state.ports;s.pi=s.combined_state.pi; }
    if(id==523)store_transformer(s);
    if(id==518) { s.admittance=s.combined_state.ports;s.pi=s.combined_state.pi; }
    if(id==538) { s.admittance=s.extended_state.network.ports;s.pi=s.extended_state.network.pi; }
    retain_capacitance(s);
    auto& t=*s.technology;const auto& limits=s.controls.limits;
    t.half_width=limits.max_length;t.half_height=limits.max_width;t.thickness_cell=limits.max_thickness;t.length_step=limits.capacitance_max_length;t.width_step=limits.capacitance_max_width;
    return true;
}
void release_console_analysis(ConsoleAnalysisState& s) {
    if(s.capacitance.groups)release_potential_contacts(s.capacitance.groups,s.geometry.memory);
    if(s.cache_allocated) { release_potential_cache(s.capacitance.cache,s.technology->tables.metal_count,s.geometry.memory);s.cache_allocated=false; }
    ::free(s.port_settings.filename.data);s.port_settings.filename={nullptr,0};
}
}
