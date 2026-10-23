#pragma once
#include "asitic/command_runtime.hpp"
namespace asitic {
// Originals 0x0804c620 and 0x08079810. Formats raw native geometry fields;
// segment reporting follows +ec primary links and +e8 secondary links.
void print_geometry_report(const GeometryShape&,const GeometryTechnology&,const CommandRuntime&);
void print_geometry_segments(const GeometryShape&,const GeometryTechnology&,const CommandRuntime&);
// Original 0x0804ee74 returns extended precision area; it does not print.
// Only primary segments longer than the native 1e-10 threshold contribute.
long double geometry_metal_area(const GeometryShape&);
// Original 0x08063fc8. Checks every pair of primary records after native
// 1e-4 scaling, printing original coordinates for classification3 intersections.
void print_geometry_intersections(const GeometryShape&,const CommandRuntime&);
// Native 0x0805bd40 cases WHO224, LISTSEGS210, GEOM410, METALAREA413, FINDI419.
// Excludes the enclosing switch's optional command-recording postlude.
bool execute_geometry_reporting_command(int,const CommandArguments&,GeometryShape* head,
    const GeometryTechnology&,const CommandRuntime&);
}
