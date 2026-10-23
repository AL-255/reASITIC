#include "asitic/optimization_grid_multimetal.hpp"
#include "asitic/optimization_radius.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1; fstpt %0":"=m"(out):"m"(value):"st");return out; }
double spill(const double& value) { return x87::binary64(load(value)); }
void word(GeometryShape& shape,unsigned offset,int value) { __builtin_memcpy(shape.filaments.prefix+offset,&value,4); }
void number(GeometryShape& shape,unsigned offset,long double value) { *reinterpret_cast<double*>(shape.filaments.prefix+offset)=x87::binary64(value); }
void message(const OptimizationGridOptions& options,const char* text) { options.runtime.message(text,true,options.runtime.user); }
void read(const OptimizationGridOptions& options,const char* prompt,char* line,bool normalize=false) {
    options.runtime.read(prompt,line,false,options.runtime.user);
    if(normalize)normalize_input_line(line,0xae,options.prompts.command.characters);
}
void prompted_parameters(OptimizationSweepState& p,GeometryShape& args,char* line,int& metal_scan,const OptimizationGridOptions& o) {
    const auto scan=o.prompts.command.scan;
    for(;;) {
        read(o,"Spiral Name? ",line,true);
        if(find_geometry_shape(o.prompts.command.shapes,o.prompts.command.selected,line)) {
            message(o,"Another existing spiral has the same name.");*line=0;
        } else scan(line,"%s",args.filaments.prefix);
        if(*line)break;
    }
    message(o,"Enter electrical constraints:");
    do { read(o,"Frequency? (GHz) ",line);scan(line,"%lf",&p.frequency); }while(p.frequency<=1e-10);
    p.frequency=x87::binary64(load(p.frequency)*1e9L);
    int count;
    do {
        read(o,"Desired range of inductance(nH) (e.g. 2 2.5):",line);count=scan(line,"%lf %lf",&p.inductance_min,&p.inductance_max);
    }while(p.inductance_min<1e-10 || p.inductance_max<1e-10 || count!=2 || p.inductance_max<p.inductance_min);
    do { read(o,"Minimum desired Q (e.g. 5):",line);count=scan(line,"%lf",&p.minimum_quality); }while(p.minimum_quality<1e-10 || count!=1);
    message(o,"Enter geometric constraints:");
    do { read(o,"Min/Inc/Max Length or Radius (e.g. 50 10 100)? ",line);count=scan(line,"%lf %lf %lf",&p.area_min,&p.area_increment,&p.area_max); }while(count!=3 || p.area_increment<1e-10 || p.area_max<p.area_min);
    do { read(o,"Min/Inc/Max Spacing? (e.g. .5 .5 5)",line);count=scan(line,"%lf %lf %lf",&p.spacing_min,&p.spacing_increment,&p.spacing_max); }while(count!=3 || p.spacing_increment<1e-10 || p.spacing_max<p.spacing_min);
    do { read(o,"Min/Step/Max Width? ",line);count=scan(line,"%lf %lf %lf",&p.width_min,&p.width_increment,&p.width_max); }while(count!=3 || p.width_increment<1e-10 || p.width_max<p.width_min);
    do { read(o,"Top metal layer? ",line);p.metal=find_shape_prompt_metal(line,o.prompts,metal_scan); }while(p.metal<0 || p.metal>=o.prompts.technology.metal_count);
    do { read(o,"Bottom metal layer? ",line);p.exit_metal=find_shape_prompt_metal(line,o.prompts,metal_scan); }while(p.exit_metal==p.metal || p.exit_metal<0 || p.exit_metal>=o.prompts.technology.metal_count);
    do { read(o,"Number of sides (e.g. 4 for square, or > 4 for polygon spirals)? ",line);scan(line,"%d",&p.sides); }while(p.sides<3);

}
OptimizationSweepState batch_parameters(const OptimizationSweepState& state) {
    auto p=state;
    p.frequency=spill(state.frequency);p.inductance_min=spill(state.inductance_min);p.inductance_max=spill(state.inductance_max);p.minimum_quality=spill(state.minimum_quality);
    p.area_min=spill(state.area_min);p.area_increment=spill(state.area_increment);p.area_max=spill(state.area_max);
    p.spacing_min=spill(state.spacing_min);p.spacing_increment=spill(state.spacing_increment);p.spacing_max=spill(state.spacing_max);
    p.width_min=spill(state.width_min);p.width_increment=spill(state.width_increment);p.width_max=spill(state.width_max);
    return p;
}
}
void run_multimetal_optimization_grid(MultimetalOptimizationGridState& multimetal,const MultimetalOptimizationGridOptions& options,int batch_mode) {
    auto& state=multimetal.grid;auto o=options.grid;
    auto args=o.history.arguments;char line[300],filename[80];__builtin_memcpy(line,o.history.line,sizeof(line));__builtin_memcpy(filename,o.history.filename,sizeof(filename));
    void* file;
    auto p=o.history.prompted;
    if(!batch_mode) {
        read(o,"Sweep filename? ",filename);file=o.runtime.open(filename,"w",o.runtime.user);
        if(!file) { message(o,"Error 1701:  Unable to open file.");return; }
        int metal_scan=o.history.metal_scan;prompted_parameters(p,args,line,metal_scan,o);
    } else {
        file=o.runtime.open(state.settings.output_filename,"w",o.runtime.user);
        if(!file) { message(o,"Error 1702:  Unable to open file.");return; }
        do {
            if(find_geometry_shape(o.prompts.command.shapes,o.prompts.command.selected,state.settings.shape_name)) {
                message(o,"Another existing spiral has the same name.");*line=0;
            } else o.prompts.command.scan(line,"%s",state.settings.shape_name);
        }while(!*line);
        p=batch_parameters(state.settings);
    }
    const int kind=p.sides==4?18:20;
    word(args,0x50,kind);word(args,0x64,p.sides);word(args,0x68,p.metal);word(args,0x6c,p.exit_metal);
    number(args,0xa0,0.L);args.attributes=0;
    const auto print=o.runtime.print;
    print(file,"\n!# Performing Inductor Sweep With Following Parameters:");
    print(file,"\n!# Geometry:");
    if(kind==18)print(file,"\n!#\tMulti-Metal Square spiral on metal %s - %s",o.header.tables.metals[p.metal].name,o.header.tables.metals[p.exit_metal].name);
    else print(file,"\n!#\tSpiral with %d sides on metal %d and exit_metal = %d",p.sides,p.metal,p.exit_metal);
    print(file,"\n!#\tLmin = %lf Linc = %lf Lmax = %lf",p.area_min,p.area_increment,p.area_max);
    print(file,"\n!#\tWmin = %lf Winc = %lf Wmax = %lf",p.width_min,p.width_increment,p.width_max);
    print(file,"\n!#\tSmin = %lf Sinc = %lf Smax = %lf",p.spacing_min,p.spacing_increment,p.spacing_max);
    print(file,"\n!# Electrical Constraints:");
    print(file,"\n!#\tfreq = %lf GHz, Lmin = %lf nH, Lmax = %lf nH, Qmin = %lf",x87::binary64(load(p.frequency)/1e9L),p.inductance_min,p.inductance_max,p.minimum_quality);
    print(file,"\n!#Analysis:");
    if(p.simulation_type==1)print(file,"\n!#\tUsing Pi Analysis");
    else {
        print(file,"\n!#\tUsing Pi2 Analysis");
        if(o.header.automatic_cells) {
            update_frequency_geometry(p.frequency,o.header.tables.metals,o.header.tables.metal_count,o.header.alpha,o.header.beta,multimetal.cells,options.progress,options.frequency_runtime);
            o.header.inductance_length=multimetal.cells.max_length;o.header.inductance_width=multimetal.cells.max_width;o.header.inductance_thickness=multimetal.cells.max_thickness;
            o.header.capacitance_length=multimetal.cells.capacitance_max_length;o.header.capacitance_width=multimetal.cells.capacitance_max_width;
            o.combined.discretization={multimetal.cells.max_length,multimetal.cells.capacitance_max_length};
            o.combined.inductance.filaments.edge_cell_size=multimetal.cells.max_width;o.combined.inductance.thickness_cell=multimetal.cells.max_thickness;
            o.combined.capacitance.contacts.length_step=multimetal.cells.capacitance_max_length;o.combined.capacitance.contacts.width_step=multimetal.cells.capacitance_max_width;
            print(file,"\n!#\tUsing Auto Cell Generation:  alpha = %lf, beta = %lf",o.header.alpha,o.header.beta);
        } else print(file,"\n!#\tUsing Following Cell Size Data:  maxL = %lf, maxW = %lf, maxT = %lf, CmaxL = %lf, CmaxW = %lf",o.header.inductance_length,o.header.inductance_width,o.header.inductance_thickness,o.header.capacitance_length,o.header.capacitance_width);
    }
    const PortOutputRuntime output{print,o.construction.geometry.memory,{},o.construction.warnings};write_port_technology_summary(file,o.header,output);
    print(file,"\n%6s%6s%6s%6s%6s%6s%8s%8s%8s%8s%6s%6s%6s","Len","W","S","N","L","R","C1","C2","R1","R2","Q1","Q2","Qdiff");
    unsigned calls=0;int iterations=0;const double turn_step=x87::binary64(1.L/p.sides);
    for(long double next_length=load(p.area_min);next_length<=p.area_max;) {
        const double length=x87::binary64(next_length);
        number(args,0x70,length);number(args,0x78,length);number(args,0x80,length);
        number(args,0x54,load(o.prompts.chip_x)*0.5L-(kind==18?load(length)*0.5L:0.L));
        number(args,0x5c,load(o.prompts.chip_y)*0.5L-(kind==18?load(length)*0.5L:0.L));
        for(long double next_spacing=load(p.spacing_min);next_spacing<=p.spacing_max;) {
            const double spacing=x87::binary64(next_spacing);number(args,0x90,spacing);o.runtime.flush(file,o.runtime.user);
            for(long double width=load(p.width_min);width<=p.width_max && !(width+width+spacing>length);width+=p.width_increment) {
                number(args,0x88,width);
                const double limit=x87::binary64(maximum_spiral_turns(length,x87::binary64(width),spacing,p.sides,kind));
                for(long double turns=1.L;turns<=limit;turns+=turn_step) {
                    ++iterations;
                    if(iterations%100==0) {
                        o.runtime.format(line,"%d:  L = %lf, W = %lf, S = %lf",iterations,length,x87::binary64(width),spacing);message(o,line);
                    }
                    number(args,0x98,turns);
                    const auto& construction=o.history.construction_at?*o.history.construction_at(calls,o.history.user):o.history.construction[calls];
                    if(kind==18)build_multimetal_square(state.shapes,args,o.prompts.technology,options.commands,construction);
                    else build_polygon_spiral(state.shapes,args,o.prompts.technology,o.construction,construction.mesh);
                    CombinedNetworkState network{state.shapes,state.capacitance,state.model.admittance,state.model.pi};auto network_options=o.combined;network_options.print=false;
                    solve_combined_network(*state.shapes,*state.shapes,p.frequency,network,network_options,o.construction.geometry);
                    state.shapes=network.shapes;state.capacitance=network.capacitance;state.model.admittance=network.ports;state.model.pi=network.pi;
                    ++calls;remove_geometry_shape(state.shapes,state.shapes,o.construction.geometry.memory);
                    const auto& pi=state.model.pi;
                    if(load(pi.inductance)/0x1.12e0be826d695p-30L>=p.inductance_min && load(pi.inductance)/0x1.0c6f7a0b5ed8dp-20L<=p.inductance_max && load(pi.quality_first)>=p.minimum_quality)
                        print(file,"\n%6.2lf%6.2lf%6.2lf%6.2lf%6.2lf%6.2lf%8.2lf%8.2lf%8.2lf%8.2lf%6.2lf%6.2lf%6.2lf",length,x87::binary64(width),spacing,x87::binary64(turns),x87::binary64(load(pi.inductance)*1e9L),pi.resistance,x87::binary64(load(pi.capacitance_second)*1e15L),x87::binary64(load(pi.capacitance_first)*1e15L),pi.resistance_second,pi.resistance_first,pi.quality_first,pi.quality_second,pi.quality_differential);
                }
            }
            next_spacing=load(spacing)+p.spacing_increment;
        }
        next_length=load(length)+p.area_increment;
    }
    o.runtime.close(file,o.runtime.user);o.runtime.format(line,"Finished after simulating %d devices.",iterations);message(o,line);
}
}
