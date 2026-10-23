#include "asitic/console_optimization.hpp"
extern "C" int printf(const char*,...);
extern "C" int fflush(void*);
extern "C" int system(const char*);

namespace asitic {
namespace {
void clear(void* data,unsigned size) { auto* p=static_cast<unsigned char*>(data);for(unsigned i=0;i<size;++i)p[i]=0; }
void copy(char* out,const char* in) { while((*out++=*in++)) {} }
void retain_square(ConsoleOptimizationState&,const SquareOptimizationState&,bool);
void publish(ConsoleOptimizationState& s) {
    auto& a=*s.analysis;auto& g=*s.geometry;
    if(s.active_shapes==&s.multimetal.grid.shapes) {
        s.settings=s.multimetal.grid.settings;a.controls.limits=s.multimetal.cells;
        // These are the same native mesh globals used by CELL/CCELL. The
        // session refresh reads them from technology before the next command.
        auto& t=*s.technology;const auto& limits=s.multimetal.cells;
        t.half_width=limits.max_length;t.half_height=limits.max_width;
        t.thickness_cell=limits.max_thickness;
        t.length_step=limits.capacitance_max_length;t.width_step=limits.capacitance_max_width;
    } else if(s.active_shapes==&s.square.shapes)retain_square(s,s.square,true);
    else if(s.active_shapes==&s.batch.search.shapes) { retain_square(s,s.batch.search,true);s.settings.sides=s.batch.sides; }
    else if(s.active_shapes==&s.symmetric_square.shapes)retain_square(s,s.symmetric_square,false);
    else if(s.active_shapes==&s.symmetric_polygon.shapes)retain_square(s,s.symmetric_polygon,false);
    else if(s.active_shapes==&s.polygon.shapes) { s.settings.area_increment=s.polygon.selected_width;s.settings.area_max=s.polygon.selected_turns; }
    else if(s.active_shapes==&s.area.shapes) { s.settings.area_min=s.area.selected_spacing;s.settings.area_increment=s.area.selected_width;s.settings.area_max=s.area.selected_turns; }
    if(s.active_shapes)g.head=*s.active_shapes;
    if(a.shapes)*a.shapes=g.head;
    s.parser->shapes=g.head;s.parser->selected=g.selected;
    g.prompt.command.shapes=g.head;g.prompt.command.selected=g.selected;g.commands.selected=g.selected;
    if(s.active_model) { a.analytical_result=*s.active_model;a.admittance=s.active_model->admittance;a.pi=s.active_model->pi; }
    if(s.active_capacitance)a.capacitance=*s.active_capacitance;
}
void read(const char* prompt,char* line,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);s.read(prompt,line,s.user);
}
void grid_read(const char* prompt,char* line,bool,void* user) { read(prompt,line,user); }
void message(const char* text,bool,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);s.analysis->report.warnings.emit(text);
}
void analytical_warning(const char* text,void* user) { message(text,true,user); }
void construction_warning(const char* text,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);s.geometry->commands.construction.warnings.emit(text);
}
void geometry_error(const char* text,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);s.geometry->geometry.errors.emit(text);
}
void terminate(int code,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);s.geometry->geometry.terminate(code,s.geometry->geometry.user);
}
void invalid(void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);s.invalid(s.user);
}
void* open(const char* path,const char* mode,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);const auto& r=s.analysis->report;return r.open(path,mode,r.user);
}
int close(void* file,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);const auto& r=s.analysis->report;return r.close(file,r.user);
}
int flush(void* file,void* user) { publish(*static_cast<ConsoleOptimizationState*>(user));return ::fflush(file); }
int run_system(const char* command,void* user) { publish(*static_cast<ConsoleOptimizationState*>(user));return ::system(command); }
void iterations(int count,void* user) { publish(*static_cast<ConsoleOptimizationState*>(user));::printf("\nCompleted in %ld iterations",count); }
void report(const PiEquivalent& pi,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);publish(s);auto& a=*s.analysis;
    a.pi=pi;a.analytical_result.pi=pi;
    if(a.twoport.analytical_report)a.twoport.analytical_report(pi,pi.frequency,a.twoport.analytical_report_user);
}
const SquareConstructionHistory* square_history(unsigned,void* user) { return &static_cast<ConsoleOptimizationState*>(user)->construction_history; }
const WireConstructionHistory* polygon_history(unsigned,void* user) { return &static_cast<ConsoleOptimizationState*>(user)->polygon_construction_history; }
const SymmetricConstructionHistory* symmetric_history(unsigned,void* user) { return &static_cast<ConsoleOptimizationState*>(user)->symmetric_construction_history; }
const AnalyticalModelHistory* analytical_history(unsigned,void* user) { return &static_cast<ConsoleOptimizationState*>(user)->analytical_history; }
SpiralConstructionRuntime construction(ConsoleOptimizationState& s) {
    auto out=s.geometry->commands.construction;out.warnings={construction_warning,&s};out.structure_violation=invalid;out.user=&s;
    out.geometry.errors={geometry_error,&s};out.geometry.terminate=terminate;out.geometry.user=&s;return out;
}
ShapePromptRuntime prompts(ConsoleOptimizationState& s) {
    auto out=s.geometry->prompt;out.command=*s.parser;out.command.shapes=s.geometry->head;out.command.selected=s.geometry->selected;
    out.invalid=invalid;out.user=&s;return out;
}
SquareOptimizationRuntime runtime(ConsoleOptimizationState& s) {
    return {nullptr,message,run_system,iterations,s.analysis->report.format,s.analysis->report.message,report,&s};
}
void prepare_square(ConsoleOptimizationState& s,SquareOptimizationState& out) {
    const auto& p=s.settings;auto& q=out.parameters;
    copy(q.name,p.shape_name);q.spacing={p.spacing_min,p.spacing_increment,p.spacing_max};q.width={p.width_min,p.width_increment,p.width_max};
    q.dimension_x=p.minimum_quality;q.dimension_y=p.absolute_tolerance;q.target_inductance_nh=p.inductance_min;q.accuracy_percent=p.inductance_max;q.frequency=p.frequency;
    q.chip_x=s.technology->input.width;q.chip_y=s.technology->input.height;q.metal=p.metal;q.underpass_metal=p.exit_metal;q.quality_metric=p.metric_kind;q.differential=p.metric_terminal;
    out.shapes=s.geometry->head;out.selected_spacing=p.area_min;out.selected_width=p.area_increment;out.selected_turns=p.area_max;out.model=s.analysis->analytical_result;
}
void retain_square(ConsoleOptimizationState& s,const SquareOptimizationState& in,bool parameters) {
    auto& p=s.settings;const auto& q=in.parameters;
    if(parameters) {
        copy(p.shape_name,q.name);p.spacing_min=q.spacing.minimum;p.spacing_increment=q.spacing.step;p.spacing_max=q.spacing.maximum;
        p.width_min=q.width.minimum;p.width_increment=q.width.step;p.width_max=q.width.maximum;p.minimum_quality=q.dimension_x;p.absolute_tolerance=q.dimension_y;
        p.inductance_min=q.target_inductance_nh;p.inductance_max=q.accuracy_percent;p.frequency=q.frequency;
        p.metal=q.metal;p.exit_metal=q.underpass_metal;p.metric_kind=q.quality_metric;p.metric_terminal=q.differential;
    }
    p.area_min=in.selected_spacing;p.area_increment=in.selected_width;p.area_max=in.selected_turns;
}
void prompt_square(SquareOptimizationState& square,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);
    const OptimizationPromptRuntime io{prompts(s),read,&s};prompt_optimization_target(s.settings,io,s.prompt_history);prepare_square(s,square);
}
void polygon(int skip,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);optimize_polygon_spiral(s.polygon,s.polygon_options,skip);
    s.settings.area_increment=s.polygon.selected_width;s.settings.area_max=s.polygon.selected_turns;
}
void symmetric_square(int skip,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);optimize_symmetric_square(s.symmetric_square,s.symmetric_square_options,skip);retain_square(s,s.symmetric_square,false);
}
void symmetric_polygon(int skip,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);optimize_symmetric_polygon(s.symmetric_polygon,s.symmetric_polygon_options,skip);retain_square(s,s.symmetric_polygon,false);
}
void area(void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);optimize_area(s.area,s.area_options,s.area_history);
    s.settings.area_min=s.area.selected_spacing;s.settings.area_increment=s.area.selected_width;s.settings.area_max=s.area.selected_turns;
}
void batch(void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);run_optimization_batch(s.batch,s.batch_options,s.batch_history);retain_square(s,s.batch.search,true);s.settings.sides=s.batch.sides;
}
void multimetal(int mode,void* user) {
    auto& s=*static_cast<ConsoleOptimizationState*>(user);run_multimetal_optimization_grid(s.multimetal,s.multimetal_options,mode);
    s.analysis->controls.limits=s.multimetal.cells;
}
}
void initialize_console_optimization(ConsoleOptimizationState& s,ConsoleGeometryState& geometry,ConsoleAnalysisState& analysis,
    TechnologyInitialization& technology,CommandRuntime& parser,void (*input)(const char*,char*,void*),void (*failure)(void*),void* user) {
    clear(&s,sizeof(s));s.geometry=&geometry;s.analysis=&analysis;s.technology=&technology;s.parser=&parser;s.read=input;s.invalid=failure;s.user=user;
    s.construction_history=geometry.square_history;s.polygon_construction_history=geometry.square_history.mesh;s.symmetric_construction_history=geometry.symmetric_history;
    s.analytical_history=analysis.analytical_history;
    s.square_options.history.arguments=geometry.arguments[0];s.polygon_options.history.arguments=geometry.arguments[0];
    s.symmetric_square_options.history.arguments=geometry.arguments[0];s.symmetric_polygon_options.history.arguments=geometry.arguments[0];s.area_history.arguments=geometry.arguments[0];
    s.grid_options.history.arguments=geometry.arguments[0];
    for(unsigned i=0;i<68;++i)s.batch_retained_history[i]=s.analytical_history;
    refresh_console_optimization(s);
}
void refresh_console_optimization(ConsoleOptimizationState& s) {
    auto& g=*s.geometry;auto& a=*s.analysis;auto& t=*s.technology;
    const auto build=construction(s);const auto prompt=prompts(s);const auto warnings=WarningSink{analytical_warning,&s};
    auto square_incoming=s.square_options.history;
    square_incoming.construction_at=square_history;square_incoming.analytical_at=analytical_history;square_incoming.user=&s;
    s.square_options={g.technology,build,a.twoport.analytical_options,warnings,square_incoming,runtime(s)};s.square_options.runtime.prompt=prompt_square;
    auto polygon_incoming=s.polygon_options.history;polygon_incoming.construction_at=polygon_history;polygon_incoming.analytical_at=analytical_history;polygon_incoming.user=&s;
    s.polygon_options={g.technology,build,a.twoport.analytical_options,warnings,polygon_incoming,{prompt,read,message,run_system,a.report.format,a.report.message,report,&s}};
    auto symmetric_square_incoming=s.symmetric_square_options.history;symmetric_square_incoming.construction_at=symmetric_history;symmetric_square_incoming.analytical_at=analytical_history;symmetric_square_incoming.user=&s;
    auto symmetric_polygon_incoming=s.symmetric_polygon_options.history;symmetric_polygon_incoming.construction_at=symmetric_history;symmetric_polygon_incoming.analytical_at=analytical_history;symmetric_polygon_incoming.user=&s;
    s.symmetric_square_options={g.technology,build,a.twoport.analytical_options,warnings,symmetric_square_incoming,runtime(s),&g.prompt,read,&s};
    s.symmetric_polygon_options={g.technology,build,a.twoport.analytical_options,warnings,symmetric_polygon_incoming,runtime(s),&g.prompt,read,&s};
    s.area_options={g.technology,build,a.twoport.analytical_options,warnings,{prompt,read,message,a.report.format,report,&s}};
    s.area_history.construction_at=square_history;s.area_history.analytical_at=analytical_history;s.area_history.user=&s;
    for(unsigned i=0;i<68;++i)s.batch_search_options[i]=s.square_options;
    s.batch_options={s.batch_search_options,s.batch_retained_history,{read,open,a.report.file_format,flush,close,&s}};
    auto grid_incoming=s.grid_options.history;grid_incoming.construction_at=square_history;grid_incoming.analytical_at=analytical_history;grid_incoming.user=&s;
    s.grid_options={prompt,build,a.twoport.analytical_options,a.combined_options,a.port_header,{grid_read,open,a.report.file_format,flush,close,message,a.report.format,&s},grid_incoming};
    s.multimetal_options={s.grid_options,g.commands,a.report,a.controls.progress!=0};
    s.multimetal_options.commands.construction=build;s.multimetal_options.frequency_runtime.warnings=warnings;
    s.commands={&s.square,&s.square_options,&s.multimetal.grid,&s.grid_options,&s.keyword_history,{}};
    s.commands.backends.polygon=polygon;s.commands.backends.symmetric_square=symmetric_square;s.commands.backends.symmetric_polygon=symmetric_polygon;
    s.commands.backends.area=area;s.commands.backends.batch=batch;s.commands.backends.multimetal_grid=multimetal;s.commands.backends.user=&s;
    s.square.parameters.chip_x=s.symmetric_square.parameters.chip_x=s.symmetric_polygon.parameters.chip_x=t.input.width;
    s.square.parameters.chip_y=s.symmetric_square.parameters.chip_y=s.symmetric_polygon.parameters.chip_y=t.input.height;
    s.polygon.parameters.chip_x=t.input.width;s.polygon.parameters.chip_y=t.input.height;
}
bool execute_console_optimization(int id,CommandArguments& arguments,ConsoleOptimizationState& s,bool* suppressed) {
    if(id!=700 && id!=706 && id!=707 && id!=708 && id!=711 && id!=713 && id!=714 && id!=715)return false;
    refresh_console_optimization(s);s.active_capacitance=nullptr;
    if(id==700 || id==707) {
        auto& square=id==700?s.square:s.batch.search;prepare_square(s,square);s.active_shapes=&square.shapes;s.active_model=&square.model;
        if(id==707)s.batch.sides=s.settings.sides;
    } else if(id==713 || id==714) {
        auto& square=id==713?s.symmetric_square:s.symmetric_polygon;prepare_square(s,square);s.active_shapes=&square.shapes;s.active_model=&square.model;
    } else if(id==708) {
        s.polygon.shapes=s.geometry->head;s.polygon.model=s.analysis->analytical_result;s.polygon.selected_width=s.settings.area_increment;s.polygon.selected_turns=s.settings.area_max;
        s.active_shapes=&s.polygon.shapes;s.active_model=&s.polygon.model;
    } else if(id==706) {
        s.area.shapes=s.geometry->head;s.area.model=s.analysis->analytical_result;s.area.selected_spacing=s.settings.area_min;s.area.selected_width=s.settings.area_increment;s.area.selected_turns=s.settings.area_max;
        s.active_shapes=&s.area.shapes;s.active_model=&s.area.model;
    } else {
        s.multimetal.grid.settings=s.settings;s.multimetal.grid.shapes=s.geometry->head;s.multimetal.grid.model=s.analysis->analytical_result;s.multimetal.grid.capacitance=s.analysis->capacitance;s.multimetal.cells=s.analysis->controls.limits;
        s.active_shapes=&s.multimetal.grid.shapes;s.active_model=&s.multimetal.grid.model;s.active_capacitance=&s.multimetal.grid.capacitance;
    }
    const bool handled=execute_optimization_command(id,arguments,s.commands,suppressed);
    if(handled) {
        if(id==700)retain_square(s,s.square,true);
        if(id==711 || id==715)s.settings=s.multimetal.grid.settings;
        publish(s);refresh_console_analysis(*s.analysis);
    }
    s.active_shapes=nullptr;s.active_model=nullptr;s.active_capacitance=nullptr;return handled;
}
}
