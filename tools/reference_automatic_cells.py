"""Independent high-precision wavelength and skin-depth mesh oracle."""
import json,sys
import mpmath as mp

with mp.workdps(150):
    permeability=mp.mpf(float.fromhex('0x1.08ef4a5975084p-18'))
    micrometres=mp.mpf(float.fromhex('0x1.0c6f7a0b5ed8dp-20'))
    results=[]
    for frequency,alpha,beta,thickness,resistance in json.load(sys.stdin):
        f,a,b,t,r=map(mp.mpf,[frequency,alpha,beta,thickness,resistance])
        # Independently express the lengths in um. The original lambda is a
        # quarter wavelength; resistivity = thickness(um) * sheet resistance.
        length=a*mp.mpf(75000000000000)/f
        width=b*mp.sqrt(t*r*micrometres*mp.mpf(1000000000000)/(f*permeability))
        results.append([float(length),float(width)])
    json.dump(results,sys.stdout)
