#pragma once
#include "asitic/technology_text.hpp"
#include "asitic/command_runtime.hpp"
namespace asitic {
enum class SessionParseFailure { invalid_header=1,file_corrupted=2,unable_to_create=3 };
struct SessionHeaderHistory { char name[80],version[80]; };
struct SessionParseRuntime {
    TechnologyTextRuntime text;
    char* line; // 1024-byte shared scratch; leading backing byte follows native trim reads
    int (*end)(void* file);
    int (*close)(void* file);
    WarningSink warnings;
    // This is the original exception-raising boundary. It must not return.
    void (*fail)(SessionParseFailure,void*);
    void* user;
};
// Original 0x08076fa0. Reads until a nonempty noncomment normalized line,
// ignores fgets' return, and recognizes complete BLOCK=... tokens only.
unsigned read_text_session_block(void* file,const SessionParseRuntime&);
// Original 0x080780e0. Stops at ENDSPIRAL, including before an EOF check;
// an EOF reached with another token raises file_corrupted.
void skip_text_session_shape(void* file,const SessionParseRuntime&);
// Original 0x08076bd0. The caller's history supplies the two incoming stack
// scan buffers; failed/partial scans retain bytes. Invalid headers emit the
// original diagnostic, close the stream and rethrow at the fail boundary.
void validate_text_session_header(void* file,SessionHeaderHistory&,const SessionParseRuntime&);
// Original 0x08078140. Returned labels have process lifetime.
const char* session_shape_description(unsigned type);
struct SessionRecordHistory {
    unsigned char leading;
    char line[1024];
    CommandArgument argument[3];
    // Indexed by named-field lookup order, including absent fields.
    CommandParseHistory scans[20];
    int metal_index,via_index;
};
struct SessionGeometryParseOptions {
    CommandRuntime commands;
    GeometryTechnology technology;
    int via_count;
    double chip_width,chip_height;
    SessionParseRuntime file;
};
// Original 0x080870e4. Copies into commands.message, tokenizes ':' then '=',
// chooses the first exact key and passes its right side to the typed parser.
// Native delimiter-only and missing-equals memory faults are preserved.
bool parse_named_session_argument(const char*,CommandArgument&,const char* key,unsigned type,
    const CommandRuntime&,CommandParseHistory&);
// Originals 0x08056494 / 0x080563fc. Numeric detection uses the first signed
// character's ctype digit bit; incoming failed-scan integer storage is explicit.
int lookup_session_metal(char*,const SessionGeometryParseOptions&,int& history);
int lookup_session_via(char*,const SessionGeometryParseOptions&,int& history);
// Originals 0x0807741c / 0x08077acc. The record is partly overwritten and
// explicit history supplies all otherwise unwritten local text/value storage.
void parse_text_session_shape(GeometryShape&,void*,SessionRecordHistory&,const SessionGeometryParseOptions&);
void parse_text_session_segment(const GeometryShape&,ConductorSegment&,void*,SessionRecordHistory&,const SessionGeometryParseOptions&);
}
