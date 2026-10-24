"""Write the fixed 200-scenario original-simulator regression inputs."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def cases():
    frequencies=[.25,1,2,3,5,7.5,10,15,20,40]
    result=[]
    for technology in ['CMOS','BiCMOS']:
        top,lower,first=('M5','M4','M1') if technology=='CMOS' else ('M3','M2','MSUB')
        starting='M4' if technology=='CMOS' else top
        starting_exit='M3' if technology=='CMOS' else lower
        templates=[
            ('starting-square',0,f'SQUARE NAME=A:LEN=100:WID=90:W=3:S=2:N=2:METAL={starting}:EXIT={starting_exit}:XORG=0:YORG=0','A',[50,1.5,.5],[50,20],[256,256]),
            ('wide-square',0,f'SQUARE NAME=A:LEN=220:WID=240:W=16:S=8:N=2:METAL={top}:EXIT={lower}:XORG=100:YORG=100','A',[1000,1000,1000],[1000,1000],[64,64]),
            ('polygon',1,f'SPIRAL NAME=A:RAD=100:SIDES=8:W=16:S=8:N=1.5:METAL={top}:XORG=256:YORG=256','A',[1000,1000,1000],[1000,1000],[64,64]),
            ('symmetric-square',16,f'SYMSQ NAME=A:LEN=200:ILEN=32:W=16:S=8:N=2:METAL={top}:METAL2={lower}:XORG=100:YORG=100','A',[1000,1000,1000],[1000,1000],[64,64]),
            ('symmetric-polygon',17,f'SYMPOLY NAME=A:RAD=110:SIDES=8:ILEN=32:W=16:S=8:N=2:METAL={top}:METAL2={lower}:XORG=256:YORG=256','A',[1000,1000,1000],[1000,1000],[64,64]),
            ('rotated-wire',2,f'WIRE NAME=A:LEN=160:WID=32:METAL={first}:XORG=150:YORG=150:ORIENT=37:PHASE=-1','A',[40,8,.25],[40,16],[64,64]),
            ('ring',22,f'RING NAME=A:RAD=100:W=16:GAP=30:SIDES=16:METAL={top}:XORG=256:YORG=256','A',[1000,1000,1000],[1000,1000],[64,64]),
            ('multi-metal-square',18,f'MMSQUARE NAME=A:LEN=200:WID=220:W=16:S=8:N=1.5:METAL={top}:EXIT={lower}:XORG=100:YORG=100','A',[1000,1000,1000],[1000,1000],[64,64]),
            ('planar-transformer',3,f'TRANS PNAME=A:SNAME=B:LEN=220:WID=200:WP=16:WS=12:S=8:NP=1.5:NS=1.25:METAL={top}:EXIT={lower}:XORG=100:YORG=100','A',[1000,1000,1000],[1000,1000],[64,64]),
            ('balun',5,f'BALUN NAME=A:LEN=220:W1=16:W2=12:S=8:N=3:METAL={top}:METAL2={lower}:XORG=100:YORG=100','A-P',[1000,1000,1000],[1000,1000],[64,64]),
        ]
        for name,creator_id,command,winding,mesh,contacts,fft in templates:
            for frequency in frequencies:
                result.append(dict(id=f'{technology.lower()}-{name}-{frequency:g}ghz',technology=technology,geometry=name,creator_id=creator_id,command=command,winding=winding,frequency_ghz=frequency,mesh=mesh,contact_mesh=contacts,fft=fft,eddy=False,maxnw=500))
    assert len(result)==len({c['id'] for c in result})==200
    return dict(schema=1,description='20 coil/process configurations x 10 frequencies; conductor estimates, LMAT and direct PI2; fixed input corpus, not filtered by recomp results',cases= result)
if __name__=='__main__':
    output=ROOT/'tests/fixtures/analysis-200-cases.json';output.write_text(json.dumps(cases(),indent=2)+'\n');print(output)
