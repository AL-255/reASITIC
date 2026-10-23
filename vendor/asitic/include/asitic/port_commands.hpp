#pragma once
#include "asitic/geometry.hpp"
#include "asitic/technology_text.hpp"
namespace asitic {
struct PortFormatRuntime {
    CharacterTables characters;
    int (*format)(char*,const char*,...);
    char* message;
    WarningSink warnings;
};
// Originals 0x0808a2d0 / 0x0808a518. Both normalize their argument in place.
// The parameter parser returns S=1,Y=2,Z=3,PI=4,TRANSPI=5, unknown=0.
// The representation parser returns polar=true, rectangular=false; unknown
// inputs warn and retain the native polar default.
int parse_port_parameter_format(char*,const PortFormatRuntime&);
bool parse_port_polar_format(char*,const PortFormatRuntime&);

enum class PortSweepKind : unsigned { Ground, Extended, Pad, Transformer, ThreePort };
struct PortSweepFilename { char* data; unsigned length; };
struct PortSweepSettings {
    PortSweepFilename filename;
    double first_frequency,last_frequency,frequency_step;
    unsigned char two_port_option,polar;
    int parameter_format;
    unsigned char three_port_option;
    const unsigned char* progress;
};
struct PortSweepRuntime {
    void* (*open)(const char*,const char*,void*);
    int (*flush)(void*,void*);
    int (*close)(void*,void*);
    void (*header)(void*,unsigned char,const GeometryShape*,const char*,void*);
    void (*two_port_columns)(void*,int,unsigned char,void*);
    void (*three_port_columns)(void*,void*);
    void (*update_frequency_geometry)(double,void*);
    // Analyze receives the native byte option and the report flag.
    bool (*analyze)(PortSweepKind,const GeometryShape* const*,double,double,unsigned char,bool,void*);
    void (*two_port_record)(void*,double,int,unsigned char,void*);
    void (*three_port_record)(void*,double,void*);
    WarningSink warnings;
    void* user;
};
// Originals 0x08087770/0x08087894/0x080879b4/0x08087ad0/0x08087bf4.
// Runtime analysis callbacks bind the corresponding complete network drivers.
// Ground/Extended stop when their driver fails; the other variants retain
// the native behavior of writing the current state even after a failed solve.
// Each step rounds its saved frequency to binary64 while comparing the
// unrounded x87 sum with the saved upper bound. Nonpositive steps can loop.
void run_port_frequency_sweep(PortSweepKind,const GeometryShape* const* shapes,double termination,
    const PortSweepSettings&,const PortSweepRuntime&);
}
