#pragma once
#include "asitic/geometry.hpp"
#include "asitic/technology_records.hpp"
namespace asitic {
struct SonnetExportHistory { double cell_x,cell_y; };
struct SonnetExportRuntime {
    void (*prompt)(const char*,char*,unsigned char,void*);
    void* (*open)(const char*,const char*,void*);
    int (*close)(void*,void*);
    int (*print)(void*,const char*,...);
    int (*scan)(const char*,const char*,...);
    WarningSink warnings;
    void* user;
};
// Original 0x08091dc0. Filename prompt uses an 80-byte buffer. Cell prompts
// repeat for negative or above-chip dimensions; zero and NaN are accepted.
// Failed scans retain the supplied incoming history. Dense metals must include
// the element metal_count-1 even when the count is zero (native negative read).
// Original counting repeats the first shape's segment bank for each shape.
// Segment metal upper bounds are checked; negative metal indices are emitted.
// Output errors and close failures are ignored, as in the original.
void export_sonnet(const GeometryShape*,const TechnologyTables&,double chip_width,double chip_height,
    SonnetExportHistory&,const SonnetExportRuntime&);
}
