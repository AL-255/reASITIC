#include "asitic/impedance.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
const ConductorSegment* next(const ConductorSegment* segment) {
    return reinterpret_cast<const ConductorSegment*>(segment->next_address);
}
const ConductorSegment* branch(const ConductorSegment* segment) {
    return reinterpret_cast<const ConductorSegment*>(segment->group_next_address);
}
bool out_of_memory(WarningSink warnings) { warnings.emit("Error:  Out of memory.");return false; }
}
bool setup_filament_groups(InductanceContext& ctx,int metal_count,ArrayAllocator allocator,WarningSink warnings) {
    int groups=0,vias=0;
    for(auto* s=ctx.shape->first;s;s=next(s)) {
        if(s->metal<metal_count)++groups;else ++vias;
    }
    ctx.group_count=groups;ctx.via_count=vias;
    auto* extra=static_cast<int*>(allocator.get(4u*groups));ctx.extra_segments=extra;
    if(!extra)return out_of_memory(warnings);
    ctx.array_flags[1]=1;
    int* indices=nullptr;
    if(vias) {
        indices=static_cast<int*>(allocator.get(4u*vias));ctx.via_indices=indices;
        if(!indices)return out_of_memory(warnings);
        ctx.array_flags[4]=1;
    }
    int group=0,via=0;
    for(auto* s=ctx.shape->first;s;s=next(s)) {
        if(s->metal<metal_count) {
            extra[group]=0;
            for(auto* b=branch(s);b;b=branch(b))++extra[group];
            ++group;
        } else indices[via++]=group;
    }
    unsigned total=0;
    for(int i=0;i<groups;++i)total+=static_cast<unsigned>(extra[i])+1u;
    ctx.segment_count=static_cast<int>(total);
    return true;
}
bool index_conductor_segments(InductanceContext& ctx,int metal_count,ArrayAllocator allocator,WarningSink warnings) {
    auto** segments=static_cast<const ConductorSegment**>(allocator.get(4u*ctx.segment_count));ctx.segments=segments;
    if(!segments)return out_of_memory(warnings);
    ctx.array_flags[0]=1;
    int i=0;
    for(auto* s=ctx.shape->first;s;s=next(s))if(s->metal<metal_count) {
        segments[i++]=s;
        for(auto* b=branch(s);b;b=branch(b))segments[i++]=b;
    }
    return true;
}
bool set_filament_counts(InductanceContext& ctx,double width_cell,double thickness_cell,
                          int max_width_cells,ArrayAllocator allocator,WarningSink warnings) {
    auto* widths=static_cast<int*>(allocator.get(4u*ctx.segment_count));ctx.width_counts=widths;
    auto* thicknesses=static_cast<int*>(allocator.get(4u*ctx.segment_count));ctx.thickness_counts=thicknesses;
    if(!widths || !thicknesses)return out_of_memory(warnings);
    ctx.array_flags[2]=ctx.array_flags[3]=1;
    unsigned total=0;
    for(int i=0;i<ctx.segment_count;++i) {
        const auto& s=*ctx.segments[i];
        int width=x87::truncate_int((long double)s.width/width_cell+0.5L);
        int thickness=x87::truncate_int((long double)s.thickness/thickness_cell+0.5L);
        if(width<1)width=1;
        if(thickness<1)thickness=1;
        if(width>max_width_cells)width=max_width_cells;
        if(s.width_count==-1 && s.thickness_count==-1)width=thickness=3;
        widths[i]=width;thicknesses[i]=thickness;
        total+=static_cast<unsigned>(width)*static_cast<unsigned>(thickness);
    }
    ctx.filament_count=static_cast<int>(total);
    return true;
}
}
