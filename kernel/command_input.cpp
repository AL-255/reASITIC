#include "engine.hpp"
#include "asitic/command_runtime.hpp"
extern "C" int sscanf(const char*,const char*,...);
extern "C" int sprintf(char*,const char*,...);
// Native types 4 / 5 / 7 / 10 retain original scalar, GHz, length/grid and
// angle behavior, including native negative-angle normalization quirks.
extern "C" double normalize_value(double input,int type,double grid,int snap) {
    if(!__builtin_isfinite(input) || !__builtin_isfinite(grid) || grid<=0 ||
       (type!=4 && type!=5 && type!=7 && type!=10))return __builtin_nan("");
    asitic::CommandArgument argument{};asitic::CommandParseHistory history{};
    sprintf(argument.text,"%.17g",input);char message[512];
    asitic::CommandRuntime runtime{};runtime.scan=sscanf;runtime.format=sprintf;runtime.message=message;
    runtime.frequency_scale=1e9;runtime.length_scale=1e-6;runtime.grid_size=grid;runtime.snap=snap!=0;
    runtime.warnings=reasitic::engine_view().warnings;
    if(!asitic::parse_command_argument(argument,type,runtime,history))return __builtin_nan("");
    float value;__builtin_memcpy(&value,&argument.value,sizeof(value));return value;
}
