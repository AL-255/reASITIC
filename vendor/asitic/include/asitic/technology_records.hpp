#pragma once
#include "asitic/technology.hpp"
#include "asitic/solver.hpp"

namespace asitic {
#pragma pack(push,4)
// Native parsed and dense records share these layouts. A dense record leaves
// its next address and trailing bytes untouched. Addresses use the i386 ABI.
struct TechnologySubstrate {
    int index;double permittivity,resistivity,height,thickness;
    unsigned next_address;
};
struct TechnologyMetal {
    char name[80],color[80];int substrate,index;
    double z,thickness,resistance,minimum_spacing,minimum_width;
    unsigned next_address;unsigned char reserved[24];
};
struct TechnologyVia {
    char name[80],color[80];
    double width,spacing,overplot1,overplot2,resistance;
    int index,bottom,top;unsigned next_address;unsigned char reserved[24];
};
#pragma pack(pop)
static_assert(sizeof(TechnologySubstrate)==40);
static_assert(sizeof(TechnologyMetal)==236);
static_assert(sizeof(TechnologyVia)==240);
static_assert(__builtin_offsetof(TechnologyMetal,next_address)==0xd0);
static_assert(__builtin_offsetof(TechnologyVia,next_address)==0xd4);
struct TechnologyRecords {
    TechnologySubstrate* substrate = nullptr;
    TechnologyMetal* metals = nullptr;
    TechnologyVia* vias = nullptr;
};
struct TechnologyTables {
    TechnologySubstrate* substrate;int substrate_count;
    TechnologyMetal* metals;int metal_count;
    TechnologyVia* vias;int via_count;
};
struct TechnologyColors { const char* palette[17];int index; };
static_assert(__builtin_offsetof(TechnologyColors,index)==68);
struct TechnologyRuntime {
    ArrayAllocator scalar;
    SolverMemory arrays;
    WarningSink warnings,fatal;
    char* line;
    int (*format)(char*,const char*,...);
};
// Originals 0x0807dfc8/0x0807dea0/0x0807dd6c. Only sentinel fields and
// initial name/color bytes are written; all other bytes retain their history.
void initialize_technology_substrate(TechnologySubstrate& record);
void initialize_technology_metal(TechnologyMetal& record);
void initialize_technology_via(TechnologyVia& record);
// Originals 0x08056378/0x08056328/0x080562d8: scalar-new prepend, no init.
// A returning fatal callback leaves the native subsequent null store reachable.
void prepend_technology_substrate(TechnologySubstrate*& head,const TechnologyRuntime& runtime);
void prepend_technology_metal(TechnologyMetal*& head,const TechnologyRuntime& runtime);
void prepend_technology_via(TechnologyVia*& head,const TechnologyRuntime& runtime);
// Originals 0x0807e010/0x0807df34/0x0807de08. Floating copies execute x87
// loads/stores; metal resistance is multiplied by binary64 0.001 here.
void copy_technology_substrate(TechnologySubstrate*& head,const TechnologySubstrate& source,const TechnologyRuntime& runtime);
void copy_technology_metal(TechnologyMetal*& head,const TechnologyMetal& source,const TechnologyRuntime& runtime);
void copy_technology_via(TechnologyVia*& head,const TechnologyVia& source,const TechnologyRuntime& runtime);
// Originals 0x080562b0/0x0805628c/0x08056260. Head must be nonnull; the
// original dereferences it before the first delete and does not clear it.
void release_technology_substrate(TechnologySubstrate* head,SolverMemory memory);
void release_technology_metals(TechnologyMetal* head,SolverMemory memory);
void release_technology_vias(TechnologyVia* head,SolverMemory memory);
// Original 0x0807cfac. Allocate three banks and select first matching numeric
// index in each linked list. Missing records leave allocation bytes untouched.
// Via count <=0 retains its old bank. Allocation failures do not roll back.
bool build_technology_tables(TechnologyTables& tables,const TechnologyRecords& records,
    GreenIntegrationState& green,int& cached_eddy,const int* eddy_indices,int eddy_count,const TechnologyRuntime& runtime);
// Originals 0x0807c5a4/0x0807c804/0x0807cc1c. Return maximum index, or zero
// on error; an empty via list returns -1. The native palette has 17 entries,
// and its index overlaps the permitted eighteenth lookup: index 17 reads a
// string at address 17, increments index, and faults before the wrap check.
int validate_technology_substrate(TechnologySubstrate* head,const TechnologyRuntime& runtime);
int validate_technology_metals(TechnologyMetal* head,TechnologyColors& colors,const TechnologyRuntime& runtime);
int validate_technology_vias(TechnologyVia* head,TechnologyColors& colors,const TechnologyRuntime& runtime);
// Original 0x0807d270: reports invalid metal substrate indices, then still
// dereferences them. Every such index needs actual backing storage or faults.
bool validate_technology_assignments(const TechnologyTables& tables,const TechnologyRuntime& runtime);
}
