#pragma once
#include "asitic/potential_cache.hpp"
#include "asitic/contacts.hpp"

namespace asitic {
struct PotentialCapacitanceState {
    PotentialCache cache;
    PotentialContactGroup* groups = nullptr;
    ComplexValue* coefficients;
};
struct PotentialCapacitanceOptions {
    PotentialContactOptions contacts;
    PotentialCacheTechnology technology;
    double* depths;
    PotentialCacheRuntime runtime;
    bool progress = false;
    bool verbose = false;
    void (*snapshot)(const char*,const ComplexMatrix&,void*) = nullptr;
    void* snapshot_user = nullptr;
    PotentialContactHistory contact_history;
    PotentialCacheHeaderHistory cache_header_history = {};
};
// Original 0x08052c50: native contact meshing, substrate coefficients, cache
// load/generation, potential fill/inversion, group reduction, terminal folding
// and final principal-matrix assignment. Cache banks are already allocated.
// Incoming groups are preserved by setup and cleared by normal/error cleanup.
// The original ignores inverse failures and continues after cache I/O errors.
bool solve_capacitance(const ShapeFilamentList& first,const ShapeFilamentList& second,
    ComplexMatrix& output,double frequency,PotentialCapacitanceState& state,
    const PotentialCapacitanceOptions& options,SolverMemory memory);
// Original 0x08092780: same contact/cache/inverse/group reduction, returning
// the leading groups-minus-second-count principal matrix directly. No extra
// terminal-folding matrix or verbose total-capacitance report is constructed.
bool solve_unfolded_capacitance(const ShapeFilamentList& first,const ShapeFilamentList& second,
    ComplexMatrix& output,double frequency,PotentialCapacitanceState& state,
    const PotentialCapacitanceOptions& options,SolverMemory memory);
}
