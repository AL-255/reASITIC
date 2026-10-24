"""Rasterize the real SVG output and independently locate native via cuts."""
import json,math,re,xml.etree.ElementTree as ET
from pathlib import Path
import cairosvg
from PIL import Image
base=Path('build/evidence/geometry-preview')
for source in base.glob('*.svg'):cairosvg.svg2png(url=str(source),write_to=str(source.with_suffix('.png')))
cases=json.loads(Path('tests/fixtures/original-creator-counts.json').read_text())['cases']
# Composite the two original landing-pad colors, then the via color, using
# the preview opacity. No pattern coordinates or SVG transforms supply cuts.
def over(back,front,opacity=.77):
    a=opacity+back[3]*(1-opacity)
    return tuple((front[i]*opacity+back[i]*back[3]*(1-opacity))/a for i in range(3))+(a,)
color=lambda s:tuple(int(s[i:i+2],16) for i in (0,2,4))
pad=over(over((0,0,0,0),color('8ab9df')),color('b89bde'));cut=over(pad,color('e1b486'))
expected=lambda value:tuple(round(v) for v in value[:3])+(round(value[3]*255),)
checks=0
for name in ['cmos-via-matrix','cmos-via-rotated']:
    case=next(c for c in cases if c['name']==name);shape=case['shapes'][0];segment=shape['segments'][0]
    tech=Path('web/technology/CMOS.tek').read_text();block=re.split(r'<via>\s*\d*',tech,flags=re.I)[2];fields=dict((k.lower(),float(v)) for k,v in re.findall(r'^\s*(\w+)\s*=\s*([-+\d.eE]+)',block,re.M));width=fields['width'];pitch=width+fields['space']
    a,b,c=segment['points'][2:5];length=math.hypot(b[0]-a[0],b[1]-a[1]);height=math.hypot(a[0]-c[0],a[1]-c[1]);u=((b[0]-a[0])/length,(b[1]-a[1])/length);v=((a[0]-c[0])/height,(a[1]-c[1])/height)
    source=base/(name+'-zoom.svg');root=ET.parse(source).getroot();vx,vy,vw,vh=map(float,root.attrib['viewBox'].split());w,h=float(root.attrib['width']),float(root.attrib['height']);scale=min(w/vw,h/vh);mx=(w-vw*scale)/2;my=(h-vh*scale)/2
    image=Image.open(source.with_suffix('.png')).convert('RGBA')
    def sample(x,y):
        px=c[0]+x*u[0]+y*v[0]+shape['origin'][0];py=c[1]+x*u[1]+y*v[1]+shape['origin'][1]
        ix,iy=round((px-vx)*scale+mx),round((-py-vy)*scale+my)
        assert 1<=ix<image.width-1 and 1<=iy<image.height-1,(name,ix,iy)
        pixels=[image.getpixel((ix+dx,iy+dy)) for dx in (-1,0,1) for dy in (-1,0,1)]
        return tuple(sum(p[i] for p in pixels)/len(pixels) for i in range(4))
    for x in range(min(5,segment['nx'])):
        for y in range(min(5,segment['ny'])):
            got=sample(x*pitch+width/2,y*pitch+width/2)
            assert max(abs(a-b) for a,b in zip(got,expected(cut)))<=6,(name,'via center',x,y,got,expected(cut));checks+=1
            margin=width/8
            for dx,dy,value,label in [(width-margin,width/2,cut,'inside x edge'),(width+margin,width/2,pad,'outside x edge'),(width/2,width-margin,cut,'inside y edge'),(width/2,width+margin,pad,'outside y edge')]:
                got=sample(x*pitch+dx,y*pitch+dy)
                assert max(abs(a-b) for a,b in zip(got,expected(value)))<=6,(name,label,x,y,got,expected(value));checks+=1
            if x+1<min(5,segment['nx']):
                got=sample(x*pitch+width+fields['space']/2,y*pitch+width/2)
                assert max(abs(a-b) for a,b in zip(got,expected(pad)))<=6,(name,'via spacing',x,y,got,expected(pad));checks+=1
Path('build/evidence/svg-preview.json').write_text(json.dumps(dict(renderer='CairoSVG '+cairosvg.__version__,cases=['cmos-via-matrix','cmos-via-rotated'],nativeCutCenterEdgeAndGapSamples=checks,maximumChannelError=6,reference='Original BSAVE geometry and native technology dimensions'),indent=2)+'\n')
print(f'Independent SVG raster reader: {checks} native via centers, edges and gaps; axis-aligned and rotated/reflected array positions, dimensions, opacity and visible tiles pass')
