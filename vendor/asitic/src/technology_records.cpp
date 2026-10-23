#include "asitic/technology_records.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
template<class T>T* next(T* p) { return reinterpret_cast<T*>(p->next_address); }
void text(char* out,const char* in) { do { *out++=*in; }while(*in++); }
void floating_copy(double& out,const double& in) {
    asm volatile("fldl %1; fstpl %0" : "=m"(out) : "m"(in) : "st");
}
template<class T>void prepend(T*& head,const TechnologyRuntime& r,const char* message) {
    auto* node=static_cast<T*>(r.scalar.get(sizeof(T)));
    if(!node)r.fatal.emit(message);
    node->next_address=reinterpret_cast<unsigned>(head);head=node;
}
template<class T>void release(T* p,SolverMemory m) {
    do { auto* q=next(p);m.put(p);p=q; }while(p);
}
void color(char* out,TechnologyColors& colors) {
    const auto* source=colors.index==17?reinterpret_cast<const char*>(17u):colors.palette[colors.index];
    ++colors.index;text(out,source);if(colors.index>17)colors.index=0;
}
template<class... A>void warning(const TechnologyRuntime& r,const char* format,A... args) {
    r.format(r.line,format,args...);r.warnings.emit(r.line);
}
}
void initialize_technology_substrate(TechnologySubstrate& s) {
    s.index=-271;s.permittivity=s.resistivity=s.height=s.thickness=-271.;
}
void initialize_technology_metal(TechnologyMetal& m) {
    m.name[0]=m.color[0]=0;m.substrate=m.index=-271;
    m.z=m.thickness=m.resistance=m.minimum_spacing=m.minimum_width=-271.;
}
void initialize_technology_via(TechnologyVia& v) {
    v.name[0]=v.color[0]=0;v.index=v.bottom=v.top=-271;
    v.width=v.spacing=v.overplot1=v.overplot2=v.resistance=-271.;
}
void prepend_technology_substrate(TechnologySubstrate*& p,const TechnologyRuntime& r) { prepend(p,r,"502:  Cannot allocate memory"); }
void prepend_technology_metal(TechnologyMetal*& p,const TechnologyRuntime& r) { prepend(p,r,"503:  Cannot allocate memory"); }
void prepend_technology_via(TechnologyVia*& p,const TechnologyRuntime& r) { prepend(p,r,"504:  Cannot allocate memory"); }
void copy_technology_substrate(TechnologySubstrate*& p,const TechnologySubstrate& s,const TechnologyRuntime& r) {
    prepend_technology_substrate(p,r);p->index=s.index;floating_copy(p->permittivity,s.permittivity);
    floating_copy(p->resistivity,s.resistivity);floating_copy(p->height,s.height);floating_copy(p->thickness,s.thickness);
}
void copy_technology_metal(TechnologyMetal*& p,const TechnologyMetal& s,const TechnologyRuntime& r) {
    prepend_technology_metal(p,r);text(p->name,s.name);text(p->color,s.color);
    p->substrate=s.substrate;p->index=s.index;floating_copy(p->z,s.z);floating_copy(p->thickness,s.thickness);
    p->resistance=x87::binary64(0x1.0624dd2f1a9fcp-10L*s.resistance);
    floating_copy(p->minimum_spacing,s.minimum_spacing);floating_copy(p->minimum_width,s.minimum_width);
}
void copy_technology_via(TechnologyVia*& p,const TechnologyVia& s,const TechnologyRuntime& r) {
    prepend_technology_via(p,r);text(p->name,s.name);text(p->color,s.color);p->index=s.index;p->bottom=s.bottom;p->top=s.top;
    floating_copy(p->width,s.width);floating_copy(p->spacing,s.spacing);floating_copy(p->overplot1,s.overplot1);
    floating_copy(p->overplot2,s.overplot2);floating_copy(p->resistance,s.resistance);
}
void release_technology_substrate(TechnologySubstrate* p,SolverMemory m) { release(p,m); }
void release_technology_metals(TechnologyMetal* p,SolverMemory m) { release(p,m); }
void release_technology_vias(TechnologyVia* p,SolverMemory m) { release(p,m); }
bool build_technology_tables(TechnologyTables& t,const TechnologyRecords& records,GreenIntegrationState& green,
    int& cached,const int* indices,int count,const TechnologyRuntime& r) {
    t.substrate=static_cast<TechnologySubstrate*>(r.arrays.arrays.get(static_cast<unsigned>(t.substrate_count)*40u));
    t.metals=static_cast<TechnologyMetal*>(r.arrays.arrays.get(static_cast<unsigned>(t.metal_count)*236u));
    if(t.via_count>0)t.vias=static_cast<TechnologyVia*>(r.arrays.arrays.get(static_cast<unsigned>(t.via_count)*240u));
    if((t.via_count>0 && !t.vias) || !t.substrate || !t.metals) { r.warnings.emit("Could not allocate memory.");return false; }
    for(int i=0;i<t.substrate_count;++i) {
        auto* s=records.substrate;while(s && s->index!=i)s=next(s);
        if(s) { auto& d=t.substrate[i];d.index=s->index;floating_copy(d.permittivity,s->permittivity);floating_copy(d.resistivity,s->resistivity);
            floating_copy(d.height,s->thickness);floating_copy(d.thickness,s->thickness); }
    }
    for(int i=0;i<t.metal_count;++i) {
        auto* s=records.metals;while(s && s->index!=i)s=next(s);
        if(s) { auto& d=t.metals[i];d.substrate=s->substrate;d.index=s->index;floating_copy(d.z,s->z);floating_copy(d.thickness,s->thickness);
            floating_copy(d.resistance,s->resistance);floating_copy(d.minimum_spacing,s->minimum_spacing);floating_copy(d.minimum_width,s->minimum_width);
            text(d.name,s->name);text(d.color,s->color); }
    }
    for(int i=0;i<t.via_count;++i) {
        auto* s=records.vias;while(s && s->index!=i)s=next(s);
        if(s) { auto& d=t.vias[i];d.bottom=s->bottom;d.top=s->top;floating_copy(d.overplot1,s->overplot1);floating_copy(d.overplot2,s->overplot2);
            floating_copy(d.width,s->width);floating_copy(d.spacing,s->spacing);floating_copy(d.resistance,s->resistance);text(d.name,s->name);text(d.color,s->color); }
    }
    cached=0;
    if(count) {
        for(int i=0;i<count;++i)if(indices[i]>=t.substrate_count) { warning(r,"Error:  Eddy layer %d does not exist.",indices[i]);return true; }
        configure_green_layers(green,cached,reinterpret_cast<const SubstrateLayer*>(t.substrate),t.substrate_count,indices,count,r.warnings);
    }
    return true;
}
int validate_technology_substrate(TechnologySubstrate* head,const TechnologyRuntime& r) {
    int maximum=-1;bool error=false;
    for(auto* p=head;p;p=next(p)) {
        if(p->index>maximum)maximum=p->index;
        if(p->permittivity==-271.) {
            warning(r,"Warning:  Did not define eps for layer <%d>.",p->index);r.warnings.emit("          Assuming eps = 11.7 (Si)");p->permittivity=11.7;
        }
        if(p->permittivity<=0.) { warning(r,"Error:  Eps for layer <%d> must be positive.",p->index);error=true; }
        if(p->resistivity==-271.) { warning(r,"Error:  Conductivity rho of layer <%d> not specified.",p->index);error=true; }
        if(p->resistivity<=0.) { warning(r,"Error:  Layer <%d> has non-positive conductivity.",p->index);error=true; }
        if(p->thickness==-271.) { warning(r,"Error:  Thickness of layer <%d> not specified.",p->index);error=true; }
        if(p->thickness<=0.) { warning(r,"Error:  Layer <%d> must have a positive thickness.",p->index);error=true; }
    }
    if(maximum==-1) { r.warnings.emit("Error:  No Layers defined.");error=true; }
    for(int i=0;i<=maximum;++i) {
        auto* p=head;while(p && p->index!=i)p=next(p);
        if(!p) { warning(r,"Did Not Define Layer <%d>.",i);error=true; }
    }
    return error?0:maximum;
}
int validate_technology_metals(TechnologyMetal* head,TechnologyColors& colors,const TechnologyRuntime& r) {
    int maximum=-1;bool error=false;
    for(auto* p=head;p;p=next(p)) {
        if(p->index>maximum)maximum=p->index;
        if(!p->name[0]) { warning(r,"Warning:  No name given to metal layer %d.",p->index);r.format(p->name,"M%d",p->index);
            warning(r,"          Using default name of %s.",p->name); }
        if(!p->color[0]) { warning(r,"Warning:  No color given to metal layer %s.",p->name);color(p->color,colors);
            warning(r,"Using default color of %s.",p->color); }
        if(p->substrate==-271) { p->substrate=1;warning(r,"Error:  Metal layer %s does not reside in a substrate layer.",p->name);error=true; }
        if(p->z==-271.) { p->z=0;warning(r,"Error:  Did not specify z-coordinate of metal layer %s.",p->name);error=true; }
        if(p->thickness==-271.) { p->thickness=0;warning(r,"Error:  Did not specify thickness of metal layer %s.",p->name);error=true; }
        // Pin the original binary64 sentinel; a decimal literal retains extra
        // precision under the required x87 compiler mode.
        if(p->resistance==-0x1.15810624dd2f2p-2L) { warning(r,"Error:  Conductivity of metal layer %s not specified.",p->name);error=true; }
        if(p->resistance<=0.) { warning(r,"Error:  Cannot assign non-positive conductance to metal layer %s",p->name);error=true; }
        if(p->minimum_spacing==-271.)p->minimum_spacing=0;
        if(p->minimum_spacing<0.) { r.format(r.line,"Warning:  Minimum metal spacing of metal layer %s must be non-negative.",p->name);p->minimum_spacing=0.1; }
        if(p->minimum_width==-271.)p->minimum_width=0;
        if(p->minimum_width<0.) { r.format(r.line,"Warning:  Minimum metal width of metal layer %s must be non-negative.",p->name);p->minimum_width=0.1; }
    }
    if(maximum==-1) { r.warnings.emit("Error:  No metal layers defined.");error=true; }
    for(int i=0;i<=maximum;++i) { auto* p=head;while(p && p->index!=i)p=next(p);
        if(!p) { warning(r,"Did Not Define Metal Layer <%d>.",i);error=true; } }
    return error?0:maximum;
}
int validate_technology_vias(TechnologyVia* head,TechnologyColors& colors,const TechnologyRuntime& r) {
    int maximum=-1;bool error=false;
    for(auto* p=head;p;p=next(p)) {
        if(p->index>maximum)maximum=p->index;
        if(!p->name[0]) { warning(r,"Warning:  No namen given to via layer %d.",p->index);r.format(p->name,"ViaM%d-M%d",p->bottom,p->top);
            warning(r,"Using default name of %s.",p->name); }
        if(!p->color[0]) { warning(r,"Warning:  No color given to via %s.",p->name);color(p->color,colors);warning(r,"Using default color of %s.",p->color); }
        if(p->resistance==-271.) { warning(r,"Warning:  Resistance of via %s not specified.",p->name);r.warnings.emit("Assuming a zero-resistance via.");p->resistance=0; }
        if(p->resistance<0.) { warning(r,"Error:  Cannot assign negative conductance to via layer %s",p->name);error=true; }
        if(p->overplot1==-271. || p->overplot2==-271. || p->width==-271.) {
            warning(r,"Error:  Did not specify width of via or minimum overplot of via %s.",p->name);error=true;
        }
        if(p->width<=0.) { warning(r,"Error:  Via %s must have a positive width.",p->name);error=true; }
        if(p->overplot1<=0. || p->overplot2<=0.) { warning(r,"Error:  Via %s must have a positive minimum overplot distance.",p->name);error=true; }
        if(p->spacing<=0.) { warning(r,"Error:  Via %s must have a positive spacing.",p->name);error=true; }
    }
    for(int i=0;i<=maximum;++i) { auto* p=head;while(p && p->index!=i)p=next(p);
        if(!p) { warning(r,"Did Not Define Via Layer <%d>.",i);error=true; } }
    return error?0:maximum;
}
bool validate_technology_assignments(const TechnologyTables& t,const TechnologyRuntime& r) {
    bool error=false;
    for(int i=0;i<t.metal_count;++i) {
        const auto& m=t.metals[i];
        if(m.substrate==0) { warning(r,"Error:  Metal Layer %s in first layer.",m.name);
            r.warnings.emit("Metal Layers Cannot be in the first layer.  To circumvent this, split the first layer into two layers.");error=true; }
        if(m.substrate>=t.substrate_count || m.substrate<0) { warning(r,"Error:  Metal Layer %s in undefined layer <%d>.",m.name,m.substrate);error=true; }
        const auto& s=t.substrate[m.substrate];
        if(s.height<m.z) { warning(r,"Error:  z-coordinate for Metal Layer %s places layer outside substrate layer <%d>.",m.name,m.substrate);error=true; }
        if(s.height<m.thickness) { warning(r,"Error:  Thickness for Metal Layer %s places contact outside substrate layer <%d>.",m.name,m.substrate);error=true; }
        if(m.z<0. || m.thickness<0.) { warning(r,"Error:  Negative Dimension(s) for Metal Layer %s Thickness or Z-Coordinate.",m.name);error=true; }
    }
    for(int i=0;i<t.via_count;++i) {
        const auto& v=t.vias[i];
        if(v.top<v.bottom) { warning(r,"Error:  Via %s Bottom is above Via Top",v.name);error=true; }
        if(v.top==v.bottom) { r.format(r.line,"Error:  Via %s Bottom is the same as Via Top",v.name);error=true; }
        if(v.top<0 || v.top>=t.metal_count) { warning(r,"Error:  Via %s top is connected to undefined metal layer <%d>.",v.name,v.top);error=true; }
        if(v.bottom<0 || v.bottom>=t.metal_count) { warning(r,"Error:  Via %s  bottom is connected to undefined metal layer <%d>.",v.name,v.top);error=true; }
    }
    return !error;
}
}
