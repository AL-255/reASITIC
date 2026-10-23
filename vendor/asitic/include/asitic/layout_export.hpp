#pragma once
#include "asitic/geometry.hpp"
#include "asitic/technology_text.hpp"
namespace asitic {
struct CifExportRuntime {
    void* (*open)(const char*,const char*,void*);
    int (*close)(void*,void*);
    int (*print)(void*,const char*,...);
    int (*time)(int*,void*);
    char* (*calendar_time)(const int*,void*);
    CharacterTables characters;
    WarningSink warnings;
    void* user;
};
enum class CifMode : int { box=1,wire=2,polygon=3 };
// Original 0x0808b30c. Bounds of the four outer/inner planar corners;
// each destination is written three times, retaining native alias semantics.
void cif_segment_bounds(const ConductorSegment&,double& xmin,double& ymin,double& xmax,double& ymax);
// Originals 0x0808b450/0x0808b4e8. Via enclosure uses the first other segment
// whose axis-aligned bounds contain the via. A missing enclosure warns and
// returns null; the exporter subsequently dereferences that null as native does.
void cif_via_enclosure(const ConductorSegment& metal,const ConductorSegment& via,double& width,double& height);
const ConductorSegment* cif_find_enclosing_segment(const GeometryShape&,const ConductorSegment&,WarningSink={});
// Original 0x0808b59c. The native CIF layer index is the word at segment+0x40;
// it is distinct from its technology metal/via index at +0xdc.
void cif_emit_layer(int,void*,const TechnologyTables&,const CifExportRuntime&);
// Originals 0x0808abf0/0x0808b5e8/0x0808b6b8/0x0808afc8. CIF width is the
// binary64 field at segment+0x30, not the discretizer's width at +0xcc.
// Conversion uses native x87 truncation, including INT_MIN for invalid input.
void cif_emit_box(const GeometryShape&,const ConductorSegment&,void*,double scale,double resolution,const CifExportRuntime&);
void cif_emit_wire(const GeometryShape&,const ConductorSegment&,void*,double scale,double resolution,const CifExportRuntime&);
void cif_emit_polygon(const GeometryShape&,const ConductorSegment&,void*,double scale,double resolution,const CifExportRuntime&);
void cif_emit_via(const GeometryShape&,const ConductorSegment&,void*,double scale,double resolution,const TechnologyTables&,const CifExportRuntime&);
// Original 0x0808b7fc. Exports this one named shape, follows +ec primary links,
// writes layer changes and via arrays, then closes the file. Mode must be 1..3;
// the native function has an uninitialized indirect-call target otherwise.
// Time is acquired before opening. The calendar callback returns mutable text,
// normalized in place with flags0x80. Print/close failures are ignored natively.
void export_cif(const GeometryShape&,const char* filename,double scale,double resolution,CifMode,
    const TechnologyTables&,const CifExportRuntime&);
}
