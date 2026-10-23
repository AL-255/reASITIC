#include "asitic/binary_session_files.hpp"

namespace asitic {
namespace {
bool equal_prefix(const char* a,const char* b,unsigned n) {
    for(unsigned i=0;i<n;++i) { if(a[i]!=b[i])return false;if(!a[i])return true; }return true;
}
bool equal_name(const char* a,const char* b) {
    while(*a && *a==*b) { ++a;++b; }return *a==*b;
}
[[noreturn]] void raise(const BinarySessionFiles& files,BinarySessionError error,const char* payload=nullptr) {
    if(files.raise)files.raise(error,payload,files.error_user);
    __builtin_trap();
}
void compose_path(char* out,const char* name) {
    bool extension=false;
    for(const char* p=name;*p;++p)if(p[0]=='.' && p[1]=='S' && p[2]=='P' && p[3]=='I')extension=true;
    while(*name)*out++=*name++;
    if(!extension)for(const char* p=".SPI";*p;++p)*out++=*p;
    *out=0;
}
}
void validate_binary_session_header(void* file,const BinarySessionFiles& files,BinarySessionFileHistory& history) {
    files.read(history.signature,6,1,file,files.user);
    files.read(history.version,8,1,file,files.user);
    files.read(history.magic,6,1,file,files.user);
    history.version[8]=0;
    if(!equal_prefix(history.signature,"ASITIC",6)) {
        files.warnings.emit("Error:  File does not have valid header.");files.close(file,files.user);
        raise(files,BinarySessionError::invalid_header);
    }
    if(!equal_prefix(history.version,"10.20.99",8) && !equal_prefix(history.version,"5.2.99",6)) {
        files.format(files.line,"Error:  File created by incompatible version %s.",history.version);
        files.warnings.emit(files.line);files.close(file,files.user);
        raise(files,BinarySessionError::invalid_version,history.version);
    }
    if(!equal_prefix(history.magic,"314159",6)) {
        files.warnings.emit("Error:  File has invalid magic number.");files.close(file,files.user);
        raise(files,BinarySessionError::invalid_magic);
    }
}
void emit_binary_session_header(void* file,const BinarySessionFiles& files) {
    files.write("ASITIC",6,1,file,files.user);
    files.write("10.20.99",8,1,file,files.user);
    files.write("314159",6,1,file,files.user);
}
void check_binary_session_conflict(void* file,const char* name,const BinarySessionFiles& files,BinarySessionFileHistory& history) {
    files.seek(file,0,0,files.user);
    validate_binary_session_header(file,files,history);
    files.read(&history.marker,4,1,file,files.user);
    while(history.marker!=0x8b) {
        if(history.marker!=0x88)raise(files,BinarySessionError::file_corrupted);
        files.read(&history.shape,0xbc,1,file,files.user);
        if(equal_name(reinterpret_cast<const char*>(&history.shape),name))raise(files,BinarySessionError::name_conflict,name);
        skip_binary_session_records(file,history.marker,files,history.skipped);
    }
}
void* open_binary_session_read(const char* name,const BinarySessionFiles& files,BinarySessionFileHistory& history) {
    char path[80];compose_path(path,name);void* file=files.open(path,"r",files.user);
    if(!file)raise(files,BinarySessionError::unable_to_open_read,path);
    validate_binary_session_header(file,files,history);
    return file;
}
void* open_binary_session_write(const char* name,const char* shape_name,const BinarySessionFiles& files,BinarySessionFileHistory& history) {
    char path[80];compose_path(path,name);void* file=files.open(path,"r",files.user);
    if(!file) {
        file=files.open(path,"w",files.user);
        if(!file)raise(files,BinarySessionError::unable_to_open_write,path);
        emit_binary_session_header(file,files);
    } else {
        validate_binary_session_header(file,files,history);files.close(file,files.user);
        file=files.open(path,"r+",files.user);
        if(!file)raise(files,BinarySessionError::unable_to_open_write,path);
        check_binary_session_conflict(file,shape_name,files,history.conflict_validation?*history.conflict_validation:history);
        files.seek(file,-4,2,files.user);
    }
    return file;
}
}
