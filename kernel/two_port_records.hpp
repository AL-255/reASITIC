#pragma once
#include "asitic/two_port_output.hpp"
namespace reasitic {
// Native numerical records and raw S cells; scalar bank uses original address order.
char* two_port_json(char*,const asitic::TwoPortAdmittance&,const double*,bool,double,bool include_model=true);
}
