"""Verify every record of a large native axis-aligned via grid in bounded RAM."""
import mmap,struct,tempfile
from pathlib import Path
import gdstk

def verify_large_grid(path,case,vias):
    assert len(case['shapes'])==1 and len(case['shapes'][0]['segments'])==1
    shape=case['shapes'][0];segment=shape['segments'][0];metals=case['metal_count'];via=vias[segment['metal']-metals]
    a,b,c=segment['points'][2:5]
    assert b[0]>a[0] and b[1]==a[1] and c[0]==a[0] and a[1]>c[1],case['name']+' streaming fixture must be axis-aligned'
    def nm(x):
        scaled=x*1000;value=round(scaled);assert abs(scaled-value)<.001,(case['name'],x,'not on nm grid');return value
    x0,y0=nm(c[0]+shape['origin'][0]),nm(c[1]+shape['origin'][1]);pitch=nm(via['width']+via['space']);cut=nm(via['width']);nx,ny=segment['nx'],segment['ny'];count=nx*ny+2
    width,height=nm(b[0]-a[0]),nm(a[1]-c[1])
    def rectangle(x,y,w,h):return (x,y,x+w,y,x+w,y+h,x,y+h,x,y)
    def record(data,offset,layer,xy):
        header=struct.pack('>HBBHBBhHBBhHBB',4,8,0,6,13,2,layer,6,14,2,0,44,16,3)
        assert data[offset:offset+20]==header and data[offset+60:offset+64]==b'\x00\x04\x11\x00',(case['name'],'record framing',offset)
        assert struct.unpack_from('>10i',data,offset+20)==xy,(case['name'],'native cut/pad vertices',offset)
    with Path(path).open('rb') as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as data:
        assert len(data)==110+64*count and data[-8:]==b'\x00\x04\x07\x00\x00\x04\x04\x00'
        for i,(metal,over) in enumerate([(via['bottom'],via['overplot1']),(via['top'],via['overplot2'])]):
            overlap=nm(over);record(data,102+64*i,int(metal)+1,rectangle(x0-overlap,y0-overlap,width+2*overlap,height+2*overlap))
        offset=230;layer=100+segment['metal']-metals
        for x in range(nx):
            for y in range(ny):record(data,offset,layer,rectangle(x0+x*pitch,y0+y*pitch,cut,cut));offset+=64
        # A separate GDS implementation parses the complete header and landing
        # pads plus the farthest cut, without allocating two million polygons.
        sample=data[:230]+data[-72:-8]+data[-8:]
    with tempfile.TemporaryDirectory(prefix='gds-grid-reader-') as folder:
        sample_path=Path(folder)/'sample.gds';sample_path.write_bytes(sample);library=gdstk.read_gds(sample_path)
        assert library.unit==1e-6 and library.precision==1e-9 and len(library.cells)==1
        assert len(library.cells[0].polygons)==3 and all(p.area()>0 and p.datatype==0 for p in library.cells[0].polygons)
    print(f'Large GDS stream: {case["name"]}, {count} original cut/pad polygons; every signed coordinate, layer and record verified with bounded RAM')
    return count
