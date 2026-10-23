#pragma once
#include "asitic/command_runtime.hpp"
#include "asitic/geometry_edit.hpp"
#include "asitic/shape_prompts.hpp"
namespace asitic {
struct GeometryCommandState { GeometryShape* head; GeometryShape* selected; double chip_x,chip_y; };
struct GeometryDispatchRuntime {
    GeometryCommandRuntime commands;
    GeometryTechnology technology;
    CharacterTables characters;
    int (*resolve_metal)(char*,void*);
    void* user;
};
// Geometry cases of original0x0805bd40:400/401/402/404..409/411/412/
// 414..418/420..424. Input is the native parsed record layout. Returns false
// for other ids; optional suppress_recording receives the native local flag.
// Excludes the shared optional command-recording postlude. Native JOINSHUNT
// falls through PHASE using the second shape pointer word as its phase value.
// Callback resolve_metal binds find_shape_prompt_metal (including input mutation).
// SELECT retains the native null-result dereference; invalid lists/partners,
// conversion faults and partial geometry changes are deliberately retained.
bool execute_geometry_command(int,CommandArguments&,GeometryCommandState&,const GeometryDispatchRuntime&,
    bool* suppress_recording=nullptr);
}
