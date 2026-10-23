#include "asitic/technology_text.hpp"
extern "C" char* fgets(char*,int,void*);
extern "C" int sscanf(const char*,const char*,...);
extern "C" void* fopen(const char*,const char*);
extern "C" int fclose(void*);
extern "C" const unsigned short** __ctype_b_loc();
extern "C" const int** __ctype_toupper_loc();
extern "C" const int** __ctype_tolower_loc();
namespace asitic {
namespace {
void* open(const char* path,const char* mode,void*) { return fopen(path,mode); }
int close(void* file,void*) { return fclose(file); }
}
TechnologyTextRuntime libc_technology_text() {
    return {{*__ctype_b_loc(),*__ctype_toupper_loc(),*__ctype_tolower_loc()},fgets,sscanf};
}
TechnologyFiles libc_technology_files() { return {open,close,nullptr,{},nullptr}; }
}
