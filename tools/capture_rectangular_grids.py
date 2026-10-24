"""Capture square Green data and fresh rectangular-grid failures in original ASITIC."""
import argparse,base64,gzip,hashlib,json,os,re,resource,struct,subprocess,tempfile
from pathlib import Path
from capture_lmat import ROOT,ORIGINAL_SHA256
from capture_capacitance import matrix

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=Path,default=Path.home()/'github/asitic-re');a=p.parse_args()
    binary=a.reference/'run/asitic.linux.2.2';assert hashlib.sha256(binary.read_bytes()).hexdigest()==ORIGINAL_SHA256
    technology=ROOT/'web/technology/CMOS.tek';text=technology.read_text();work=Path(tempfile.mkdtemp(prefix='rectangular-',dir=ROOT/'build'))
    env=dict(os.environ,TERM='dumb',INPUTRC='/dev/null',LD_LIBRARY_PATH='/tmp/asitic-native-deps/readline-original:'+str(a.reference/'run/libs')+':/tmp/asitic-native-deps/root/usr/lib/i386-linux-gnu');env.pop('LD_PRELOAD',None)
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(16*1024*1024,16*1024*1024))
    cases=[];payloads=[]
    for nx,ny in [(32,32),(8,16),(16,8)]:
        repeats=[]
        for repeat in range(2):
            d=work/f'{nx}x{ny}-{repeat}';d.mkdir();tech=text
            for key,value in [('fftx',nx),('ffty',ny)]:tech=re.sub(r'(?mi)^\s*'+key+r'\s*=.*$',key+' = '+str(value),tech)
            (d/'process.tek').write_text(tech);(d/'input.asit').write_text('EDDY FALSE\nCCELL 1000 1000\nSAVEMAT TRUE\nWIRE NAME=A:LEN=512:WID=512:METAL=M1:XORG=0:YORG=0\nCAP A 1\n')
            with (d/'stdout.log').open('w') as out,(d/'stderr.log').open('w') as err:
                r=subprocess.run([str(binary),'-g','-t','process.tek','-x','input.asit'],cwd=d,env=env,input='EXIT\n',text=True,stdout=out,stderr=err,start_new_session=True,preexec_fn=limits,timeout=60)
            data={'returncode':r.returncode,'printed_potential':matrix(d/'Pmat.out') if (d/'Pmat.out').exists() else None}
            if nx==ny:
                assert r.returncode==0
                raw=next(d.glob('*.dat')).read_bytes();offset=16+2*40+5*236;payload=raw[offset:];assert len(payload)==15*((nx+1)*(ny+1)+2*nx+2*ny)*16
                data['frequency_hz']=struct.unpack_from('<d',raw,8)[0];data['cache_payload_sha256']=hashlib.sha256(payload).hexdigest();payloads.append(payload[:((nx+1)*(ny+1)+2*nx+2*ny)*16])
            else:assert r.returncode<0,'Original rectangular failure changed: '+str(data)
            repeats.append(data)
        assert repeats[0]==repeats[1],str((nx,ny))+' original outcome varies'
        cases.append({'grid':[nx,ny],'fresh_repeats':2,**repeats[0]});print(str((nx,ny))+': two matching fresh original outcomes',flush=True)
    assert payloads[0]==payloads[1]
    document={'reference':'Unmodified original square-grid Green cache, binary64 bytes for M1/M1; rectangular process outcomes are diagnostics, not expected numerical values','executable_sha256':ORIGINAL_SHA256,'technology_sha256':hashlib.sha256(technology.read_bytes()).hexdigest(),'frequency_ghz':1,'frequency_hz':cases[0]['frequency_hz'],'domain':[512,512],'base_grid':32,'pair':[0,0],'pair_payload_sha256':hashlib.sha256(payloads[0]).hexdigest(),'pair_payload_base64':base64.b64encode(payloads[0]).decode(),'original_cases':cases}
    out=ROOT/'tests/fixtures/original-potential-grid.json.gz';out.write_bytes(gzip.compress(json.dumps(document,separators=(',',':')).encode(),mtime=0));print(out);print('Raw proof:',work)
if __name__=='__main__':main()
