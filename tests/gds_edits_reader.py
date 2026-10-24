"""Independently read edited GDS and compare native physical ribbon vertices."""
import collections,json,math,re
from pathlib import Path
import gdstk

original=json.loads(Path('tests/fixtures/original-geometry-edits.json').read_text())
technology=Path('web/technology/CMOS.tek').read_text();vias=[]
for block in re.split(r'<via>\s*\d*',technology,flags=re.I)[1:]:
    fields={k.lower():float(v) for k,v in re.findall(r'^\s*(\w+)\s*=\s*([-+\d.eE]+)',block,re.M)}
    vias.append(fields)
def records(segments):
    for segment in segments:
        yield segment
        yield from records(segment.get('branches',[]))
def vertices(points):return frozenset((math.floor(x*1000+.5),math.floor(y*1000+.5)) for x,y in points)
files=polygons=0
for case in original['cases']:
    path=Path('build/gds-edits')/(case['name']+'.gds');library=gdstk.read_gds(path)
    assert library.unit==1e-6 and library.precision==1e-9
    assert len(library.cells)==1 and not library.cells[0].references
    actual=library.cells[0].polygons;expected_count=0;expected_layers=collections.Counter();actual_layers=collections.Counter(p.layer for p in actual)
    available=collections.Counter((p.layer,vertices(p.points)) for p in actual)
    for shape in case['shapes']:
        for segment in records(shape['segments']):
            metal=segment['metal']
            if metal<5:
                points=[segment['points'][i] for i in [2,3,5,4]]
                absolute=[(p[0]+shape['origin'][0],p[1]+shape['origin'][1]) for p in points]
                key=(metal+1,vertices(absolute));assert available[key]>0,(case['name'],key);available[key]-=1
                expected_count+=1;expected_layers[metal+1]+=1
            else:
                via=vias[metal-5];cuts=segment['nx']*segment['ny'];expected_count+=2+cuts
                expected_layers[int(via['bottom'])+1]+=1;expected_layers[int(via['top'])+1]+=1;expected_layers[100+metal-5]+=cuts
    assert len(actual)==expected_count and actual_layers==expected_layers,case['name']
    assert all(p.area()>0 and p.datatype==0 for p in actual),case['name']
    files+=1;polygons+=len(actual)
print(f'Independent GDS reader: {files} edited files, {polygons} polygons; units, layers, branches, native conductor vertices and nonzero boundaries pass')
