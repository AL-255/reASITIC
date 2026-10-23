#pragma once

namespace asitic {
#pragma pack(push,4)
// Recovered fields of the original conductor polygon record. Unknown bytes
// remain explicit; no ownership or C++ runtime layout is assumed here.
struct ConductorSegment {
    double endpoints[6];                  // +0x00
    unsigned char prefix_rest[0x14];
    double outer_start[3];                 // +0x44
    double outer_end[3];                   // +0x5c
    unsigned char gap[0x14];
    double inner_start[3];                 // +0x88
    double inner_end[3];                   // +0xa0
    unsigned char gap2[0x14];
    double width;                         // +0xcc
    double thickness;                     // +0xd4
    int metal;                            // +0xdc
    int width_count;                      // +0xe0
    int thickness_count;                  // +0xe4
    unsigned group_next_address;          // +0xe8, secondary segment chain
    unsigned next_address;                // +0xec, original i386 pointer
};
struct MetalLayer {
    unsigned char prefix[0xb0];
    double thickness;
    double resistance;
    unsigned char suffix[0x2c];
};
struct ViaLayer {
    unsigned char prefix[0xa0];
    double perimeter_step;
    unsigned char gap[0x18];
    double resistance;
    unsigned char suffix[0x28];
};
struct Filament {
    double values[8];                     // start, end, width, thickness
    int metal;
};
#pragma pack(pop)
static_assert(__builtin_offsetof(ConductorSegment,inner_start)==0x88);
static_assert(__builtin_offsetof(ConductorSegment,width)==0xcc);
static_assert(__builtin_offsetof(ConductorSegment,metal)==0xdc);
static_assert(__builtin_offsetof(ConductorSegment,group_next_address)==0xe8);
static_assert(sizeof(ConductorSegment)==0xf0);
static_assert(sizeof(MetalLayer)==0xec);
static_assert(sizeof(ViaLayer)==0xf0);
static_assert(sizeof(Filament)==68);

struct FilamentSettings {
    double edge_cell_size;
    int max_width_cells;
    const MetalLayer* metals;
};

// Original 0x0806597c, with formerly global technology/mesh state explicit.
void build_filament(const ConductorSegment& segment, Filament& out,
                    const double* origin_xy, int x_index, int y_index, int z_index,
                    int nx, int ny, int nz, const FilamentSettings& settings);
}
