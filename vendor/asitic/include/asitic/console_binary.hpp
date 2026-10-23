#pragma once
#include "asitic/binary_dispatch.hpp"
namespace asitic {
// Ordinary libc host bindings for BSAVE/BLOAD/BCAT. Incoming native stack
// histories have no process-independent value: this host starts them at zero
// and retains them explicitly between commands. Raw saved pointers are kept.
struct ConsoleBinaryState {
    BinarySessionRuntime runtime;
    BinarySessionLoadHistory history;
    BinarySessionFileHistory conflict_history;
    BinarySegmentHistory* segments;
    unsigned capacity,shape_reads;
    bool loading;
    void (*invalid)(void*);
    void* user;
};
void initialize_console_binary(ConsoleBinaryState&,const GeometryRuntime&,const CommandRuntime&,
    void (*invalid)(void*),void* user);
// Handles ids 201/202/205 only. Histories grow from the actual input file's
// length bound and observed shape reads; no fixed shape-count limit is used.
bool execute_console_binary_command(int,const CommandArguments&,GeometryShape*& head,
    ConsoleBinaryState&,const CommandRuntime& current_parser);
void release_console_binary(ConsoleBinaryState&);
}
