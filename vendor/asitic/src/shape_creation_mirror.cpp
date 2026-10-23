#include "asitic/shape_creation_commands.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double result;asm volatile("fldl %1; fstpt %0":"=m"(result):"m"(value):"st");return result; }
double& field(GeometryShape& shape,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&shape)+offset); }
volatile unsigned& word(GeometryShape& shape,unsigned offset) { return *reinterpret_cast<volatile unsigned*>(reinterpret_cast<unsigned char*>(&shape)+offset); }
void store(GeometryShape& shape,unsigned offset,long double value) { field(shape,offset)=x87::binary64(value); }
void bounded_name(GeometryShape& shape,const char* name) {
    auto* target=reinterpret_cast<char*>(&shape);unsigned i=0;for(;i<80 && name[i];++i)target[i]=name[i];for(;i<80;++i)target[i]=0;
}
}
void prompt_3d_transformer_creation(GeometryShape& primary,GeometryShape& secondary,const ShapeCreationRuntime& runtime,ShapeCreationHistory& history) {
    auto& values=history.mirror_values;auto& names=history.mirror_names;double orientation=0.;
    char* line=runtime.shape.command.message;const auto& command=runtime.shape.command;
    names[0][0]=0;
    do {
        normalize_shape_creation_prompt("Primary Name? ",line,runtime);
        if(find_geometry_shape(command.shapes,command.selected,line)) { command.warnings.emit("Another existing spiral has the same name.");names[0][0]=0; }
        else command.scan(line,"%s",names[0]);
    }while(!names[0][0]);
    do {
        normalize_shape_creation_prompt("Secondary Name? ",line,runtime);
        if(find_geometry_shape(command.shapes,command.selected,line)) { command.warnings.emit("Another existing spiral has the same name.");names[1][0]=0; }
        else command.scan(line,"%s",names[1]);
    }while(!names[1][0]);
    do { read_shape_creation_line("L1, L2 = ? ",line,runtime); }while(command.scan(line,"%lf %lf",values,values+1)<2);
    do { read_shape_creation_line("Width? ",line,runtime);command.scan(line,"%lf",values+2); }while(load(values[2])<load(0x1.b7cdfd9d7bdbbp-34));
    do {
        read_shape_creation_line("Spacing (metal edge to metal edge)? ",line,runtime);command.scan(line,"%lf",values+3);
        const long double projected=(load(values[3])-load(0x1.a8240b780346ep-2)*load(values[2]))/load(0x1.6a0902de00d1bp+0);
        if(projected<2.L) { command.warnings.emit("Spacing Too Small.");values[3]=-1.; }
    }while(load(values[3])<load(0x1.b7cdfd9d7bdbbp-34));
    do { read_shape_creation_line("Turns (fractions okay)? ",line,runtime);command.scan(line,"%lf",values+4); }while(load(values[4])<1.L);
    int first_metal,second_metal;
    do { read_shape_creation_line("Primary metal layer? ",line,runtime);first_metal=resolve_shape_creation_metal(line,runtime,history); }while(first_metal<0 || first_metal>=runtime.shape.technology.metal_count);
    do { read_shape_creation_line("Secondary metal layer? ",line,runtime);second_metal=resolve_shape_creation_metal(line,runtime,history); }while(second_metal<0 || second_metal>=runtime.shape.technology.metal_count || second_metal==first_metal);
    do { read_shape_creation_line("Origin (x,y)? ",line,runtime); }while(command.scan(line,"%lf %lf",values+5,values+6)<2);
    do { read_shape_creation_line("Orientation (degrees)? ",line,runtime);command.scan(line,"%lf",&orientation); }while(load(orientation)< -360.L || load(orientation)>360.L);
    do { read_shape_creation_line("Electrical phase (+1 or -1)? ",line,runtime);command.scan(line,"%d",&history.mirror_phase); }while(history.mirror_phase!= -1 && history.mirror_phase!=1);
    normalize_input_line(names[0],0x8e,command.characters);bounded_name(primary,names[0]);word(primary,0x64)=4;
    const long double first_length=load(values[0]);store(primary,0x70,first_length);const long double first_radius=first_length*load(0.5);
    word(primary,0x68)=first_metal;word(primary,0x6c)=0;word(primary,0x50)=10;store(primary,0x80,first_radius);
    store(primary,0x78,load(values[1]));store(primary,0x90,load(values[3]));store(primary,0x88,load(values[2]));store(primary,0x98,load(values[4]));store(primary,0x54,load(values[5]));store(primary,0x5c,load(values[6]));store(primary,0xa0,load(orientation));word(primary,0xac)=history.mirror_phase;
    normalize_input_line(names[1],0x8e,command.characters);bounded_name(secondary,names[1]);word(secondary,0x64)=4;
    const long double second_length=load(values[0]);store(secondary,0x70,second_length);const long double second_radius=second_length*load(0.5);
    word(secondary,0x68)=second_metal;word(secondary,0x6c)=0;word(secondary,0x50)=10;store(secondary,0x80,second_radius);
    store(secondary,0x78,load(values[1]));store(secondary,0x90,load(values[3]));store(secondary,0x88,load(values[2]));store(secondary,0x98,load(values[4]));store(secondary,0x54,load(values[5]));store(secondary,0x5c,load(values[6]));store(secondary,0xa0,load(orientation));word(secondary,0xac)=history.mirror_phase;
}
}
