#pragma once
#include "asitic/geometry.hpp"
#include "asitic/eddy.hpp"
#include "asitic/technology_records.hpp"
namespace asitic {
struct AnalysisReportRuntime {
    char* message;
    int (*format)(char*,const char*,...);
    WarningSink warnings;
    void* (*open)(const char*,const char*,void*);
    int (*file_format)(void*,const char*,...);
    int (*close)(void*,void*);
    void* user;
};
struct FrequencyGeometryLimits {
    double max_length,max_width,max_thickness,capacitance_max_length,capacitance_max_width;
};
// Original 0x080605ec, mislabeled format_complex_pi_print in the decompilation.
// Uses metals[metal_count-1], with the native backed-index requirement.
// The five fields map to globals580,588,590,598,5a0 respectively.
void update_frequency_geometry(double frequency,const TechnologyMetal* metals,int metal_count,
    double alpha,double beta,FrequencyGeometryLimits& limits,bool print,const AnalysisReportRuntime& runtime);
struct InductanceMatrixReportOptions {
    GeometryDiscretization discretization;
    InductanceRunOptions inductance;
    EddyRunOptions eddy;
    GeometryRuntime geometry;
    AnalysisReportRuntime report;
};
// Original0x08066214. Clone/discretize, solve with impedance=true, remove the
// clone, write optional raw matrix/row sums, report total L/R, release arrays.
// The source shape is unchanged. The shape-list head follows native removal.
void report_inductance_matrix(const GeometryShape& source,double frequency,const char* filename,
    GeometryShape*& shapes,const InductanceMatrixReportOptions& options);
}
