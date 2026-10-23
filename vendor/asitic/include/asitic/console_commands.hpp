#pragma once
#include "asitic/console_runtime.hpp"
#include "asitic/command_session.hpp"
namespace asitic {
struct ConsoleCommandRuntime {
    const ConsoleRuntime* output;
    const ReplInputRuntime* input;
    ReplAcquisition acquisition;
    CommandScriptWorkspace* script_workspace;
    CommandParseHistory* script_histories;
    const CommandScriptRuntime* script;
};
// Native 0x0805bd40 cases LOG203, EXEC209, RECORD213, INPUT215, PAUSE216.
// script_recording is the separate native 0x80cbab9 byte; output.recording is
// the logging byte0x80cbab8. Native RECORD does not share LOG's enable flag.
// Optional recording postlude is excluded. suppress_recording also reports
// native early returns, so an INPUT with no stored path is not recorded.
// Input prompt_echo mirrors output.recording on entry/normal return because
// both represent the same native global; keep them synchronized elsewhere.
bool execute_console_command(int,const CommandArguments&,ReplInputState&,ConsoleOutputState&,
    unsigned char& script_recording,const ConsoleCommandRuntime&,bool* suppress_recording=nullptr);
}
