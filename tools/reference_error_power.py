"""Generate high-precision references for the algebraic QUADPACK x^(3/2) term."""
from pathlib import Path
import json,math,random,struct
import mpmath as mp
ROOT=Path(__file__).resolve().parents[1]
values=[0.,-0.,5e-324,2.2250738585072014e-308,3.967488138091792e-197,7.604978072193256e-141,4.694408367036838e-155,.1,.5,1.,math.nextafter(1.,0.),math.nextafter(1.,2.),2.,1e100,1e200,float.fromhex('0x1.fffffffffffffp+1023')]
values.extend(float(q*q) for q in [208065,208067,262141,262143])
rng=random.Random(0x51554144)
while len(values)<1040:
    x=struct.unpack('<d',struct.pack('<Q',rng.getrandbits(63)))[0]
    if math.isfinite(x):values.append(x)
cases=[]
with mp.workdps(200):
    for x in values:
        # Each binary64 input cubed fits exactly in 159 significand bits; this
        # reference uses a different expression and 200 decimal digits.
        y=float(mp.sqrt(mp.mpf(x)**3));cases.append({'input':x,'value':y if math.isfinite(y) else None,'overflow':not math.isfinite(y)})
p=ROOT/'tests/fixtures/portable-error-power.json';p.write_text(json.dumps({'reference':'mpmath sqrt(exact binary64 input cubed), 200 decimal digits','mpmath_version':mp.__version__,'cases':cases},indent=2)+'\n');print(p,len(cases))
