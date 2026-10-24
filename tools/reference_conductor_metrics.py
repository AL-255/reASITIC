"""Independent physical DC/primary-ribbon area and low-frequency Q equation."""
import json,sys
import mpmath as mp

def reference(data):
    with mp.workdps(80):
        technology=data['technology'];metals=technology['metals'];vias=technology['vias'];output=[]
        measurements={w['name']:w for w in data['analysis']['windings']}
        epsilon=mp.mpf(float.fromhex('0x1.b7cdfd9d7bdbbp-34'))
        angular_nh=mp.mpf(float.fromhex('0x1.afc6f70213be5p-28'))
        for shape in data['layout']['shapes']:
            areas=[];resistances=[]
            for segment in shape['segments']:
                p,q=segment['points'][:2]
                length=mp.sqrt(mp.fsum((mp.mpf(a)-mp.mpf(b))**2 for a,b in zip(p,q)))
                if length>epsilon:areas.append(length*mp.mpf(segment['width']))
                metal=segment['metal']
                if metal<len(metals):
                    sheet=mp.mpf(metals[metal]['sheet_milliohms'])*mp.mpf(.001)
                    # The original process stores sheet resistance as binary64.
                    sheet=mp.mpf(float(sheet))
                    resistances.append(length/segment['width']*sheet)
                else:resistances.append(mp.mpf(vias[metal-len(metals)]['resistance'])/(segment['nx']*segment['ny']))
            dc=mp.fsum(resistances);w=measurements[shape['name']]
            numerator=angular_nh*mp.mpf(data['analysis']['frequency_hz'])*mp.mpf(w['inductance_nh'])
            output.append(dict(name=shape['name'],metal_area_um2=float(mp.fsum(areas)),resistance_dc=float(dc),
                               quality_low_frequency=float(numerator/dc) if dc else None,
                               quality_from_rounded_estimates=float(numerator/mp.mpf(w['resistance_dc'])) if w['resistance_dc'] else None))
        return dict(mpmath_version=mp.__version__,decimal_digits=80,windings=output)

if __name__=='__main__':print(json.dumps(reference(json.load(sys.stdin)),allow_nan=False))
