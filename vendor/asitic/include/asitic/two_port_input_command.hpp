#pragma once
#include "asitic/command_runtime.hpp"
#include "asitic/analysis_reports.hpp"
#include "asitic/network.hpp"
namespace asitic {
// Native0x0805bd40 case537 (2PZIN). Optional arguments are load real/imag,
// port, first shunt C and second shunt C; numeric values are parsed float words.
// Capacitors use the last cache-I/O frequency (native0x080ce578). Both diagonal
// cells undergo ordered add/sub stores even for omitted zero capacitors, so
// rounding, signed-zero and NaN changes remain visible in admittance.
// Returns false for other command ids. The enclosing runner owns recording.
bool execute_two_port_input_command(int,CommandArguments&,TwoPortAdmittance&,const double& cached_frequency,
    const AnalysisReportRuntime&);
}
