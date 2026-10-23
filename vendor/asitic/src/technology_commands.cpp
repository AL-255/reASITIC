#include "asitic/technology_commands.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1":"=t"(out):"m"(value));return out; }
long double argument(const CommandArgument& arg) { float f;__builtin_memcpy(&f,&arg.value,4);long double out;asm volatile("flds %1":"=t"(out):"m"(f));return out; }
unsigned word(const double& value,unsigned index) { unsigned out;__builtin_memcpy(&out,reinterpret_cast<const char*>(&value)+4*index,4);return out; }
bool prefix(const char* s,const char* p) { while(*p)if(*s++!=*p++)return false;return true; }
void refresh_green(TechnologyInitialization& state,const TechnologyRuntime& r) {
    for(int i=0;i<state.input.eddy_count;++i)if(state.input.eddy_indices[i]>=state.tables.substrate_count) {
        r.format(r.line,"Error:  Eddy layer %d does not exist.",state.input.eddy_indices[i]);r.warnings.emit(r.line);state.cached_eddy=0;return;
    }
    configure_green_layers(state.green,state.cached_eddy,reinterpret_cast<const SubstrateLayer*>(state.tables.substrate),
        state.tables.substrate_count,state.input.eddy_indices,state.input.eddy_count);
}
}
void print_technology(const TechnologyInitialization& s,const TechnologyRuntime& r) {
    r.format(r.line,"\nTechnolog File Data <%s> @ <%s>",s.input.name,s.input.directory);r.warnings.emit(r.line);
    r.format(r.line,"\nMetal Layers:");r.warnings.emit(r.line);
    for(int i=0;i<s.tables.metal_count;++i) { const auto& m=s.tables.metals[i];
        r.format(r.line,"Name=%-10s\tMet=%3d\tLyr=%3d\tMz=%5.4lf\tT=%5.4lf\tRS=%4.2lf\tCol=%-10s",m.name,i,m.substrate,
            word(m.z,0),word(m.z,1),word(m.thickness,0),word(m.thickness,1),word(m.resistance,0),word(m.resistance,1),m.color);r.warnings.emit(r.line);
    }
    r.format(r.line,"\nSubstrate Layers:");r.warnings.emit(r.line);
    for(int i=0;i<s.tables.substrate_count;++i) { const auto& t=s.tables.substrate[i];
        r.format(r.line,"Lyr=%d\tEps=%4.2lg\tRho=%5.4lg\tDZ=%5.4lg\tT=%4.3lg",i,
            word(t.permittivity,0),word(t.permittivity,1),word(t.resistivity,0),word(t.resistivity,1),word(t.height,0),word(t.height,1),word(t.thickness,0),word(t.thickness,1));r.warnings.emit(r.line);
    }
    r.format(r.line,"\nVia Layers:");r.warnings.emit(r.line);
    for(int i=0;i<s.tables.via_count;++i) { const auto& v=s.tables.vias[i];
        r.format(r.line,"Name=%-10s\tMetTop=%d\tMetBot=%d\tR=%4.2lg\twid=%4.2lg\tspace=%4.2lg\toverplot1=%4.2lg\toverplot2=%4.2lg\tcolor=%s",v.name,v.top,v.bottom,
            word(v.resistance,0),word(v.resistance,1),word(v.width,0),word(v.width,1),word(v.spacing,0),word(v.spacing,1),word(v.overplot1,0),word(v.overplot1,1),word(v.overplot2,0),word(v.overplot2,1),v.color);r.warnings.emit(r.line);
    }
}
bool execute_technology_command(int id,const CommandArguments& a,TechnologyInitialization& s,TechnologyCommandSettings& c,const TechnologyRuntime& r) {
    const int count=static_cast<int>(a.command.value);
    switch(id) {
    case 108:case 112: {
        const bool snap=id==112;
        if(count==0) {
            auto& enabled=snap?c.snap_enabled:c.grid_enabled;
            if(snap) { if(enabled)r.warnings.emit("Snap to grid off.");else { r.format(r.line,"Snap to grid size = %5.2lf",word(c.snap_size,0),word(c.snap_size,1));r.warnings.emit(r.line); } }
            enabled=enabled?0:1;return true;
        }
        if(snap)c.snap_enabled=1;
        const auto value=argument(a.arguments[0]);const auto width=load(s.input.width),height=load(s.input.height);
        // Native selects width only on an ordered height > width comparison.
        const auto minimum=height>width?width:height;
        if(value>0 && minimum>value)(snap?c.snap_size:c.grid_size)=x87::binary64(value);
        else r.warnings.emit(snap?"Invalid snap grid size ignored.":"Invalid grid size ignored.");
        return true;
    }
    case 207:
        if(count<1) { r.format(r.line,"Current cell size:\nMax Length = %5.2lf\tMax Width = %5.2lf\tMax Thickness = %5.2lf",word(s.half_width,0),word(s.half_width,1),word(s.half_height,0),word(s.half_height,1),word(s.thickness_cell,0),word(s.thickness_cell,1));r.warnings.emit(r.line); }
        if(count>0)s.half_width=x87::binary64(argument(a.arguments[0]));
        if(count>1)s.half_height=x87::binary64(argument(a.arguments[1]));
        if(count==3)s.thickness_cell=x87::binary64(argument(a.arguments[2]));
        c.automatic_cells=0;return true;
    case 211: {
        if(count<1) { r.format(r.line,"Current cap cell size:\nMax Length = %5.2lf\tMax Width = %5.2lf",word(s.length_step,0),word(s.length_step,1),word(s.width_step,0),word(s.width_step,1));r.warnings.emit(r.line); }
        if(count>0)s.length_step=x87::binary64(argument(a.arguments[0]));
        if(count>1)s.width_step=x87::binary64(argument(a.arguments[1]));
        for(unsigned i=0;i<2;++i) { auto& cell=i?s.width_step:s.length_step;
            const auto x=load(s.input.width)/s.input.fft_x,y=load(s.input.height)/s.input.fft_y;
            if(load(cell)<x || load(cell)<y) {
                const double minimum=x87::binary64(y<x?x:y);
                r.format(r.line,i?"Cell width cannot be smaller than bchip_dim/FFTSize = %lf.  You should increase FFT size.":"Cell length cannot be smaller than chip_dim/FFTSize = %lf.  You should increase FFT size.",word(minimum,0),word(minimum,1));r.warnings.emit(r.line);
                const auto nx=load(s.input.width)/s.input.fft_x,ny=load(s.input.height)/s.input.fft_y;cell=x87::binary64(ny<nx?nx:ny);
            }
        }c.automatic_cells=0;return true;
    }
    case 212:
        r.warnings.emit("Entering Auto-Cellsize Generation Mode.");
        if(count>0)c.auto_alpha=x87::binary64(argument(a.arguments[0]));
        if(count>1)c.auto_beta=x87::binary64(argument(a.arguments[1]));
        c.automatic_cells=1;return true;
    case 214:print_technology(s,r);return true;
    case 222: {
        const int layer=static_cast<int>(a.arguments[1].value);
        if(layer<0 || layer>=s.tables.substrate_count) { r.warnings.emit("Error:  Invalid layer number specified.");return true; }
        auto& t=s.tables.substrate[layer];
        const auto* name=a.arguments[0].text;
        if(prefix(name,"RHO")) {
            const auto value=argument(a.arguments[2]);
            if(!(value>0 && value<1000000000000000.L)) { r.warnings.emit("Error:  Invalid resistivity ignored.");return true; }
            t.resistivity=x87::binary64(value*10000.L);refresh_green(s,r);c.dirty=1;
        } else if(prefix(name,"T")) {
            const auto old=layer==s.tables.substrate_count-1?load(t.height):load(t.height)-load(s.tables.substrate[layer+1].height);
            const auto value=argument(a.arguments[2]);const auto delta=value-old;
            if(!(value>0 && value<1000.L)) { r.warnings.emit("Error:  Invalid layer thickness ignored.");return true; }
            t.thickness=x87::binary64(value);
            for(int i=layer;i>=0;--i)s.tables.substrate[i].height=x87::binary64(load(s.tables.substrate[i].height)+delta);
            for(int i=0;i<s.tables.metal_count;++i)if(s.tables.metals[i].substrate<=layer)s.tables.metals[i].z=x87::binary64(load(s.tables.metals[i].z)+delta);
            for(int i=0;i<s.tables.substrate_count;++i)s.depths[i]=x87::binary64(load(s.tables.substrate[i].height));
            s.depths[s.tables.substrate_count]=0.;
            for(int i=0;i<s.tables.metal_count;++i)s.metal_heights[i]=x87::binary64(load(s.tables.substrate[s.tables.metals[i].substrate].height)-load(s.tables.metals[i].z));
            refresh_green(s,r);c.dirty=1;
        } else if(prefix(name,"EPS")) {
            const auto value=argument(a.arguments[2]);
            if(value>1 && value<100) { t.permittivity=x87::binary64(value);c.dirty=1; }
            else r.warnings.emit("Error:  Invalid layer thickness ignored.");
        }return true;
    }
    case 223:
        if(count==1)c.technology_io=static_cast<unsigned char>(a.arguments[0].value);else c.technology_io^=1;
        r.warnings.emit(c.technology_io?"Turning ON TechFile IO.":"Turning OFF TechFile IO.");return true;
    default:return false;
    }
}
}
