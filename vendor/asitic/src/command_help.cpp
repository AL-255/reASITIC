#include "asitic/command_help.hpp"
namespace asitic {
namespace {
struct HelpEntry { int id;const char* text; };
#include "command_help_catalog.inc"
bool equal(const char* a,const char* b) { while(*a && *a==*b){++a;++b;}return *a==*b; }
void copy(char* d,const char* s) { do { *d=*s;++d; } while(*s++); }
}
int lookup_command_id(const char* name,const CommandDefinition* rows) {
    const int index=lookup_command_index(name,rows);return index<0?-1:rows[index].id;
}
void print_command_help(const char* topic,const CommandRuntime& r) {
    copy(r.message,topic);normalize_input_line(r.message,0xae,r.characters);
    if(!*r.message) { r.warnings.emit(help_general_heading);r.warnings.emit(help_general_categories);return; }
    for(unsigned i=0;i<7;++i)if(equal(r.message,help_categories[i][0])) {
        r.warnings.emit(help_category_heading);r.warnings.emit(help_categories[i][1]);return;
    }
    const int index=lookup_command_index(r.message);
    if(index<0) { r.warnings.emit("Sorry, no help for this command.");return; }
    const int id=lookup_command_id(r.message);
    r.format(r.message,"%s",native_command_catalog()[index].usage);r.warnings.emit(r.message);
    unsigned i=0;while(help_entries[i].id!=-2 && help_entries[i].id!=id)++i;
    if(help_entries[i].id<0)return;
    r.format(r.message,"%s",help_entries[i].text);r.warnings.emit(r.message);
}
bool execute_help_command(int id,const CommandArguments& arguments,const CommandRuntime& r) {
    if(id!=206)return false;
    print_command_help(arguments.command.value==1?arguments.arguments[0].text:"",r);
    r.warnings.emit("");return true;
}
}
