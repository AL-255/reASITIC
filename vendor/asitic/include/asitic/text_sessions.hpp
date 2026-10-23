#pragma once
#include "asitic/shape_construction.hpp"
#include "asitic/text_session_parse.hpp"

namespace asitic {
struct TextSessionLayers {
    const MetalLayer* metals;
    const ViaLayer* vias;
    int metal_count;
};
// These callbacks use the native i386 fprintf argument layout. Double values
// are passed as their two untouched words, including signaling-NaN payloads.
struct TextSessionOutput {
    int (*print)(void* file,const char* format,...);
};
enum class TextSessionFailure { unable_to_read=1,unable_to_write=2,name_conflict=3,file_corrupted=4,invalid_header=5,unable_to_create=6 };
struct TextSessionFiles {
    void* (*open)(const char*,const char*);
    int (*close)(void*);
    int (*seek)(void*,int,int);
};
struct TextSessionRuntime {
    TextSessionFiles files;
    TextSessionOutput output;
    SessionParseRuntime parsing;
    // Corresponds to native file/name exception creation. Must not return.
    void (*fail)(TextSessionFailure,const char* detail,void*);
    void* user;
};
// Originals 0x080770e0, 0x08077278, 0x08078024 and 0x080780c8.
// Shape fields use %lg, segment fields %lf. Secondary segments follow +e8.
void write_text_session_shape_fields(const GeometryShape& shape,void* file,
    const TextSessionLayers& layers,const TextSessionOutput& output);
void write_text_session_segment(const ConductorSegment& segment,void* file,
    const TextSessionLayers& layers,const TextSessionOutput& output);
void write_text_session_shape(const GeometryShape& shape,void* file,
    const TextSessionLayers& layers,const TextSessionOutput& output);
void write_text_session_version(void* file,const TextSessionOutput& output);
// Original 0x08076a00. The literal substring .spi suppresses appending the
// extension anywhere in the name. The output pointer changes after validation.
void open_text_session_input(const char* filename,void*& file,
    SessionHeaderHistory& history,const TextSessionRuntime& runtime);
}
