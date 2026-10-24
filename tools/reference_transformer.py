"""Independent folded circuit reduction and published transformer labels."""
import json,sys
import mpmath as mp
from reference_two_port import reference,matrix,encode,scalar

def transformer(result):
    output=reference(result,nodes_only=True)
    with mp.workdps(80):
        # Independently invert the published 2x2 Y. Native winding labels
        # reverse the two diagonals; use both off-diagonal input cells.
        inverse=matrix(result['matrix'])**-1
        first,second,cross=inverse[1,1],inverse[0,0],inverse[1,0]
        omega=mp.mpf(float.fromhex('0x1.921fb54442d18p+2'))*mp.mpf(result['frequency_hz'])
        l1,l2,m=mp.im(first)/omega,mp.im(second)/omega,-mp.im(cross)/omega
        values=dict(first_inductance_h=l1,second_inductance_h=l2,
                    first_resistance_ohm=mp.re(first-cross),second_resistance_ohm=mp.re(second-cross),
                    ground_resistance_ohm=mp.re(cross),mutual_inductance_h=m,coupling=m/mp.sqrt(l1*l2))
        output['transformer']={key:scalar(value) for key,value in values.items()}
        output['impedance_matrix']=encode(mp.matrix([[first,cross],[cross,second]]))
    return output

if __name__=='__main__':print(json.dumps(transformer(json.load(sys.stdin)),allow_nan=False))
