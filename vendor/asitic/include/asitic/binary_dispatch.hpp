#pragma once
#include "asitic/binary_sessions.hpp"
#include "asitic/command_runtime.hpp"
namespace asitic {
// Cases 201, 202 and 205 of native 0x0805bd40 (bodies 0x0805d030,
// 0x0805d090 and 0x0805d0f1). The filename is argument zero. Exactly one
// argument, or exactly two with a second text equal to "*", selects all shapes.
// A selective save uses argument one's typed pointer; a selective load uses
// its original text. Excludes the dispatcher's optional recording postlude.
bool execute_binary_session_command(int id,const CommandArguments& arguments,GeometryShape*& head,
    const BinarySessionRuntime& runtime,BinarySessionLoadHistory& history);
}
