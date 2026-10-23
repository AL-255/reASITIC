#include "asitic/layout_export.hpp"
#include "x87.hpp"
namespace asitic {
namespace {
long double fld(const double& value) { long double out;asm volatile("fldl %1":"=t"(out):"m"(value));return out; }
const double& raw_double(const void* record,unsigned offset) { return *reinterpret_cast<const double*>(static_cast<const char*>(record)+offset); }
int raw_int(const void* record,unsigned offset) { int value;__builtin_memcpy(&value,static_cast<const char*>(record)+offset,4);return value; }
const ConductorSegment* next(const ConductorSegment* p) { return reinterpret_cast<const ConductorSegment*>(p->next_address); }
long double origin(const GeometryShape& shape,unsigned offset) { return fld(raw_double(&shape,offset)); }
long double cif_width(const ConductorSegment& s) { return fld(raw_double(&s,0x30)); }
long double square(const double& end,const double& start) {
    const long double delta=fld(end)-fld(start);if(delta==0)return 0.L;const double squared=x87::binary64(delta*delta);return fld(squared);
}
long double root(long double value) {
    if(value==0)return 0;
    long double logarithm;asm volatile("fld1; fxch; fyl2x":"=t"(logarithm):"0"(value):"st(1)");
    const long double power=0.5L*logarithm,integer=x87::round(power);
    return x87::binary64(x87::scale(x87::pow2m1(power-integer)+1.L,integer));
}
const char* layer_name(int index,const TechnologyTables& t) { return index<t.metal_count?t.metals[index].name:t.vias[index-t.metal_count].name; }
}
void cif_segment_bounds(const ConductorSegment& s,double& xmin,double& ymin,double& xmax,double& ymax) {
    long double other=fld(s.inner_start[0]),value=fld(s.inner_end[0]);if(value>other)value=other;xmin=x87::binary64(value);
    other=fld(s.outer_start[0]);if(!(other>value))value=other;xmin=x87::binary64(value);
    other=fld(s.outer_end[0]);if(!(other>value))value=other;xmin=x87::binary64(value);
    other=fld(s.inner_start[1]);value=fld(s.inner_end[1]);if(value>other)value=other;ymin=x87::binary64(value);
    other=fld(s.outer_start[1]);if(!(other>value))value=other;ymin=x87::binary64(value);
    other=fld(s.outer_end[1]);if(!(other>value))value=other;ymin=x87::binary64(value);
    other=fld(s.inner_start[0]);value=fld(s.inner_end[0]);if(value<other)value=other;xmax=x87::binary64(value);
    other=fld(s.outer_start[0]);if(!(other<value))value=other;xmax=x87::binary64(value);
    other=fld(s.outer_end[0]);if(!(other<value))value=other;xmax=x87::binary64(value);
    other=fld(s.inner_start[1]);value=fld(s.inner_end[1]);if(value<other)value=other;ymax=x87::binary64(value);
    other=fld(s.outer_start[1]);if(!(other<value))value=other;ymax=x87::binary64(value);
    other=fld(s.outer_end[1]);if(!(other<value))value=other;ymax=x87::binary64(value);
}
void cif_via_enclosure(const ConductorSegment& metal,const ConductorSegment& via,double& width,double& height) {
    double mx0,my0,mx1,my1,vx0,vy0,vx1,vy1;cif_segment_bounds(metal,mx0,my0,mx1,my1);cif_segment_bounds(via,vx0,vy0,vx1,vy1);
    auto first=x87::abs(fld(mx0)-fld(vx0)),second=x87::abs(fld(mx1)-fld(vx1));auto selected=second>first?first:second;
    width=x87::binary64(selected+selected+cif_width(via));
    first=x87::abs(fld(my0)-fld(vy0));second=x87::abs(fld(my1)-fld(vy1));selected=first>second?second:first;
    height=x87::binary64(selected+selected+cif_width(via));
}
const ConductorSegment* cif_find_enclosing_segment(const GeometryShape& shape,const ConductorSegment& via,WarningSink warnings) {
    for(auto* p=shape.filaments.first;p;p=next(p))if(p!=&via) {
        double mx0,my0,mx1,my1,vx0,vy0,vx1,vy1;cif_segment_bounds(*p,mx0,my0,mx1,my1);cif_segment_bounds(via,vx0,vy0,vx1,vy1);
        if(fld(mx0)<=fld(vx0) && fld(mx1)>=fld(vx1) && fld(my0)<=fld(vy0) && fld(my1)>=fld(vy1))return p;
    }
    warnings.emit("Error:  Via segment found without any metal.");return nullptr;
}
void cif_emit_layer(int layer,void* file,const TechnologyTables& t,const CifExportRuntime& r) { r.print(file,"\nL%s;",layer_name(layer,t)); }
void cif_emit_wire(const GeometryShape& shape,const ConductorSegment& s,void* file,double scale,double resolution,const CifExportRuntime& r) {
    const auto factor=fld(scale)*fld(resolution),x=origin(shape,0x54),y=origin(shape,0x5c);
    const int x0=x87::truncate_int(factor*(x+fld(s.endpoints[0]))),y0=x87::truncate_int(factor*(y+fld(s.endpoints[1]))),
        x1=x87::truncate_int(factor*(x+fld(s.endpoints[3]))),y1=x87::truncate_int(factor*(y+fld(s.endpoints[4]))),w=x87::truncate_int(factor*cif_width(s));
    r.print(file,"\nW%-10d%-10d%-10d%-10d%-10d;",w,x0,y0,x1,y1);
}
void cif_emit_polygon(const GeometryShape& shape,const ConductorSegment& s,void* file,double scale,double resolution,const CifExportRuntime& r) {
    const auto factor=fld(scale)*fld(resolution),x=origin(shape,0x54),y=origin(shape,0x5c);
    const int x0=x87::truncate_int(factor*(x+fld(s.outer_start[0]))),y0=x87::truncate_int(factor*(y+fld(s.outer_start[1]))),
        x1=x87::truncate_int(factor*(x+fld(s.outer_end[0]))),y1=x87::truncate_int(factor*(y+fld(s.outer_end[1]))),
        x2=x87::truncate_int(factor*(x+fld(s.inner_end[0]))),y2=x87::truncate_int(factor*(y+fld(s.inner_end[1]))),
        x3=x87::truncate_int(factor*(x+fld(s.inner_start[0]))),y3=x87::truncate_int(factor*(y+fld(s.inner_start[1])));
    r.print(file,"\nP%-8d%-8d%-8d%-8d%-8d%-8d%-8d%-8d;",x0,y0,x1,y1,x2,y2,x3,y3);
}
void cif_emit_box(const GeometryShape& shape,const ConductorSegment& s,void* file,double scale,double resolution,const CifExportRuntime& r) {
    const auto dx=fld(s.endpoints[3])-fld(s.endpoints[0]),dy=fld(s.endpoints[4])-fld(s.endpoints[1]),factor=fld(scale)*fld(resolution);
    const int x=x87::truncate_int(factor*(origin(shape,0x54)+fld(s.endpoints[0])+dx*0.5L)),
        y=x87::truncate_int(factor*(origin(shape,0x5c)+fld(s.endpoints[1])+dy*0.5L)),
        rx=x87::truncate_int(factor*dx),ry=x87::truncate_int(factor*dy);
    const auto length=root(square(s.outer_end[1],s.outer_start[1])+square(s.outer_end[0],s.outer_start[0]));
    const int l=x87::truncate_int(factor*length),w=x87::truncate_int(factor*cif_width(s));
    r.print(file,"\nB%-10d%-10d%-10d%-10d%-10d%-10d;",l,w,x,y,rx,ry);
}
void cif_emit_via(const GeometryShape& shape,const ConductorSegment& s,void* file,double scale,double resolution,const TechnologyTables& t,const CifExportRuntime& r) {
    const double factor=x87::binary64(fld(scale)*fld(resolution)),ox=x87::binary64(origin(shape,0x54)),oy=x87::binary64(origin(shape,0x5c));
    const auto& via=t.vias[s.metal-t.metal_count];const double w=x87::binary64(fld(via.width)),spacing=x87::binary64(fld(via.spacing));const int nx=s.width_count,ny=s.thickness_count;
    const auto* enclosing=cif_find_enclosing_segment(shape,s,r.warnings);double width,height;cif_via_enclosure(*enclosing,s,width,height);
    r.print(file,"\nL%s;",t.metals[via.bottom].name);
    const double dx=x87::binary64(fld(s.endpoints[3])-fld(s.endpoints[0])),dy=x87::binary64(fld(s.endpoints[4])-fld(s.endpoints[1]));
    const int x=x87::truncate_int(fld(factor)*(fld(dx)*0.5L+fld(ox)+fld(s.endpoints[0]))),y=x87::truncate_int(fld(factor)*(fld(oy)+fld(s.endpoints[1])+fld(dy)*0.5L)),
        rx=x87::truncate_int(fld(factor)*fld(dx)),ry=x87::truncate_int(fld(factor)*fld(dy)),h=x87::truncate_int(fld(factor)*fld(height));
    r.print(file,"\nB%-10d%-10d%-10d%-10d%-10d%-10d;",x87::truncate_int(fld(factor)*fld(width)),h,x,y,rx,ry);
    r.print(file,"\nL%s;",t.metals[via.top].name);
    r.print(file,"\nB%-10d%-10d%-10d%-10d%-10d%-10d;",x87::truncate_int(fld(factor)*fld(width)),h,x,y,rx,ry);
    r.print(file,"\nL%s;",via.name);
    for(int ix=0;ix<nx;++ix)for(int iy=0;iy<ny;++iy) {
        const auto step=fld(spacing)+fld(w),pitchx=step*ix,pitchy=step*iy,
            startx=fld(ox)+fld(s.inner_start[0]),starty=fld(oy)+fld(s.inner_start[1]);
        const int x0=x87::truncate_int(fld(factor)*(startx+pitchx)),y0=x87::truncate_int(fld(factor)*(starty+pitchy)),
            x1=x87::truncate_int(fld(factor)*(startx+fld(w)+pitchx)),y1=x87::truncate_int(fld(factor)*(pitchy+starty+fld(w)));
        r.print(file,"\nP%-8d%-8d%-8d%-8d%-8d%-8d%-8d%-8d;",x0,y0,x1,y0,x1,y1,x0,y1);
    }
}
void export_cif(const GeometryShape& shape,const char* filename,double scale,double resolution,CifMode mode,const TechnologyTables& t,const CifExportRuntime& r) {
    int now;r.time(&now,r.user);void* file=r.open(filename,"w",r.user);
    if(!file) { r.warnings.emit("Error:  Could not open CIF file for writing.");return; }
    char* stamp=r.calendar_time(&now,r.user);normalize_input_line(stamp,0x80,r.characters);
    r.print(file,"(CIF file written on %s by ASITIC);",stamp);r.print(file,"\nDS %d 1 1;\n9 %s;",1,reinterpret_cast<const char*>(&shape));
    int current=-1;
    for(auto* p=shape.filaments.first;p;p=next(p)) {
        const int layer=raw_int(p,0x40);if(layer!=current) { cif_emit_layer(layer,file,t,r);current=layer; }
        if(current<t.metal_count) {
            if(mode==CifMode::box)cif_emit_box(shape,*p,file,scale,resolution,r);
            else if(mode==CifMode::wire)cif_emit_wire(shape,*p,file,scale,resolution,r);
            else cif_emit_polygon(shape,*p,file,scale,resolution,r);
        }else cif_emit_via(shape,*p,file,scale,resolution,t,r);
    }
    r.print(file,"\nDF;\nC %d;",1);r.print(file,"\nE");r.close(file,r.user);
}
}
