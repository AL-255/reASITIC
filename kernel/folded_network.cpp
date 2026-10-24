#include "folded_network.hpp"
#include "pi_shunts.hpp"
namespace reasitic {
bool solve_portable_folded_nodes(asitic::ComplexValue* cap,const asitic::ComplexValue* ind,int n,
    asitic::TwoPortAdmittance& ports,const asitic::NodeEquationOptions& options){
    if(options.progress){char message[128];options.format(message,"Solving endpoint voltage equations...(%dx%d)",n>0?n-1:0,n>0?n-1:0);options.warnings.emit(message);}
    double values[8];
    if(!folded_port_network(reinterpret_cast<const double*>(cap),reinterpret_cast<const double*>(ind),n,n,n,nullptr,0,values,reinterpret_cast<double*>(cap))){options.warnings.emit("Folded endpoint equations could not produce finite admittance.");return false;}
    ports={{values[0],values[1]},{values[4],values[5]},{values[2],values[3]},{values[6],values[7]}};
    if(options.snapshot)options.snapshot("Ycap.out",{cap,n},n,options.snapshot_user);
    return true;
}
}
