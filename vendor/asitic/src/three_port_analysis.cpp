#include "asitic/three_port_analysis.hpp"
namespace asitic {
namespace {
bool equal_name(const GeometryShape& a,const GeometryShape& b) {
    const auto* x=a.filaments.prefix;const auto* y=b.filaments.prefix;
    while(*x && *x==*y) { ++x;++y; }return *x==*y;
}
char* copy_name(const GeometryShape& source,char* out) {
    const auto* p=reinterpret_cast<const char*>(source.filaments.prefix);
    char* result=out;while(*p)*out++=*p++;*out++='_';*out++='_';*out=0;return result;
}
unsigned name_size(const GeometryShape& source) {
    unsigned n=0;while(source.filaments.prefix[n])++n;return n+3;
}
const double& number(const GeometryShape& shape,unsigned offset) {
    return *reinterpret_cast<const double*>(shape.filaments.prefix+offset);
}
ComplexMatrix matrix(int n,SolverMemory memory,WarningSink warnings) {
    const unsigned elements=static_cast<unsigned>(n)*static_cast<unsigned>(n);
    auto* p=static_cast<ComplexValue*>(memory.arrays.get(elements*16u));
    for(int i=0;i<static_cast<int>(elements);++i)p[i]={0,0};
    if(!p)warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    return {p,static_cast<int>(elements),0,n,n,n,0};
}
void destroy(const ComplexMatrix& value,SolverMemory memory) { if(!value.vector_reference)memory.put(value.data); }
}
bool solve_three_port_network(const GeometryShape& first,const GeometryShape& second,
    const GeometryShape& third,GeometryShape& reference,double frequency,
    ThreePortAnalysisState& state,const ThreePortAnalysisOptions& o) {
    const auto& runtime=o.commands.construction.geometry;const auto memory=runtime.memory;
    auto* a=clone_geometry_shape(state.shapes,first,o.discretization,runtime);
    auto* b=clone_geometry_shape(state.shapes,second,o.discretization,runtime);
    auto* c=clone_geometry_shape(state.shapes,third,o.discretization,runtime);
    const int first_count=count_geometry_metals(*a,o.technology.metal_count);
    const int second_count=count_geometry_metals(*b,o.technology.metal_count);
    unsigned n=static_cast<unsigned>(first_count)+static_cast<unsigned>(second_count)+static_cast<unsigned>(count_geometry_metals(*c,o.technology.metal_count));
    if(!equal_name(first,reference))for(auto* p=const_cast<ConductorSegment*>(reference.filaments.first);p;p=reinterpret_cast<ConductorSegment*>(p->next_address)) {
        ++n;p->width_count=p->thickness_count=10;
    }
    auto* first_name=static_cast<char*>(__builtin_alloca(name_size(first)));
    auto* second_name=static_cast<char*>(__builtin_alloca(name_size(second)));
    auto* third_name=static_cast<char*>(__builtin_alloca(name_size(third)));
    auto* joined=copy_geometry_shape(state.shapes,*a,copy_name(first,first_name),number(*a,0x54),number(*a,0x5c),-1,o.technology,o.commands);
    copy_geometry_shape(state.shapes,*b,copy_name(second,second_name),number(*b,0x54),number(*b,0x5c),-1,o.technology,o.commands);
    auto* middle=state.shapes;
    copy_geometry_shape(state.shapes,*c,copy_name(third,third_name),number(*c,0x54),number(*c,0x5c),-1,o.technology,o.commands);
    reverse_geometry_segments(*state.shapes);
    join_geometry_shapes(state.shapes,*middle,*state.shapes,o.technology,o.commands);
    join_geometry_shapes(state.shapes,*joined,*middle,o.technology,o.commands);
    remove_geometry_shape(state.shapes,a,memory);remove_geometry_shape(state.shapes,b,memory);remove_geometry_shape(state.shapes,c,memory);
    const GeometryShape* ground=equal_name(first,reference)?joined:&reference;
    auto capacitance=matrix(static_cast<int>(n),memory,runtime.errors);
    auto cap_options=o.capacitance;cap_options.verbose=false;
    if(!solve_capacitance(joined->filaments,ground->filaments,capacitance,frequency,state.capacitance,cap_options,memory)) {
        remove_geometry_shape(state.shapes,joined,memory);destroy(capacitance,memory);return false;
    }
    auto inductance=matrix(static_cast<int>(n),memory,runtime.errors);
    auto ind_options=o.inductance;ind_options.impedance=true;
    if(!solve_inductance(joined->filaments,inductance,frequency,ind_options,o.eddy,memory,o.commands.construction.warnings)) {
        remove_geometry_shape(state.shapes,joined,memory);destroy(inductance,memory);destroy(capacitance,memory);return false;
    }
    remove_geometry_shape(state.shapes,joined,memory);
    if(!solve_three_port_equations(capacitance.data,inductance.data,first_count,second_count,static_cast<int>(n),state.impedance,memory,o.nodes)) {
        destroy(inductance,memory);destroy(capacitance,memory);return false;
    }
    state.model=extract_three_port_equivalent(state.impedance,frequency);
    if(o.print && o.report)o.report(state.model,frequency,o.report_user);
    destroy(inductance,memory);destroy(capacitance,memory);return true;
}
}
