#include "asitic/filaments.hpp"
#include "asitic/numerics.hpp"
#include "x87.hpp"

namespace asitic {
void build_filament(const ConductorSegment& segment, Filament& out,
                    const double* origin, int xi, int yi, int zi,
                    int nx, int ny, int nz, const FilamentSettings& settings) {
    using x87::binary64;
    double a[3],b[3],c[3],d[3];
    for(unsigned i=0;i<3;++i) {
        a[i]=segment.inner_start[i]; b[i]=segment.inner_end[i];
        c[i]=segment.outer_start[i]; d[i]=segment.outer_end[i];
    }
    double count=ny;
    if (static_cast<long double>(segment.width)/settings.edge_cell_size > settings.max_width_cells) {
        constexpr long double fraction=0x1.999999999999ap-2L; // binary64 0.4
        const int edges=static_cast<int>(fraction*ny);
        const int middle=settings.max_width_cells-2*edges;
        const auto blend = [&](const double* from, const double* to, double* dst) {
            const long double r=(static_cast<long double>(edges)*settings.edge_cell_size)/sqrt_dot(from,to);
            const long double inv=1.0L-r;
            dst[0]=binary64(from[0]*inv+r*to[0]);
            dst[1]=binary64(inv*from[1]+static_cast<long double>(binary64(r))*to[1]);
        };
        if(yi < edges) {
            blend(a,c,c); blend(b,d,d);
            out.values[6]=settings.edge_cell_size;
            count=edges;
        } else if(yi > settings.max_width_cells-edges-1) {
            blend(c,a,a); blend(d,b,b);
            yi-=edges+middle;
            out.values[6]=settings.edge_cell_size;
            count=edges;
        } else {
            double ca[3],cb[3],cc[3],cd[3];
            for(unsigned i=0;i<3;++i){ca[i]=a[i];cb[i]=b[i];cc[i]=c[i];cd[i]=d[i];}
            blend(ca,cc,a); blend(cb,cd,b);
            blend(cc,ca,c); blend(cd,cb,d);
            yi-=edges;
            out.values[6]=binary64((static_cast<long double>(segment.width)
                                   -(static_cast<long double>(settings.edge_cell_size)+settings.edge_cell_size)*edges)/edges);
            count=middle;
        }
    } else out.values[6]=binary64(static_cast<long double>(segment.width)/ny);
    out.metal=segment.metal;
    out.values[7]=binary64(static_cast<long double>(segment.thickness)/nz);
    const double u=binary64(static_cast<long double>(xi)/nx);
    const double v=binary64((static_cast<long double>(yi)+0.5L)/count);
    const double vm=binary64(1.0L-v), um=binary64(1.0L-u);
    const long double weight=((static_cast<long double>(u)*v+1.0L)-u)-v;
    const double vcx=binary64(static_cast<long double>(v)*c[0]);
    const long double vcy=static_cast<long double>(v)*c[1];
    // The x start retains complements in ST while the y start reloads them
    // as QWORDs. The v*cx product is spilled before x interpolation too.
    out.values[0]=binary64(((static_cast<long double>(b[0])*u*(1.0L-v)+a[0]*weight)
                           +static_cast<long double>(vcx)*(1.0L-u))+static_cast<long double>(d[0])*u*v+origin[0]);
    out.values[1]=binary64(((a[1]*weight+static_cast<long double>(b[1])*u*vm)+vcy*um)
                           +static_cast<long double>(d[1])*u*v+origin[1]);
    const long double layer_thickness=settings.metals[segment.metal].thickness;
    // Preserve the binary's dimensionless subtraction; there is no trailing
    // multiplication of the fraction by the layer thickness here.
    const double z=binary64((layer_thickness*0.5L+a[2])
                            -(static_cast<long double>(zi)+1.0L)/(static_cast<long double>(nz)+1.0L));
    out.values[2]=out.values[5]=z;
    const long double raw_u1=static_cast<long double>(xi+1)/nx;
    const double u1=binary64(raw_u1);
    // The end weight retains the raw quotient, although the other end
    // products use its binary64 store.
    const long double weight1=((raw_u1*v+1.0L)-raw_u1)-v;
    const long double um1=1.0L-u1;
    out.values[3]=binary64(((static_cast<long double>(b[0])*u1*vm+a[0]*weight1)
                           +static_cast<long double>(vcx)*um1)+static_cast<long double>(d[0])*u1*v+origin[0]);
    out.values[4]=binary64(((a[1]*weight1+static_cast<long double>(b[1])*u1*vm)
                           +um1*binary64(vcy))+static_cast<long double>(d[1])*u1*v+origin[1]);
}
}
