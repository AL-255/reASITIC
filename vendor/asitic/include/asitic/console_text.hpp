#pragma once
#include "asitic/console_geometry.hpp"
#include "asitic/text_sessions_commands.hpp"
#include "asitic/layout_export.hpp"
#include "asitic/layout_export_sonnet.hpp"
namespace asitic {
struct ConsoleTextState {
    const TechnologyInitialization* technology;
    ConsoleGeometryState* geometry;
    CommandRuntime* parser;
    TextSessionLoadHistory history;
    unsigned segment_history_capacity;
    SonnetExportHistory sonnet_history;
    void (*read)(const char*,char*,void*);
    void (*invalid)(void*);
    void* user;
};
// Ordinary FILE bindings for the reconstructed SAVE/LOAD/CAT and CIF/Sonnet
// components. Per-shape incoming histories are zeroed host workspaces, grown
// from an actual fgets-chunk count of the input file rather than a fixed cap.
void initialize_console_text(ConsoleTextState&,const TechnologyInitialization&,ConsoleGeometryState&,
    CommandRuntime&,void (*read)(const char*,char*,void*),void (*invalid)(void*),void* user);
// Native dispatcher cases225/226/227/300/302. References attached at
// initialization are read afresh on every command. CIF mode text is normalized
// in place; its native default is polygon, scale1, resolution100.
bool execute_console_text_command(int,CommandArguments&,GeometryShape*& head,ConsoleTextState&,
    bool* suppress_recording=nullptr);
void release_console_text(ConsoleTextState&);
}
