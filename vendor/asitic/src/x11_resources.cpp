#include "asitic/x11_runtime.hpp"
namespace asitic {
namespace {
#include "x11_bitmap_data.inc"
const unsigned char* screen(const X11State& s) {
    const auto* screens=*reinterpret_cast<const unsigned char*const*>(static_cast<const unsigned char*>(s.display)+0x8c);
    return screens+static_cast<unsigned>(s.screen)*0x50u;
}
unsigned screen_word(const X11State& s,unsigned offset) { return *reinterpret_cast<const unsigned*>(screen(s)+offset); }
short font_short(const void* font,unsigned offset) { return *reinterpret_cast<volatile const short*>(static_cast<const unsigned char*>(font)+offset); }
double channel(unsigned short value) {
    const int n=value;const double denominator=255.;double result;
    asm volatile("fldl %1; fidivrl %2; fstpl %0":"=m"(result):"m"(denominator),"m"(n):"st");return result;
}
X11GC create_gc(X11State& s,const X11Runtime& r) { return r.create_gc(s.display,s.window,0,nullptr); }
void mono_gc(X11State& s,X11GC gc,unsigned foreground,unsigned background,const X11Runtime& r) {
    r.set_fill_rule(s.display,gc,1);r.set_fill_style(s.display,gc,0);
    r.set_foreground(s.display,gc,foreground);r.set_background(s.display,gc,background);
}
void color_rule(X11State& s,X11GC gc,const X11Runtime& r) { r.set_fill_rule(s.display,gc,1);r.set_fill_rule(s.display,gc,0); }
}
void initialize_x11_resources(X11State& s,const TechnologyTables& t,X11ResourceHistory& h,const X11Runtime& r) {
    const unsigned first=r.create_bitmap(s.display,screen_word(s,8),reinterpret_cast<const char*>(stipple_even),16,16);
    const unsigned second=r.create_bitmap(s.display,screen_word(s,8),reinterpret_cast<const char*>(stipple_odd),16,16);
    s.metal_gcs=static_cast<X11GC*>(r.memory.arrays.get((static_cast<unsigned>(t.metal_count)+3u)*4u));
    if(!s.metal_gcs)r.fatal.emit("2701:  Could not allocate memory.");
    s.depth=static_cast<int>(screen_word(s,0x24));unsigned colormap=screen_word(s,0x30),background;
    if(s.depth==1) {
        for(int i=0;i<t.metal_count;++i) {
            s.metal_gcs[i]=create_gc(s,r);
            mono_gc(s,s.metal_gcs[i],screen_word(s,0x34),screen_word(s,0x38),r);
        }
        s.metal_gcs[t.metal_count]=create_gc(s,r);
        mono_gc(s,s.metal_gcs[t.metal_count],screen_word(s,0x38),screen_word(s,0x34),r);
        s.metal_gcs[t.metal_count+1]=create_gc(s,r);
        r.set_fill_rule(s.display,s.metal_gcs[t.metal_count+1],1);r.set_fill_style(s.display,s.metal_gcs[t.metal_count+1],0);
        r.set_foreground(s.display,s.metal_gcs[t.metal_count+1],screen_word(s,0x38));background=screen_word(s,0x34);
    } else {
        for(int i=0;i<t.metal_count;++i) {
            r.parse_color(s.display,colormap,t.metals[i].color,&h.color);
            auto* rgb=reinterpret_cast<double*>(t.metals[i].reserved);
            rgb[0]=channel(h.color.red);rgb[1]=channel(h.color.green);rgb[2]=channel(h.color.blue);
            r.allocate_color(s.display,colormap,&h.color);s.metal_gcs[i]=create_gc(s,r);
            r.set_fill_rule(s.display,s.metal_gcs[i],1);
            if(first && second) {
                r.set_fill_style(s.display,s.metal_gcs[i],2);r.set_stipple(s.display,s.metal_gcs[i],i&1?second:first);
            }else r.set_fill_style(s.display,s.metal_gcs[i],0);
            r.set_foreground(s.display,s.metal_gcs[i],h.color.pixel);r.set_background(s.display,s.metal_gcs[i],screen_word(s,0x38));
        }
        r.parse_color(s.display,colormap,"white",&h.color);r.allocate_color(s.display,colormap,&h.color);
        s.metal_gcs[t.metal_count]=create_gc(s,r);color_rule(s,s.metal_gcs[t.metal_count],r);
        r.set_foreground(s.display,s.metal_gcs[t.metal_count],h.color.pixel);r.set_background(s.display,s.metal_gcs[t.metal_count],screen_word(s,0x38));
        r.set_line_attributes(s.display,s.metal_gcs[t.metal_count],3,0,1,0);
        void* font=r.load_font(s.display,"9x15");
        if(!font)r.warnings.emit("Warning:  Unable to allocate 9x15 font");
        else r.set_font(s.display,s.metal_gcs[t.metal_count],*reinterpret_cast<const unsigned*>(static_cast<const unsigned char*>(font)+4));
        const int right=font_short(font,0x3a),left=font_short(font,0x2c);s.font_width=right-left;
        const int ascent=font_short(font,0x3e),descent=font_short(font,0x40);s.font_height=ascent+descent;s.font_baseline=font_short(font,0x34);
        r.parse_color(s.display,colormap,"gray85",&h.color);r.allocate_color(s.display,colormap,&h.color);
        s.metal_gcs[t.metal_count+1]=create_gc(s,r);color_rule(s,s.metal_gcs[t.metal_count+1],r);
        r.set_foreground(s.display,s.metal_gcs[t.metal_count+1],h.color.pixel);background=screen_word(s,0x38);
    }
    r.set_background(s.display,s.metal_gcs[t.metal_count+1],background);
    s.via_gcs=static_cast<X11GC*>(r.memory.arrays.get((static_cast<unsigned>(t.via_count)+2u)*4u));
    if(!s.via_gcs)r.fatal.emit("Could not allocate memory.");
    s.depth=static_cast<int>(screen_word(s,0x24));colormap=screen_word(s,0x30);
    if(s.depth==1) {
        for(int i=0;i<t.via_count;++i) {
            s.via_gcs[i]=create_gc(s,r);mono_gc(s,s.via_gcs[i],screen_word(s,0x34),screen_word(s,0x38),r);
        }
        s.via_gcs[t.via_count]=create_gc(s,r);mono_gc(s,s.via_gcs[t.via_count],screen_word(s,0x38),screen_word(s,0x34),r);
    } else {
        int i=0;
        for(;i<t.via_count;++i) {
            r.parse_color(s.display,colormap,t.vias[i].color,&h.color);r.allocate_color(s.display,colormap,&h.color);
            s.via_gcs[i]=create_gc(s,r);color_rule(s,s.via_gcs[i],r);
            r.set_foreground(s.display,s.via_gcs[i],h.color.pixel);r.set_background(s.display,s.via_gcs[i],screen_word(s,0x38));
        }
        r.parse_color(s.display,colormap,"grey",&h.color);r.allocate_color(s.display,colormap,&h.color);
        s.via_gcs[i]=create_gc(s,r);color_rule(s,s.via_gcs[t.via_count],r);
        r.set_foreground(s.display,s.via_gcs[t.via_count],h.color.pixel);r.set_background(s.display,s.via_gcs[t.via_count],screen_word(s,0x38));
    }
    s.grid_gc=create_gc(s,r);color_rule(s,s.grid_gc,r);r.set_function(s.display,s.grid_gc,6);
    r.set_foreground(s.display,s.grid_gc,screen_word(s,0x34));r.set_background(s.display,s.grid_gc,screen_word(s,0x38));
    s.default_gc=create_gc(s,r);color_rule(s,s.default_gc,r);r.set_function(s.display,s.default_gc,6);
    r.set_foreground(s.display,s.default_gc,screen_word(s,0x38));r.set_background(s.display,s.default_gc,screen_word(s,0x34));
    s.background_gc=create_gc(s,r);color_rule(s,s.background_gc,r);r.set_function(s.display,s.background_gc,3);
    r.set_foreground(s.display,s.background_gc,screen_word(s,0x38));r.set_background(s.display,s.background_gc,screen_word(s,0x38));
}
}
