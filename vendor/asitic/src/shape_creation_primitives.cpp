#include "asitic/shape_creation_commands.hpp"
namespace asitic {
namespace {
double& primitive_double(GeometryShape& shape,unsigned offset) { return *reinterpret_cast<double*>(reinterpret_cast<unsigned char*>(&shape)+offset); }
int& primitive_word(GeometryShape& shape,unsigned offset) { return *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(&shape)+offset); }
long double primitive_load(const double& value) { long double result;asm volatile("fldl %1; fstpt %0":"=m"(result):"m"(value):"st");return result; }
double primitive_difference(const double& upper,const double& lower) { double result;asm volatile("fldl %1; fsubl %2; fstpl %0":"=m"(result):"m"(upper),"m"(lower):"st");return result; }
void primitive_copy(char* destination,const char* source) { do { *destination++=*source; }while(*source++); }
void primitive_name(GeometryShape& shape,const ShapeCreationRuntime& r) {
    char* line=r.shape.command.message;
    do {
        normalize_shape_creation_prompt("Name? ",line,r);
        if(find_geometry_shape(r.shape.command.shapes,r.shape.command.selected,line)) {
            r.shape.command.warnings.emit("Another existing spiral has the same name.");*line=0;
        } else primitive_copy(reinterpret_cast<char*>(&shape),line);
    }while(!*line);
    normalize_input_line(reinterpret_cast<char*>(&shape),0x8e,r.shape.command.characters);
}
void primitive_dimension(double& value,const char* prompt,const char* error,const ShapeCreationRuntime& r) {
    char* line=r.shape.command.message;
    for(;;) {
        read_shape_creation_line(prompt,line,r);r.shape.command.scan(line,"%lf",&value);
        long double length=primitive_load(value);
        if(primitive_load(r.shape.chip_x)<length || primitive_load(r.shape.chip_y)<length) {
            r.shape.command.warnings.emit(error);length=primitive_load(value);
        }
        if(!(length<primitive_load(1e-10) || primitive_load(r.shape.chip_x)<length || primitive_load(r.shape.chip_y)<length))return;
    }
}
void primitive_origin(GeometryShape& shape,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    double& x=primitive_double(shape,0x54);double& y=primitive_double(shape,0x5c);
    for(;;) {
        read_shape_creation_line("Origin of spiral center (x y)? ",h.origin,r);
        for(char* p=h.origin;*p;++p)if(*p=='(' || *p==')' || *p==',')*p=' ';
        const int count=r.shape.command.scan(h.origin,"%lf %lf",&x,&y);
        if(primitive_load(x)<0.L || primitive_load(y)<0.L || primitive_load(r.shape.chip_x)<primitive_load(x) || primitive_load(r.shape.chip_y)<primitive_load(y))r.shape.command.warnings.emit("Error:  Origin falls outside of chip boundary.");
        if(count>=1 && !(primitive_load(x)<0.L || primitive_load(y)<0.L || primitive_load(r.shape.chip_x)<primitive_load(x) || primitive_load(r.shape.chip_y)<primitive_load(y)))return;
    }
}
void primitive_orientation(GeometryShape& shape,const ShapeCreationRuntime& r) {
    double& angle=primitive_double(shape,0xa0);char* line=r.shape.command.message;
    do {
        read_shape_creation_line("Spiral orientation in degrees? ",line,r);r.shape.command.scan(line,"%lf",&angle);
    }while(primitive_load(angle)< -360.L || 360.L<primitive_load(angle));
}
void primitive_metal(int& result,const char* prompt,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    do { read_shape_creation_line(prompt,r.shape.command.message,r);result=resolve_shape_creation_metal(r.shape.command.message,r,h); }while(result<0 || result>=r.shape.technology.metal_count);
}
void primitive_finish(GeometryShape& shape,int type) {
    primitive_word(shape,0x64)=1;
    const long double length=primitive_load(primitive_double(shape,0x70));primitive_double(shape,0x78)=length;
    primitive_word(shape,0x50)=type;primitive_double(shape,0x80)=length*0.5L;
    primitive_double(shape,0x90)=0.;primitive_double(shape,0x98)=type==2?0.25:0.;
}
}
void prompt_wire_creation(GeometryShape& shape,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    primitive_name(shape,r);primitive_double(shape,0x70)=-1.;primitive_double(shape,0x88)=-1.;
    primitive_dimension(primitive_double(shape,0x70),"Length? ","Error:  Length can't exceed chip size.",r);
    primitive_dimension(primitive_double(shape,0x88),"Width? ","Error:  Width can't exceed chip size.",r);
    primitive_metal(primitive_word(shape,0x68),"Metal layer? ",r,h);
    int& phase=primitive_word(shape,0xac);
    do { read_shape_creation_line("Spiral Electrical phase (+1 or -1)? ",r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%d",&phase); }while(phase!=-1 && phase!=1);
    primitive_origin(shape,r,h);primitive_orientation(shape,r);primitive_finish(shape,2);
}
void prompt_capacitor_creation(GeometryShape& shape,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    primitive_name(shape,r);
    primitive_dimension(primitive_double(shape,0x70),"Length? ","Error:  Length can't exceed chip size.",r);
    primitive_dimension(primitive_double(shape,0x88),"Width? ","Error:  Width can't exceed chip size.",r);
    primitive_metal(primitive_word(shape,0x68),"Top plate metal layer? ",r,h);
    primitive_metal(primitive_word(shape,0x6c),"Bottom plate metal layer? ",r,h);
    primitive_origin(shape,r,h);primitive_orientation(shape,r);primitive_finish(shape,4);
}
void prompt_via_creation(GeometryShape& shape,const ShapeCreationRuntime& r,ShapeCreationHistory& h) {
    primitive_name(shape,r);int via;
    for(;;) {
        read_shape_creation_line("Via Layer Name or Number? ",r.shape.command.message,r);via=resolve_shape_creation_via(r.shape.command.message,r,h);
        if(via!=-1)break;
        r.shape.command.warnings.emit("Error:  Unknown via layer.");
    }
    const auto* record=reinterpret_cast<const unsigned char*>(r.shape.technology.vias+via);
    const int first=*reinterpret_cast<const int*>(record+0xcc),second=*reinterpret_cast<const int*>(record+0xd0);
    const double height=primitive_difference(r.shape.technology.metal_heights[second],r.shape.technology.metal_heights[first]);
    const double width=primitive_load(*reinterpret_cast<const double*>(record+0xa0));
    int nx=-1,ny=-1,direction=h.phase;
    do { read_shape_creation_line("How many vias in parallel in x-dir? ",r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%d",&nx); }while(nx<1);
    do { read_shape_creation_line("How many vias in parallel in y-dir? ",r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%d",&ny); }while(ny<1);
    do { read_shape_creation_line("Via Current Flowing Down (-1) or Up (+1)?",r.shape.command.message,r);r.shape.command.scan(r.shape.command.message,"%d",&direction); }while(direction!=-1 && direction!=1);
    h.phase=direction;primitive_origin(shape,r,h);
    primitive_word(shape,0x64)=nx;primitive_double(shape,0x70)=primitive_load(height);primitive_double(shape,0x78)=primitive_load(width);
    primitive_double(shape,0x80)=0.5L*primitive_load(height);primitive_double(shape,0x90)=0.;primitive_double(shape,0x88)=primitive_load(width);
    primitive_double(shape,0x98)=0.25;primitive_word(shape,0x68)=via+r.shape.technology.metal_count;primitive_word(shape,0x6c)=ny;
    primitive_word(shape,0xac)=direction;primitive_double(shape,0xa0)=0.;primitive_word(shape,0x50)=14;
}
}
