#pragma once
#include "asitic/potential_tables.hpp"
#include "asitic/technology.hpp"

namespace asitic {
struct PotentialCache {
    ComplexValue** xy = nullptr;
    ComplexValue** x = nullptr;
    ComplexValue** y = nullptr;
    double frequency = 0;
    bool enabled = false;
    bool dirty = false;
};
struct PotentialCacheTechnology {
    const SubstrateLayer* substrate;
    int substrate_count;
    const MetalLayer* metals;
    int metal_count;
};
// Standard C file/formatting services, explicit so an embedding runtime can
// supply libc or an observable equivalent. Sizes/counts use the i386 ABI.
struct PotentialCacheFiles {
    void* (*open)(const char* path,const char* mode,void* user);
    unsigned (*read)(void*,unsigned size,unsigned count,void* file,void* user);
    unsigned (*write)(const void*,unsigned size,unsigned count,void* file,void* user);
    int (*close)(void* file,void* user);
    int (*format)(char*,const char*,...);
    void* user = nullptr;
};
struct PotentialCacheRuntime {
    PotentialCacheFiles files;
    // Names/paths fit the native buffers: technology name <80 bytes,
    // generated filename <80 bytes and loaded filename <200 bytes.
    const char* directory;
    const char* technology_name;
    WarningSink warnings;
    WarningSink status;
};
// Header fread does not check its return values. Supply the incoming stack
// words when observing a short header read; successful reads replace them.
struct PotentialCacheHeaderHistory { int nx = 0,ny = 0;double frequency = 0; };
// Originals 0x0808bdd0/0x0808bccc. Allocation zeroes complex cells before null
// checks. Release leaves bank pointers unchanged, matching the original.
bool allocate_potential_cache(PotentialCache& cache,int nx,int ny,int metal_count,
    SolverMemory memory,WarningSink warnings = {});
void release_potential_cache(const PotentialCache& cache,int metal_count,SolverMemory memory);
// Original 0x0807d72c: selected technology fields are checked; file frequency
// and several record fields are ignored. Name/color mismatches only warn.
bool validate_potential_cache_header(void* file,int nx,int ny,const PotentialCacheTechnology& technology,
    SolverMemory memory,const PotentialCacheRuntime& runtime,PotentialCacheHeaderHistory history = {});
// Originals 0x0808b950/0x0808c0bc. Save/load raw header and upper-pair tables in
// original order, including ignored short-read/write results, dirty-state
// changes and the open-file leak after header validation failure.
bool generate_potential_cache(PotentialCache& cache,double frequency,bool verbose,
    const PotentialTableGeometry& geometry,const PotentialCacheTechnology& technology,
    SolverMemory memory,const PotentialCacheRuntime& runtime);
bool load_potential_cache(PotentialCache& cache,double frequency,int nx,int ny,
    const PotentialCacheTechnology& technology,SolverMemory memory,const PotentialCacheRuntime& runtime,
    PotentialCacheHeaderHistory history = {});
}
