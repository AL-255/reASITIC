"""Independently verify creator GDS on both process stacks against original geometry."""
import collections,hashlib,json,math,re,sys
from pathlib import Path
import gdstk
from gds_large_grid_reader import verify_large_grid

proof_name='creator-counts' if '--counts' in sys.argv else 'creator-ranges' if '--ranges' in sys.argv else 'creator-coverage'
original=json.loads(Path('tests/fixtures/original-'+proof_name+'.json').read_text())
def vertices(points):return frozenset((math.floor(x*1000+.5),math.floor(y*1000+.5)) for x,y in points)
files=polygons=0
for case in original['cases']:
    technology=Path('web/technology/'+case['technology']+'.tek').read_text();vias=[]
    for block in re.split(r'<via>\s*\d*',technology,flags=re.I)[1:]:
        vias.append({k.lower():float(v) for k,v in re.findall(r'^\s*(\w+)\s*=\s*([-+\d.eE]+)',block,re.M)})
    metals=case['metal_count']
    path=Path('build/gds-'+proof_name)/(case['name']+'.gds')
    if path.stat().st_size>16*1024*1024:
        polygons+=verify_large_grid(path,case,vias);files+=1;continue
    library=gdstk.read_gds(path)
    assert library.unit==1e-6 and library.precision==1e-9
    assert len(library.cells)==1 and not library.cells[0].references
    actual=library.cells[0].polygons;expected=collections.Counter()
    for shape in case['shapes']:
        ox,oy=shape['origin']
        for segment in shape['segments']:
            points=segment['points'];metal=segment['metal']
            if metal<metals:
                expected[metal+1,vertices([(points[i][0]+ox,points[i][1]+oy) for i in [2,3,5,4]])]+=1
                continue
            via=vias[metal-metals];a,b,c=points[2:5]
            width=math.hypot(b[0]-a[0],b[1]-a[1]);height=math.hypot(a[0]-c[0],a[1]-c[1])
            xunit=((b[0]-a[0])/width,(b[1]-a[1])/width);yunit=((a[0]-c[0])/height,(a[1]-c[1])/height)
            def rectangle(x0,y0,x1,y1):return vertices([(c[0]+x*xunit[0]+y*yunit[0]+ox,c[1]+x*xunit[1]+y*yunit[1]+oy) for x,y in [(x0,y0),(x1,y0),(x1,y1),(x0,y1)]])
            for layer,over in [(int(via['bottom']),via['overplot1']),(int(via['top']),via['overplot2'])]:expected[layer+1,rectangle(-over,-over,width+over,height+over)]+=1
            pitch=via['width']+via['space']
            for x in range(segment['nx']):
                for y in range(segment['ny']):expected[100+metal-metals,rectangle(x*pitch,y*pitch,x*pitch+via['width'],y*pitch+via['width'])]+=1
    assert collections.Counter((p.layer,vertices(p.points)) for p in actual)==expected,case['name']
    assert all(p.area()>0 and p.datatype==0 for p in actual),case['name']
    files+=1;polygons+=len(actual)
print(f'Independent creator GDS reader: {files} libraries, {polygons} polygons; original ribbons, every via cut/pad, units and nonzero boundaries pass')
evidence=Path('build/evidence');evidence.mkdir(exist_ok=True,parents=True)
(evidence/('gds-'+proof_name+'.json')).write_text(json.dumps(dict(reference='Original native physical records; independent gdstk and bounded-memory GDS readers',fixture_sha256=hashlib.sha256(Path('tests/fixtures/original-'+proof_name+'.json').read_bytes()).hexdigest(),libraries=files,polygons=polygons),indent=2)+'\n')
