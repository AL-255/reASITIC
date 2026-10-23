#pragma once
#include "asitic/three_port_transforms.hpp"
#include "asitic/geometry.hpp"
#include "asitic/technology_text.hpp"
namespace asitic {
struct PortOutputRuntime {
    int (*print)(void*,const char*,...);
    SolverMemory memory;
    lapack::GeneralInverseRuntime inverse;
    WarningSink warnings;
};
struct ThreePortOutputState {
    ThreePortImpedance impedance;
    ThreePortAdmittance admittance;
    ThreePortScattering scattering;
};
// Originals 0x08088b24 and 0x0808aa50. Preserve missing labels, repeated
// per-row comment prefixes and the native character mask in three-port labels.
void write_two_port_header(void*,int parameter,unsigned char polar,const PortOutputRuntime&);
void write_three_port_header(void*,int parameter,unsigned char polar,const PortOutputRuntime&);
// Original 0x080890bc. Only S/Y/Z modes emit a record. Cross cells use the
// native reciprocal scattering, and the third row's middle cell stays zero.
// Polar conversion repeatedly writes its first
// local cell; the final first cell is the polar form of the last local cell.
void write_three_port_record(void*,double frequency,int parameter,unsigned char polar,
    ThreePortOutputState&,const PortOutputRuntime&);

struct PortFileHeaderState {
    const char* technology_name;
    const char* technology_directory;
    TechnologyTables tables;
    double inductance_length,inductance_width,inductance_thickness;
    double capacitance_length,capacitance_width,alpha,beta;
    unsigned char automatic_cells;
};
struct PortFileHeaderHistory { int timestamp; };
struct PortFileHeaderRuntime {
    PortOutputRuntime output;
    CharacterTables characters;
    char* (*calendar)(const int*);
};
// Originals 0x08074cd0 and 0x08074e50. Native formatting transfers doubles
// as two raw argument words, without quieting signaling NaNs.
void write_port_shape_summary(void*,const GeometryShape&,const PortOutputRuntime&);
void write_port_technology_summary(void*,const PortFileHeaderState&,const PortOutputRuntime&);
// Original 0x0808aafc. The incoming stack timestamp is explicit. Its calendar
// string is normalized in place, then the complete shape/technology comments
// and optional fixed-cell details are emitted in native order.
void write_port_file_header(void*,unsigned char fast,const GeometryShape&,const char* command,
    const PortFileHeaderState&,PortFileHeaderHistory&,const PortFileHeaderRuntime&);
}
