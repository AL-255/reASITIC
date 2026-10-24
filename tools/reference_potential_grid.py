"""Independent 80-digit finite cosine-series contact integrals.

Invert the original *square* Green cache with direct DCT-I sums, restrict its
spectral coefficients to the requested rectangular truncation, and integrate
cosine basis functions analytically over each contact. This uses neither the
ported rectangular table addressing nor its corner-difference assembly/FFT.
The zero mode follows the two-layer planar series impedance independently.
"""
import base64,gzip,hashlib,json,struct,sys
from pathlib import Path
import mpmath as mp
ROOT=Path(__file__).resolve().parents[1]

def reference(cases):
    fixture=json.loads(gzip.decompress((ROOT/'tests/fixtures/original-potential-grid.json.gz').read_bytes()))
    raw=base64.b64decode(fixture['pair_payload_base64']);assert hashlib.sha256(raw).hexdigest()==fixture['pair_payload_sha256']
    with mp.workdps(80):
        n=fixture['base_grid'];values=[mp.mpc(*z) for z in struct.iter_unpack('<dd',raw)];xy=mp.matrix(n+1,n+1)
        for i in range(n+1):
            for j in range(n+1):xy[i,j]=values[i*(n+1)+j]
        max_mode=min(n,max(max(c['grid']) for c in cases))
        # Full-domain cosine averages vanish exactly for every nonzero mode.
        # Such contacts need no unknown samples beyond the captured cutoff.
        for c in cases:
            for axis,limit in enumerate(c['grid']):
                assert limit<=n or all(a[2*axis]==0 and a[2*axis+1]==limit for a in c['contacts'])
        # DCT-I is its own inverse up to 2*n. The native table is the even
        # extension's unnormalized FFT, so undo each dimension independently.
        basis=mp.matrix([[mp.cos(mp.pi*k*j/n)*(1 if j in (0,n) else 2)/(2*n) for j in range(n+1)] for k in range(max_mode)])
        spectrum=basis*xy*basis.T
        x=basis*mp.matrix(values[(n+1)**2:(n+1)**2+n+1]);y=basis*mp.matrix(values[(n+1)**2+2*n:(n+1)**2+2*n+n+1])
        w=mp.mpf(fixture['frequency_hz'])*mp.mpf(float.fromhex('0x1.921fb54442d18p+2'))*mp.mpf(float.fromhex('0x1.8ebfab864ade4p-44'))*mp.mpf(float.fromhex('0x1.a36e2eb1c432dp-14'))
        coefficient=lambda rho,eps:1/(1/(mp.mpf(rho)*10000)+mp.j*w*mp.mpf(eps))
        height=mp.mpf(50)-mp.mpf(float(mp.mpf(50)-mp.mpf(1.3)))
        zero=700*coefficient(.001,11.9)+height*coefficient(1e10,3.9)
        area=mp.mpf(fixture['domain'][0])*fixture['domain'][1]
        encode=lambda matrix:[[[float(mp.re(v)),float(mp.im(v))] for v in row] for row in matrix.tolist()]
        original=fixture['original_cases'][0]['printed_potential'][0][0]
        for v,expected in zip([mp.re(zero/area),mp.im(zero/area)],original):assert float(format(float(v),'.10g'))==expected
        out=[]
        for c in cases:
            nx,ny=c['grid'];contacts=c['contacts'];count=len(contacts)
            def averages(a,b,limit):
                if a==0 and b==limit:return [mp.mpf(1)]+[mp.mpf(0)]*(limit-1)
                return [mp.mpf(1)]+[(mp.sin(mp.pi*k*b/limit)-mp.sin(mp.pi*k*a/limit))/(mp.pi*k*(b-a)/limit) for k in range(1,limit)]
            ux=[averages(a[0],a[1],nx) for a in contacts];uy=[averages(a[2],a[3],ny) for a in contacts];potential=mp.matrix(count)
            for a in range(count):
                for b in range(count):
                    gx=[ux[a][k]*ux[b][k] for k in range(nx)];gy=[uy[a][k]*uy[b][k] for k in range(ny)]
                    potential[a,b]=(zero+2*mp.fsum(x[k]*k*k*gx[k] for k in range(1,nx) if gx[k])+2*mp.fsum(y[l]*l*l*gy[l] for l in range(1,ny) if gy[l])+4*mp.fsum(spectrum[k,l]*k*k*l*l*gx[k]*gy[l] for k in range(1,nx) for l in range(1,ny) if gx[k] and gy[l]))/area
            admittance=potential**-1;folded=mp.fsum(admittance);impedance=1/folded
            angular=mp.mpf(fixture['frequency_hz'])*mp.mpf(float.fromhex('0x1.921fb54442d18p+2'))
            out.append({'name':c['name'],'potential_matrix':encode(potential),'contact_admittance_matrix':encode(admittance),'matrix':[[[float(mp.re(folded)),float(mp.im(folded))]]],'capacitance_f':float(-1/(angular*mp.im(impedance))),'resistance':float(mp.re(impedance))})
        return {'decimal_digits':80,'square_original_cache_sha256':fixture['pair_payload_sha256'],'cases':out}
if __name__=='__main__':print(json.dumps(reference(json.load(sys.stdin)['cases']),allow_nan=False))
