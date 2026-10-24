"""Independent high-precision nodal reduction of the three-port model.

The C++ solver factors a 2*n voltage/current system. Here currents are
eliminated first: (Y + D.T * Z**-1 * D) V = J. D is the voltage-difference
incidence matrix of the three conductor chains. No extra reference nodes
are introduced: their capacitance contribution is already in the reduced Y.
"""
import json,sys
import mpmath as mp

def reference(result):
    with mp.workdps(80):
        def complex_matrix(values):return mp.matrix([[mp.mpc(mp.mpf(z[0]),mp.mpf(z[1])) for z in row] for row in values])
        cap=complex_matrix(result['capacitance_matrix']);impedance=complex_matrix(result['inductance_impedance_matrix']);n=cap.rows
        assert n==cap.cols==impedance.rows==impedance.cols==result['node_count']
        groups=result['winding_groups'];assert len(groups)==3 and sum(groups)==n and all(g>0 for g in groups)
        difference=mp.eye(n);end=0
        for group in groups:
            for i in range(end,end+group-1):difference[i,i+1]=-1
            end+=group
        admittance=mp.matrix(n)
        for i in range(n):
            for j in range(n):admittance[i,j]=mp.fsum(cap[i,k] for k in range(n)) if i==j else -cap[i,j]
        node_impedance=(admittance+difference.T*(impedance**-1)*difference)**-1
        ports=[0,groups[0],sum(groups[:2])]
        z=mp.matrix([[node_impedance[i,j] for j in ports] for i in ports])
        pairs=[(0,1),(0,2),(1,2)];omega=mp.mpf(float.fromhex('0x1.921fb54442d18p+2'))*mp.mpf(result['frequency_hz'])
        inductance=[mp.im(z[i,i])/omega for i in range(3)];mutual=[mp.im(z[i,j])/omega for i,j in pairs]
        coupling=[mutual[k]/mp.sqrt(inductance[i]*inductance[j]) if inductance[i]*inductance[j]>0 else mp.nan for k,(i,j) in enumerate(pairs)]
        # Isolate the conversion stage from the preceding nodal reduction.
        # The saved binary64 impedance is the actual converter input.
        saved=complex_matrix(result['impedance_matrix']);identity=mp.eye(3)
        g=mp.mpf(float.fromhex('0x1.47ae147ae147bp-6'))
        port_order=[1,0,2]
        def converted(values):return [[[float(mp.re(values[i,j])),float(mp.im(values[i,j]))] for j in port_order] for i in port_order]
        conversion={'admittance_matrix':converted(saved**-1),'scattering_matrix':converted((g*saved-identity)*(g*saved+identity)**-1)}
        scalar=lambda x:float(x) if mp.isfinite(x) else None
        return {'mpmath_version':mp.__version__,'decimal_digits':80,'node_count':n,'conversion':conversion,'impedance_matrix':[[[float(mp.re(z[i,j])),float(mp.im(z[i,j]))] for j in range(3)] for i in range(3)],'model':{'inductance_h':list(map(scalar,inductance)),'resistance_ohm':[float(mp.re(z[i,i])) for i in range(3)],'mutual_inductance_h':list(map(scalar,mutual)),'cross_resistance_ohm':[float(mp.re(z[i,j])) for i,j in pairs],'coupling':list(map(scalar,coupling))}}

if __name__=='__main__':print(json.dumps(reference(json.load(sys.stdin)),allow_nan=False))
