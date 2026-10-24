"""Generate independent high-precision references for the cross-section integral."""
from pathlib import Path
import json,math,random
import mpmath as mp
ROOT=Path(__file__).resolve().parents[1]
def reference(values):
    scale=max(map(abs,values));loss=sum(max(0,math.log10(scale)-math.log10(v)) for v in values[:4]);digits=max(100,int(loss)+100)
    with mp.workdps(digits):
        w1,w2,t1,t2,dx,dy=map(mp.mpf,values);r=mp.sqrt(dx*dx+dy*dy)
        if r>mp.mpf('1.5')*(w1+w2) and r>mp.mpf('1.5')*(t1+t2):return float(r),digits
        scale=max(map(abs,[w1,w2,t1,t2,dx,dy]));a,b,c,d,x,y=[v/scale for v in [w1,w2,t1,t2,dx,dy]]
        def primitive(x,y):
            x2,y2=x*x,y*y
            if not x or not y:
                square=x2 or y2
                return square*square*mp.log(square)/24 if square else mp.mpf(0)
            return mp.log(x2+y2)*(x2*x2-6*x2*y2+y2*y2)/24-x*y*(x2*mp.atan(y/x)+y2*mp.atan(x/y))/3
        xs=[(a-b)/2-x,-(a+b)/2-x,(a+b)/2-x,-(a-b)/2-x];ys=[(c-d)/2-y,(c+d)/2-y,-(c+d)/2-y,-(c-d)/2-y];signs=[1,-1,-1,1]
        total=mp.fsum(signs[i]*signs[j]*primitive(xs[i],ys[j]) for i in range(4) for j in range(4))
        return float(scale*mp.exp(-total/(2*a*b*c*d)-mp.mpf(25)/12)),digits

def direct_integral(values):
    """Independent quadrature of the mean log distance, without corner sums.

    Differences of two uniform coordinates have trapezoidal densities. The
    selected disjoint rectangles keep the logarithmic singularity outside the
    integration domain, so high-order Gauss quadrature converges rapidly.
    """
    with mp.workdps(70):
        w1,w2,t1,t2,dx,dy=map(mp.mpf,values)
        nodes,weights=mp.gauss_quadrature(64,'legendre')
        def axis(a,b):
            outer,inner=(a+b)/2,abs(a-b)/2
            cuts=sorted(set([-outer,-inner,inner,outer]));terms=[]
            for lo,hi in zip(cuts,cuts[1:]):
                for node,weight in zip(nodes,weights):
                    x=(lo+hi)/2+(hi-lo)*node/2
                    density=min(a,b,outer-abs(x))/(a*b)
                    terms.append((x,weight*(hi-lo)*density/2))
            return terms
        xs,ys=axis(w1,w2),axis(t1,t2)
        average=mp.fsum(wx*wy*mp.log((x-dx)**2+(y-dy)**2)/2 for x,wx in xs for y,wy in ys)
        return float(mp.exp(average))

def main():
    cases=[]
    def add(name,values):
        result,digits=reference(values);cases.append({'name':name,'input':values,'distance':result,'reference_decimal_digits':digits})
    add('square-self',[1,1,1,1,0,0]);add('wire-self',[64,64,.5,.5,0,0]);add('wire-adjacent',[64,64,.5,.5,64,0])
    for t in [1e-8,1e-40,1e-100,1e-300]:
        add('thin-self-'+str(t),[64,64,t,t,0,0]);add('thin-adjacent-'+str(t),[64,64,t,t,64,0])
    add('asymmetric-extreme',[1e-300,64,.5,1e-200,30,.2])
    add('subnormal-scale',[5e-324]*4+[0,0])
    m,n=100000001,41421356
    add('farfield-rounding-midpoint',[1,1,1,1,float(m*m-n*n),float(2*m*n)])
    for dx in [math.nextafter(3.,0.),3.,math.nextafter(3.,4.)]:add('farfield-cutoff-'+repr(dx),[1,1,.5,.5,dx,0])
    add('farfield-cutoff-tiny-width',[1,1e-300,.5,.5,1.5,0])
    base=[64,40,.5,.5,33.33333333333333,.16666666666666666]
    for power in [-900,-500,0,500,900]:add('power-two-scale-'+str(power),[math.ldexp(v,power) for v in base])
    rng=random.Random(0x415349544943)
    for i in range(180):
        w1,w2=[10**rng.uniform(-3,3) for _ in range(2)];t1,t2=[10**rng.uniform(-6,1) for _ in range(2)]
        dx=rng.uniform(-1.4,1.4)*(w1+w2);dy=rng.uniform(-1.4,1.4)*(t1+t2)
        add('rectangles-'+str(i),[w1,w2,t1,t2,dx,dy])
    with mp.workdps(100):
        square=float(mp.exp((mp.pi+mp.log(2))/3-mp.mpf(25)/12));flat=float(64*mp.exp(-mp.mpf(3)/2));adjacent=float(256*mp.exp(-mp.mpf(3)/2))
    assert cases[0]['distance']==square
    for case in cases:
        if case['name'] in ['thin-self-1e-100','thin-self-1e-300']:assert case['distance']==flat
        if case['name'] in ['thin-adjacent-1e-100','thin-adjacent-1e-300']:assert case['distance']==adjacent
    for values in [[2,1,.5,.25,2,.7],[1,.75,.25,.5,-1.5,.8]]:
        assert reference(values)[0]==direct_integral(values), 'Corner formula disagrees with direct log-distance quadrature'
    path=ROOT/'tests/fixtures/portable-gmd.json';path.write_text(json.dumps({'reference':'mpmath independently evaluates the exact rectangular logarithmic integral at 100+ decimal digits; precision follows the input aspect ratios','mpmath_version':mp.__version__,'analytic_limits':{'square_self':square,'flat_self_width_64':flat,'flat_adjacent_width_64':adjacent},'cases':cases},indent=2)+'\n');print(path,len(cases),'cases')
if __name__=='__main__':main()
