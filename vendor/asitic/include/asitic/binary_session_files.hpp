#pragma once
#include "asitic/geometry.hpp"

namespace asitic {
enum class BinarySessionError {
    invalid_header,invalid_version,invalid_magic,file_corrupted,name_conflict,
    unable_to_open_read,unable_to_open_write
};
// Exceptions are exposed after the original helper's catch cleanup. raise must not return;
// payload is the native version, conflicting name, or composed filename.
struct BinarySessionFiles {
    void* (*open)(const char*,const char*,void*);
    unsigned (*read)(void*,unsigned,unsigned,void*,void*);
    unsigned (*write)(const void*,unsigned,unsigned,void*,void*);
    int (*close)(void*,void*);
    int (*seek)(void*,long,int,void*);
    void (*raise)(BinarySessionError,const char*,void*);
    void* user;
    void* error_user;
    WarningSink warnings;
    int (*format)(char*,const char*,...);
    char* line;
};
struct BinarySkipHistory {
    unsigned marker;
    ConductorSegment parent,child;
};
// fread results are intentionally ignored. Incoming stack bytes therefore
// remain observable on short reads; callers supply the corresponding history.
struct BinarySessionFileHistory {
    char signature[6],version[9],magic[6];
    unsigned marker;
    GeometryShape shape;
    BinarySkipHistory skipped;
    // open-for-write validates in two distinct native stack frames. Supply
    // the conflict scanner's independent incoming history when its reads can
    // be short; complete reads do not need a separate object.
    BinarySessionFileHistory* conflict_validation = nullptr;
};
// Original 0x080744c0; implemented by the binary record module.
void skip_binary_session_records(void* file,unsigned& next,const BinarySessionFiles& files,BinarySkipHistory& history);
// Originals 0x08073e3c and 0x08078274. Header bytes are ASITIC, 10.20.99,
// 314159, without separators. Validation also accepts the first six version
// bytes 5.2.99, regardless of the following two bytes.
void validate_binary_session_header(void* file,const BinarySessionFiles& files,BinarySessionFileHistory& history);
void emit_binary_session_header(void* file,const BinarySessionFiles& files);
// Original 0x08074a70. Seeks to the start, validates the header, and scans
// raw records for an exact name conflict. It leaves the file open at EOF.
void check_binary_session_conflict(void* file,const char* name,const BinarySessionFiles& files,BinarySessionFileHistory& history);
// Originals 0x080742e8 and 0x080746a4. A case-sensitive substring .SPI
// anywhere in the incoming name suppresses extension insertion. The complete
// path must fit the original 80-byte buffer. Existing-file writes validate
// twice and seek four bytes before EOF, ignoring seek/write failures.
void* open_binary_session_read(const char* name,const BinarySessionFiles& files,BinarySessionFileHistory& history);
void* open_binary_session_write(const char* name,const char* shape_name,const BinarySessionFiles& files,BinarySessionFileHistory& history);
}
