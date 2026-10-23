#pragma once
#include "asitic/technology_records.hpp"

namespace asitic {
// Ctype tables use the original glibc ABI and signed-char indexing. Backing
// storage covers indices -128..255. Space is bit 0x2000, control bit 0x0002.
struct CharacterTables {
    const unsigned short* classification;
    const int* uppercase;
    const int* lowercase;
};
// Original 0x0807adc0. Normalizes in place and returns the incoming pointer.
// Trailing trim reads one byte before an empty result before checking bounds.
char* normalize_input_line(char* line,unsigned flags,const CharacterTables& tables);
struct TechnologyInput {
    char* name;       // native 80-byte technology name buffer
    char* directory;  // native 200-byte directory buffer
    double width,height;
    int fft_x,fft_y;
    double frequency;
    int eddy_count;
    int* eddy_indices; // includes backing for any incoming negative count
};
struct TechnologyTextRuntime {
    CharacterTables characters;
    char* (*read_line)(char*,int,void* file);
    int (*scan)(const char*,const char*,...);
};
struct TechnologyParseHistory { double number; };
// Original 0x0807b328. Input lines use fgets(...,256,...), section/parameter
// matching uses prefixes, and failed scans retain their destination bytes.
// The number history represents the original incoming/final stack double.
// Name/color/path values must fit their native destination buffers. Frequency,
// eddy settings and existing record heads are retained until explicitly changed.
// Always returns zero after EOF; allocation fatal callbacks may terminate sooner.
int parse_technology(void* file,TechnologyInput& input,TechnologyRecords& records,
    TechnologyParseHistory& history,const TechnologyRuntime& runtime,const TechnologyTextRuntime& text);
// Original 0x0807bdac. Returns true on invalid chip/FFT input, false otherwise.
// Supplies defaults and preserves the original ceil/log/power FFT adjustment,
// including the extra factor of two for ordinary non-power-of-two counts.
bool validate_technology_input(const char* source_name,TechnologyInput& input,const TechnologyRuntime& runtime);
struct TechnologyFiles {
    void* (*open)(const char*,const char*,void* user);
    int (*close)(void*,void* user);
    void (*prompt)(char*,unsigned char,void* user);
    WarningSink prompt_output;
    void* user;
};
struct TechnologyInitialization {
    TechnologyInput input;
    TechnologyRecords records;
    TechnologyTables tables;
    TechnologyColors colors;
    GreenIntegrationState green;
    int cached_eddy;
    double* depths;
    ComplexValue* coefficients;
    double* metal_heights;
    int* group_sizes;
    double half_width,half_height,thickness_cell,length_step,width_step;
};
// Original 0x0807af80. Opens the file (or uses the runtime prompt for a null
// name), parses/validates records, converts cumulative heights and metal Z,
// allocates numerical work banks and releases temporary lists on success.
// The complete filename, including an appended .tek, must fit 80 bytes.
// Error paths retain partial state; vector constructors can fault before
// allocation checks, matching the original. No rollback is performed.
bool load_technology(const char* source_name,TechnologyInitialization& state,
    TechnologyParseHistory& history,const TechnologyRuntime& runtime,
    const TechnologyTextRuntime& text,const TechnologyFiles& files);
// Ordinary libc adapters. Linking callers need i386 libc. The file adapter
// leaves prompt callbacks empty; supply them before passing a null filename.
TechnologyTextRuntime libc_technology_text();
TechnologyFiles libc_technology_files();
}
