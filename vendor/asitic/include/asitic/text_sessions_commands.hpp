#pragma once
#include "asitic/text_sessions.hpp"
namespace asitic {
struct TextSessionFileHistory {
    SessionHeaderHistory header[2];
    GeometryShape shape;
    SessionRecordHistory record;
};
struct TextSessionSegmentHistory {
    ConductorSegment parent,child;
    SessionRecordHistory parent_record,child_record;
};
struct TextSessionLoadHistory {
    TextSessionFileHistory file;
    // One fresh incoming factory-frame history for each shape actually loaded.
    TextSessionSegmentHistory* segments;
};
struct TextSessionCommandRuntime {
    TextSessionRuntime files;
    TextSessionLayers layers;
    GeometryRuntime geometry;
    GeometryShape* selected;
    SessionGeometryParseOptions record;
    int (*format)(char*,const char*,...);
    char* line;
    WarningSink output;
};
// Originals 0x08076d14 and 0x08076660. Existing files are validated twice;
// the write stream seeks ten bytes before EOF to overwrite BLOCK=END.
void check_text_session_conflict(void*,const char* shape_name,
    const TextSessionCommandRuntime&,TextSessionFileHistory&);
void* open_text_session_output(const char* filename,const char* shape_name,
    const TextSessionCommandRuntime&,TextSessionFileHistory&);
// Original 0x0807656c. Text record parsing consumes ENDSEGMENT/ENDSHUNT;
// this outer factory requires ENDSPIRAL and reverses primary links once.
void load_text_session_segments(GeometryShape&,void*,const TextSessionCommandRuntime&,
    TextSessionSegmentHistory&);
// Originals 0x08075204, 0x0807557c, 0x08075978, 0x08075d58, 0x08076198.
// Catalog listing leaves its stream open. Selected-name loading reverses
// primary links twice; all-name loading skips exact conflicts in memory.
void load_text_session(GeometryShape*& head,const char* filename,
    const TextSessionCommandRuntime&,TextSessionLoadHistory&);
void save_text_session(GeometryShape* head,const char* filename,
    const TextSessionCommandRuntime&,TextSessionFileHistory&);
void save_text_session_shape(const GeometryShape&,const char* filename,
    const TextSessionCommandRuntime&,TextSessionFileHistory&);
void list_text_session(const char* filename,const TextSessionCommandRuntime&,
    TextSessionFileHistory&);
void load_text_session_shape(GeometryShape*& head,const char* shape_name,const char* filename,
    const TextSessionCommandRuntime&,TextSessionLoadHistory&);
// The SAVE/LOAD/CAT branches of 0x0805bd40 (IDs 225/226/227). Argument
// parsing and the dispatcher's common optional debug report surround this API.
// SAVE's optional boolean is passed but ignored by the native file writers.
bool execute_text_session_command(int id,const CommandArguments&,GeometryShape*& head,
    const TextSessionCommandRuntime&,TextSessionLoadHistory&);
}
