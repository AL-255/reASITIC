"""Independent PIX reduction using charge conservation and mean chain current.

Eliminate n+1 chain currents from the original 2n+2 node system. For midpoint
averaging B, forward differences D and cumulative integration T, conservation
gives I = 1*I0 - T*C*B*V. Substitution in D*V = Z*B*I leaves n+2 unknowns,
including I0 and both voltage boundaries. This avoids the C++ block stamping
and its LU factorization while using the same physical C and Z inputs.
"""
import json,sys
import mpmath as mp
from reference_two_port import metrics

def reference(result,*,nodes_only=False):
    digits=int(result.get('reference_decimal_digits',80))
    assert 80<=digits<=1000
    with mp.workdps(digits):
        def matrix(values):return mp.matrix([[mp.mpc(mp.mpf(z[0]),mp.mpf(z[1])) for z in row] for row in values])
        c=matrix(result['capacitance_matrix']);z=matrix(result['inductance_impedance_matrix']);n=c.rows
        assert n==c.cols==z.rows==z.cols and n>0
        b=mp.matrix(n,n+1);d=mp.matrix(n,n+1);t=mp.matrix(n+1,n)
        for i in range(n):
            b[i,i]=b[i,i+1]=mp.mpf('.5');d[i,i]=1;d[i,i+1]=-1
            for j in range(i+1,n+1):t[j,i]=1
        reduced=(z**-1)*d+b*t*c*b
        system=mp.matrix(n+2);rhs=mp.matrix(n+2,2)
        for i in range(n):
            for j in range(n+1):system[i,j]=reduced[i,j]
            system[i,n+1]=-1
        system[n,0]=system[n+1,n]=1;rhs[n,0]=rhs[n+1,1]=1
        solved=(system**-1)*rhs
        voltages=solved[:n+1,:];currents=mp.ones(n+1,1)*solved[n+1:n+2,:]-t*c*b*voltages
        y=mp.matrix([[-currents[n,1],-currents[n,0]],[currents[0,1],currents[0,0]]])
        encode=lambda m:[[[float(mp.re(m[i,j])),float(mp.im(m[i,j]))] for j in range(m.cols)] for i in range(m.rows)]
        scalar_pair=lambda z:[float(mp.re(z)),float(mp.im(z))]
        output=dict(mpmath_version=mp.__version__,decimal_digits=digits,groups=n,matrix=encode(y),currents=encode(currents[:,0:1]),voltages=encode(voltages[:,0:1]),
                    circuit_shunts=dict(first=scalar_pair(y[0,0]+y[0,1]),second=scalar_pair(y[1,0]+y[1,1])))
        if not nodes_only:
            output.update(circuit_metrics=metrics(y,mp.mpf(result['frequency_hz'])),
                          reported_metrics=metrics(matrix(result['matrix']),mp.mpf(result['frequency_hz']),result.get('pi_shunts')))
        return output

if __name__=='__main__':print(json.dumps(reference(json.load(sys.stdin),nodes_only='--nodes-only' in sys.argv[1:]),allow_nan=False))
