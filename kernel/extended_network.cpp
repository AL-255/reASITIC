#include "extended_network.hpp"
#include "pi_shunts.hpp"
namespace reasitic {
void solve_portable_extended_nodes(const asitic::ComplexMatrix& cap,const asitic::ComplexMatrix& impedance,
    asitic::TwoPortAdmittance& ports,asitic::SolverMemory memory,const asitic::ExtendedNodeOptions& options){
    const int n=cap.rows;
    const auto fail=[&](){const double invalid=__builtin_nan("");ports.y11=ports.y12=ports.y21=ports.y22={invalid,invalid};options.warnings.emit("Reduced PIX node equations could not produce finite states.");};
    if(n<1||n>1024||cap.columns!=n||impedance.rows!=n||impedance.columns!=n){fail();return;}
    auto* states=static_cast<asitic::ComplexValue*>(memory.arrays.get(2u*(n+1)*sizeof(asitic::ComplexValue)));
    if(!states){fail();return;}
    auto* voltages=states+n+1;double values[8];
    if(!extended_port_network(reinterpret_cast<const double*>(cap.data),reinterpret_cast<const double*>(impedance.data),n,cap.stride,impedance.stride,values,reinterpret_cast<double*>(states),reinterpret_cast<double*>(voltages))){memory.put(states);fail();return;}
    ports={{values[0],values[1]},{values[4],values[5]},{values[2],values[3]},{values[6],values[7]}};
    if(options.dump&&options.snapshot){
        char name[128];const double ghz=options.frequency*0x1.12e0be826d695p-30;
        const asitic::ComplexMatrix currents{states,n+1,1,n+1,1,n+1,1},voltage{voltages,n+1,1,n+1,1,n+1,1};
        options.format(name,"current_%5.3lf.out",ghz);options.snapshot(name,currents,options.user);
        options.format(name,"voltages_%5.3lf.out",ghz);options.snapshot(name,voltage,options.user);
    }
    memory.put(states);
}
}
