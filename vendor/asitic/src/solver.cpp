#include "asitic/solver.hpp"
#include "asitic/lapack.hpp"
#include "asitic/eddy.hpp"

namespace asitic {
namespace {
ComplexValue* complex_vector(int n,SolverMemory memory,WarningSink warnings) {
    auto* data=static_cast<ComplexValue*>(memory.arrays.get(static_cast<unsigned>(n)*16u));
    for(int i=0;i<n;++i)data[i]={0,0};
    if(!data)warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    return data;
}
ComplexMatrix matrix(int n,SolverMemory memory,WarningSink warnings) {
    return {complex_vector(n*n,memory,warnings),n*n,0,n,n,n,0};
}
void assign_zero(ComplexMatrix& out,int n,SolverMemory memory,WarningSink warnings) {
    const auto temporary=matrix(n,memory,warnings);
    if(out.matrix_reference==0) {
        if(out.vector_reference!=0)warnings.emit("MV_Vector::newsize can't operator on references.");
        else if(out.elements!=n*n) {
            memory.put(out.data);
            out.data=complex_vector(n*n,memory,warnings);
            if(out.data)out.elements=n*n;
        }
        out.rows=out.columns=out.stride=n;
    } else if(out.rows!=n || out.columns!=n) {
        warnings.emit("\nMV_ColMatRef::operator=  non-conformant assignment.");
        memory.put(temporary.data);
        return;
    }
    // The driver's source is an allocated, zero-filled square matrix.
    for(int j=0;j<n;++j)for(int i=0;i<n;++i)out.view().at(i,j)=temporary.view().at(i,j);
    memory.put(temporary.data);
}
void release_context(const InductanceContext& ctx,SolverMemory memory) {
    if(ctx.array_flags[0])memory.put(ctx.segments);
    if(ctx.array_flags[1])memory.put(ctx.extra_segments);
    if(ctx.array_flags[2])memory.put(ctx.width_counts);
    if(ctx.array_flags[3])memory.put(ctx.thickness_counts);
    if(ctx.array_flags[4])memory.put(ctx.via_indices);
}
void snapshot(const InductanceRunOptions& options,const char* filename,const ComplexMatrix& value) {
    if(options.snapshot)options.snapshot(filename,value,options.snapshot_user);
}
void timer_start(InductanceTimer& timer,const InductanceReporting& reporting) {
    if(reporting.time)reporting.time(timer.first,timer.zone,reporting.user);
}
void timer_stop(InductanceTimer& timer,const InductanceReporting& reporting) {
    if(reporting.time) {
        reporting.time(timer.last,timer.zone,reporting.user);
        timer.accumulated=static_cast<int>(static_cast<unsigned>(timer.accumulated)+static_cast<unsigned>(inductance_elapsed_milliseconds(timer)));
    }
}
void progress_dot(const InductanceReporting& reporting) { if(reporting.progress)reporting.status.emit("."); }
}
int inductance_elapsed_milliseconds(const InductanceTimer& timer) {
    const int seconds=static_cast<int>(static_cast<unsigned>(timer.last[0])-static_cast<unsigned>(timer.first[0]));
    const int micros=static_cast<int>(static_cast<unsigned>(timer.last[1])-static_cast<unsigned>(timer.first[1]));
    return static_cast<int>(static_cast<unsigned>(seconds)*1000u+static_cast<unsigned>(micros/1000));
}
bool invert_matrix_allocated(ComplexMatrix& value,bool mirror,SolverMemory memory,WarningSink warnings) {
    const int n=value.rows;
    auto* work=complex_vector(n,memory,warnings);
    auto* pivots=static_cast<int*>(memory.arrays.get(static_cast<unsigned>(n)*4u));
    if(!pivots)warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    const ComplexMatrixView dense{value.data,n};
    int info=lapack::factor_symmetric_upper_workspace(n,dense,pivots,work,n);
    bool result=false;
    if(info<0)warnings.emit("Error:  Illegal option passed to zsytrf.");
    else if(info>0)warnings.emit("Error:  Matrix factorization produced a singular matrix.");
    else {
        auto* inverse_work=complex_vector(n*2,memory,warnings);
        info=lapack::invert_symmetric_upper(n,dense,pivots,inverse_work);
        if(info<0)warnings.emit("Error:  Illegal option passed to zsytri.");
        else if(info>0)warnings.emit("Error:  Matrix factorization produced a singular matrix.");
        else {
            if(mirror)for(int j=0;j<n;++j)for(int i=0;i<j;++i)value.view().at(j,i)=value.view().at(i,j);
            result=true;
        }
        memory.put(inverse_work);
    }
    memory.put(pivots);memory.put(work);
    return result;
}
bool invert_general_allocated(ComplexMatrix& value,SolverMemory memory,lapack::GeneralInverseRuntime runtime) {
    const int n=value.rows;
    auto* pivots=static_cast<int*>(memory.arrays.get(static_cast<unsigned>(n)*4u));
    if(!pivots)runtime.warnings.emit("Error: NULL pointer in MV_Vector(int) constructor.\n       Most likely out of memory... ");
    const ComplexMatrixView dense{value.data,n};
    bool result=false;
    if(lapack::factor_general(n,n,dense,pivots)!=0) {
        if(runtime.cleanup)runtime.cleanup(runtime.user);
        runtime.warnings.emit("Error:  Error in matrix factorization.");
    } else {
        auto* work=complex_vector(n,memory,runtime.warnings);
        if(lapack::invert_general(n,dense,pivots,work,n)!=0)
            runtime.warnings.emit("Error:  Error in matrix inversion.");
        else result=true;
        memory.put(work);
    }
    memory.put(pivots);return result;
}
static bool solve_core(const ShapeFilamentList& shape,ComplexMatrix& output,double frequency,
    const InductanceRunOptions& options,const EddyRunOptions* eddy,SolverMemory memory,WarningSink warnings) {
    InductanceContext ctx{};ctx.shape=&shape;ctx.frequency=frequency;
    // The original driver takes the translation directly from the shape.
    const auto* bytes=reinterpret_cast<const unsigned char*>(&shape);
    ctx.origin[0]=*reinterpret_cast<const double*>(bytes+0x54);
    ctx.origin[1]=*reinterpret_cast<const double*>(bytes+0x5c);
    const auto& reporting=options.reporting;
    InductanceTimer total{},setup{},fill{},invert{},reduce{},eddy_timer{};
    timer_start(total,reporting);timer_start(setup,reporting);
    if(!setup_filament_groups(ctx,options.metal_count,memory.arrays,warnings)
       || !index_conductor_segments(ctx,options.metal_count,memory.arrays,warnings)
       || !set_filament_counts(ctx,options.filaments.edge_cell_size,options.thickness_cell,
                               options.filaments.max_width_cells,memory.arrays,warnings)) {
        release_context(ctx,memory);return false;
    }
    char local_message[256];char* message=reporting.buffer?reporting.buffer:local_message;
    if(reporting.progress && reporting.format) { reporting.format(message,"Generating inductance matrix (%dx%d).",ctx.filament_count,ctx.filament_count);warnings.emit(message); }
    auto full=matrix(ctx.filament_count,memory,warnings);
    timer_stop(setup,reporting);timer_start(fill,reporting);
    if(!fill_filament_impedance(full.view(),ctx,options.filaments,warnings)) {
        memory.put(full.data);release_context(ctx,memory);return false;
    }
    timer_stop(fill,reporting);timer_start(invert,reporting);
    snapshot(options,"Zbig.out",full);progress_dot(reporting);
    warnings.emit("Inverting matrix...");
    // The original driver ignores the inverse wrapper's return value.
    invert_matrix_allocated(full,false,memory,warnings);
    progress_dot(reporting);timer_stop(invert,reporting);timer_start(reduce,reporting);
    assign_zero(output,ctx.group_count,memory,warnings);
    reduce_filament_impedance(full.view(),output.view(),ctx);
    timer_stop(reduce,reporting);snapshot(options,"Ysmall.out",output);progress_dot(reporting);
    const bool enabled=eddy && eddy->mode!=0;
    if(options.impedance || ctx.via_count!=0 || enabled) {
        timer_start(invert,reporting);
        invert_matrix_allocated(output,true,memory,warnings);
        timer_stop(invert,reporting);timer_start(eddy_timer,reporting);
        if(enabled) {
            const int kind=*reinterpret_cast<const int*>(bytes+0x50);
            if(kind==0 && eddy->mode==1) {
                const SquareSpiralGeometry geometry{*reinterpret_cast<const double*>(bytes+0x70),
                    *reinterpret_cast<const double*>(bytes+0x88),*reinterpret_cast<const double*>(bytes+0x90),
                    *reinterpret_cast<const double*>(bytes+0x98)};
                const double height=eddy->layer_count==0?0:eddy->metal_heights[*reinterpret_cast<const int*>(bytes+0x68)];
                add_eddy_square(output.view(),geometry,frequency,{eddy->layer_count,kind,height,eddy->snapshot,eddy->snapshot_user},
                    eddy->environment,memory,warnings);
            } else fold_eddy_segments(shape,output.view(),output.rows,frequency,options.metal_count,eddy->environment);
        }
        timer_stop(eddy_timer,reporting);snapshot(options,"Zsmall_eddy.out",output);progress_dot(reporting);
        if(ctx.via_count) {
            timer_start(fill,reporting);
            auto** scratch=static_cast<const ConductorSegment**>(memory.arrays.get(4u*ctx.via_count));
            const bool ok=add_via_impedance(output.view(),ctx,options.vias,options.metal_count,scratch,warnings);
            if(!ok) { memory.put(full.data);release_context(ctx,memory);return false; }
            memory.put(scratch);
            timer_stop(fill,reporting);
        }
        if(!options.impedance) { timer_start(invert,reporting);invert_matrix_allocated(output,true,memory,warnings);timer_stop(invert,reporting); }
    }
    timer_stop(total,reporting);
    if(reporting.format) {
        reporting.format(message,"Ind Timing:  tot = %5.2ld, setup = %5.2ld, fill = %5.2ld\n           invert = %5.2ld, reduce = %5.2ld, eddy = %5.2ld",
            total.accumulated,setup.accumulated,fill.accumulated,invert.accumulated,reduce.accumulated,eddy_timer.accumulated);
        if(reporting.timing)warnings.emit(message);
    }
    memory.put(full.data);release_context(ctx,memory);
    return true;
}
bool solve_inductance_without_eddy(const ShapeFilamentList& shape,ComplexMatrix& output,double frequency,
    const InductanceRunOptions& options,SolverMemory memory,WarningSink warnings) {
    return solve_core(shape,output,frequency,options,nullptr,memory,warnings);
}
bool solve_inductance(const ShapeFilamentList& shape,ComplexMatrix& output,double frequency,
    const InductanceRunOptions& options,const EddyRunOptions& eddy,SolverMemory memory,WarningSink warnings) {
    return solve_core(shape,output,frequency,options,&eddy,memory,warnings);
}
}
