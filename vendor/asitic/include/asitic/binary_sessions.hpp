#pragma once
#include "asitic/binary_session_files.hpp"

namespace asitic {
struct BinarySegmentHistory {
    unsigned marker;
    ConductorSegment parent,child;
};
// The original loader creates a fresh stack frame for each shape. Supply one
// incoming segment history for each shape that is actually loaded.
struct BinarySessionLoadHistory {
    BinarySessionFileHistory file;
    unsigned marker;
    GeometryShape shape;
    BinarySkipHistory skipped;
    BinarySegmentHistory* segments;
};
struct BinarySessionRuntime {
    BinarySessionFiles files;
    GeometryRuntime geometry;
    GeometryShape* selected;
    int (*format)(char*,const char*,...);
    char* line_buffer;
    WarningSink output;
};
// Original 0x0807a57c. Inserts a raw child at the first primary segment,
// inheriting that primary's next link. The source's embedded pointers are not
// followed or copied recursively.
void prepend_binary_session_child(const ConductorSegment*& head,const ConductorSegment& source,
    const GeometryRuntime& runtime);
// Originals 0x080781c0 and 0x08074580. Serialization includes the complete raw
// records, including pointer/history fields. Loading reconstructs links and
// reverses the primary list; children remain in reverse serialized order.
void emit_binary_session_shape(const GeometryShape& shape,void* file,const BinarySessionFiles& files);
void load_binary_session_segments(GeometryShape& shape,void* file,unsigned& next,
    const BinarySessionFiles& files,const GeometryRuntime& runtime,BinarySegmentHistory& history);
// Originals 0x08072b50, 0x08072edc, 0x080732a8, 0x080736b8 and 0x08073aa8.
// These are the binary commands, despite their historical .SPI extension.
// Read failures are ignored exactly as in the original; histories retain the
// unread bytes. The selected-name loader reverses its primary list a second
// time. Successful catalog listing deliberately leaves the file open.
void load_binary_session(GeometryShape*& head,const char* filename,
    const BinarySessionRuntime& runtime,BinarySessionLoadHistory& history);
void load_binary_session_shape(GeometryShape*& head,const char* name,const char* filename,
    const BinarySessionRuntime& runtime,BinarySessionLoadHistory& history);
void save_binary_session(GeometryShape* head,const char* filename,
    const BinarySessionRuntime& runtime,BinarySessionFileHistory& history);
void save_binary_session_shape(const GeometryShape& shape,const char* filename,
    const BinarySessionRuntime& runtime,BinarySessionFileHistory& history);
void list_binary_session(const char* filename,const BinarySessionRuntime& runtime,
    BinarySessionLoadHistory& history);
}
