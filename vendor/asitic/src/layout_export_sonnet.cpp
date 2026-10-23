#include "asitic/layout_export_sonnet.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double load(const double& value) { long double out;asm volatile("fldl %1":"=t"(out):"m"(value));return out; }
long double subtract(long double first,const double& second) { long double out;asm("fsubl %2":"=t"(out):"0"(first),"m"(second));return out; }
unsigned word(const double& value,unsigned index) { unsigned result;__builtin_memcpy(&result,reinterpret_cast<const char*>(&value)+4*index,4);return result; }
const ConductorSegment* next(const ConductorSegment* segment) { return reinterpret_cast<const ConductorSegment*>(segment->next_address); }
void point(void* file,const double& x,const double& y,double offset_x,double offset_y,const SonnetExportRuntime& runtime) {
    const double px=x87::binary64(subtract(load(x),offset_x)),py=x87::binary64(subtract(load(offset_y),y));
    runtime.print(file,"\n%-8.5lf %-8.5lf",word(px,0),word(px,1),word(py,0),word(py,1));
}
void material(void* file,const TechnologySubstrate& layer,const SonnetExportRuntime& r) {
    const double thickness=x87::binary64(load(layer.thickness));const auto rho=load(layer.resistivity);
    if(10000000000.L>rho) { const double conductivity=x87::binary64(1000000.L/rho);
        r.print(file,"\n%lf\t%lf\t1.0\t0\t0\t%lf",word(thickness,0),word(thickness,1),word(layer.permittivity,0),word(layer.permittivity,1),word(conductivity,0),word(conductivity,1));
    }else r.print(file,"\n%lf\t%lf\t1.0\t0\t0",word(thickness,0),word(thickness,1),word(layer.permittivity,0),word(layer.permittivity,1));
}
}
void export_sonnet(const GeometryShape* shapes,const TechnologyTables& technology,double chip_width,double chip_height,
    SonnetExportHistory& history,const SonnetExportRuntime& runtime) {
    char line[80];runtime.prompt("Enter filename:  ",line,0,runtime.user);
    void* file=runtime.open(line,"w",runtime.user);
    if(!file) { runtime.warnings.emit("Error:  Could not open file.");return; }
    do {
        runtime.prompt("Cell Size In Microns (x y):  ",line,0,runtime.user);
        runtime.scan(line,"%lf %lf",&history.cell_x,&history.cell_y);
    }while(load(history.cell_x)<0 || load(history.cell_y)<0 || load(history.cell_x)>load(chip_width) || load(history.cell_y)>load(chip_height));
    int polygons=0;
    for(auto* shape=shapes;shape;shape=shape->next)
        for(auto* segment=shapes->filaments.first;segment;segment=next(segment))if(segment->metal<technology.metal_count)++polygons;
    runtime.print(file,"\nVER 4.0a");runtime.print(file,"\nLEN\tuM\t1.0e-6");
    const int active=technology.metals[technology.metal_count-1].substrate;int levels=0;
    for(int i=0;i<technology.metal_count;++i) {
        const auto& metal=technology.metals[i];if(metal.substrate==active)++levels;
        runtime.print(file,"\nRES\t%lf\t%lf",word(metal.resistance,0),word(metal.resistance,1),0u,0u);
        runtime.print(file,"\nREN\t%d\t%s",i,metal.name);
    }
    levels+=technology.substrate_count;
    const int cells_y=x87::truncate_int(load(chip_height)/load(history.cell_y)),cells_x=x87::truncate_int(load(chip_width)/load(history.cell_x));
    const double width=x87::binary64(load(chip_width)),height=x87::binary64(load(chip_height));
    runtime.print(file,"\nBOX %d %lf %lf %d %d 20",levels-2,word(width,0),word(width,1),word(height,0),word(height,1),cells_x,cells_y);
    if(active!=technology.metal_count-1)for(int i=technology.substrate_count-1;i>active;--i)material(file,technology.substrate[i],runtime);
    for(int i=technology.metal_count-1;i>0;--i)if(technology.metals[i].substrate==active) {
        const auto& metal=technology.metals[i];const auto& previous=technology.metals[i-1];
        const double gap=x87::binary64(previous.substrate==active?subtract(load(previous.z),metal.z):load(metal.z));
        const auto& layer=technology.substrate[active];const double conductivity=x87::binary64(1000000.L/load(layer.resistivity));
        runtime.print(file,"\n%lf\t%lf\t1.0\t0\t0\t%lf",word(gap,0),word(gap,1),word(layer.permittivity,0),word(layer.permittivity,1),word(conductivity,0),word(conductivity,1));
    }
    for(int i=active-1;i>=0;--i)material(file,technology.substrate[i],runtime);
    runtime.print(file,"\nNUM\t%d",polygons);
    double offset_x=0.,offset_y=x87::binary64(load(chip_height));
    for(auto* shape=shapes;shape;shape=shape->next) {
        offset_x=x87::binary64(subtract(load(offset_x),*reinterpret_cast<const double*>(shape->filaments.prefix+0x54)));offset_y=x87::binary64(subtract(load(offset_y),*reinterpret_cast<const double*>(shape->filaments.prefix+0x5c)));
        for(auto* segment=shape->filaments.first;segment;segment=next(segment))if(segment->metal<technology.metal_count) {
            runtime.print(file,"\n%-5d%-5d%-5d T 0 1 1 100 100",technology.metal_count-segment->metal-1,5,segment->metal);
            const auto& os=segment->outer_start;const auto& oe=segment->outer_end;const auto& is=segment->inner_start;const auto& ie=segment->inner_end;
            if(*reinterpret_cast<const int*>(shape->filaments.prefix+0x50)!=0) {
                point(file,os[0],os[1],offset_x,offset_y,runtime);point(file,oe[0],oe[1],offset_x,offset_y,runtime);point(file,ie[0],ie[1],offset_x,offset_y,runtime);point(file,is[0],is[1],offset_x,offset_y,runtime);point(file,os[0],os[1],offset_x,offset_y,runtime);
            }else {
                if(load(oe[0])>load(os[0]) && x87::abs(subtract(load(oe[1]),os[1]))<0x1.b7cdfd9d7bdbbp-34L) {
                    point(file,os[0],os[1],offset_x,offset_y,runtime);point(file,ie[0],oe[1],offset_x,offset_y,runtime);point(file,ie[0],ie[1],offset_x,offset_y,runtime);point(file,os[0],is[1],offset_x,offset_y,runtime);point(file,os[0],os[1],offset_x,offset_y,runtime);
                }else if(load(oe[1])<load(os[1]) && x87::abs(subtract(load(os[0]),oe[0]))<0x1.b7cdfd9d7bdbbp-34L) {
                    point(file,os[0],os[1],offset_x,offset_y,runtime);point(file,oe[0],ie[1],offset_x,offset_y,runtime);point(file,ie[0],ie[1],offset_x,offset_y,runtime);point(file,is[0],os[1],offset_x,offset_y,runtime);point(file,os[0],os[1],offset_x,offset_y,runtime);
                }
                if(load(oe[0])<load(os[0]) && x87::abs(subtract(load(oe[1]),os[1]))<0x1.b7cdfd9d7bdbbp-34L) {
                    point(file,os[0],is[1],offset_x,offset_y,runtime);point(file,ie[0],ie[1],offset_x,offset_y,runtime);point(file,ie[0],oe[1],offset_x,offset_y,runtime);point(file,os[0],os[1],offset_x,offset_y,runtime);point(file,os[0],is[1],offset_x,offset_y,runtime);
                }else if(load(os[1])<load(oe[1]) && x87::abs(subtract(load(os[0]),oe[0]))<0x1.b7cdfd9d7bdbbp-34L) {
                    point(file,is[0],os[1],offset_x,offset_y,runtime);point(file,ie[0],ie[1],offset_x,offset_y,runtime);point(file,oe[0],ie[1],offset_x,offset_y,runtime);point(file,os[0],os[1],offset_x,offset_y,runtime);point(file,is[0],os[1],offset_x,offset_y,runtime);
                }
            }
            runtime.print(file,"\nEND");
        }
        offset_x=x87::binary64(load(offset_x)+load(*reinterpret_cast<const double*>(shape->filaments.prefix+0x54)));offset_y=x87::binary64(load(offset_y)+load(*reinterpret_cast<const double*>(shape->filaments.prefix+0x5c)));
    }
    runtime.print(file,"\n");runtime.close(file,runtime.user);
}
}
