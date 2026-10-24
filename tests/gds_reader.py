"""Read exported streams with an independent GDS implementation."""
from pathlib import Path
import json,math
import gdstk
count=0
for p in sorted(Path('build/evidence').glob('*.gds')):
    library=gdstk.read_gds(p)
    assert math.isclose(library.unit,1e-6,rel_tol=1e-14)
    assert math.isclose(library.precision,1e-9,rel_tol=1e-14)
    assert len(library.cells)==1
    polygons=library.cells[0].polygons
    assert polygons and all(poly.area()>0 for poly in polygons),p
    assert all(poly.datatype==0 for poly in polygons)
    if p.name.startswith('via'):
        assert len([poly for poly in polygons if poly.layer==100])==4
        assert {poly.layer for poly in polygons}=={1,2,100}
    count+=1
assert count>=20
print(f'Independent GDS reader: {count} complete libraries; units, nonzero boundaries, via cuts and pads verified')
