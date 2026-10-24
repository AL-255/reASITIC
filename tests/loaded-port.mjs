import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
const math=await make(),cases=[],y=[.5,.25,-.125,.0625,-.25,.03125,1,-.125];
for(const port of [1,2])for(const load of [[50,0],[50,10],[0,0],[0,-2],[-.5,2]])for(const caps of [[0,0],[1e-12,2e-12]])cases.push({name:`port ${port}, load ${load}, C ${caps}`,v:[...y,...load,port,1e9,...caps]});
for(const exponent of [-1000,-500,0,500,1000])cases.push({name:'scale '+exponent,v:[...y.map(n=>n*2**exponent),50*2**-exponent,0,1,1e9,0,0]});
for(const [name,values,load]of [
 ['subnormal finite impedance',[1,0,0,0,0,0,Number.MAX_VALUE,0],[0,0]],
 ['overflow impedance',[1,0,0,0,0,0,Number.MIN_VALUE,0],[0,0]],
 ['open pole',Array(8).fill(0),[50,0]],
 ['indeterminate active termination',[1,0,0,0,0,0,1,0],[-1,0]],
 ['strong determinant cancellation',[1,0,1,0,1,0,1+Number.EPSILON,0],[2**1000,0]],
])cases.push({name,v:[...values,...load,1,1e9,0,0]});
const reference=JSON.parse(execFileSync('python3',['-c',`
import json,sys,math
from fractions import Fraction as F
zero=(F(0),F(0));one=(F(1),F(0))
def add(a,b):return (a[0]+b[0],a[1]+b[1])
def neg(a):return (-a[0],-a[1])
def sub(a,b):return add(a,neg(b))
def mul(a,b):return (a[0]*b[0]-a[1]*b[1],a[0]*b[1]+a[1]*b[0])
def div(a,b):
 n=b[0]**2+b[1]**2
 return ((a[0]*b[0]+a[1]*b[1])/n,(a[1]*b[0]-a[0]*b[1])/n)
def rounded(a):
 try:x=float(a)
 except OverflowError:return None
 return x if math.isfinite(x) else None
out=[]
for case in json.load(sys.stdin):
 v=list(map(F,case['v']));p=int(v[10]);z=tuple(v[8:10]);omega=v[11]*F(float.fromhex('0x1.921fb54442d18p+2'))
 Y=[[tuple(v[0:2]),tuple(v[4:6])],[tuple(v[2:4]),tuple(v[6:8])]]
 Y[0][0]=add(Y[0][0],(F(0),omega*v[13]));Y[1][1]=add(Y[1][1],(F(0),omega*v[12]))
 d=2-p;o=1-d
 # Unit current at the driven endpoint. The load constraint is Vother
 # + Zload * Iother = 0. Eliminate Vdrive with exact Gaussian row operations.
 a,b=Y[d][d],Y[d][o];c,e=mul(z,Y[o][d]),add(one,mul(z,Y[o][o]))
 determinant=sub(mul(a,e),mul(b,c))
 if determinant==zero:out.append(dict(status='indeterminate' if e==zero else 'pole',impedance_ohm=[None,None]));continue
 if a!=zero:
  factor=div(c,a);remaining=sub(e,mul(factor,b));other=div(neg(factor),remaining);drive=div(sub(one,mul(b,other)),a)
 else:
  other=div(one,b);drive=div(neg(mul(e,other)),c)
 impedance=list(map(rounded,drive));out.append(dict(status='finite' if all(x is not None for x in impedance) else 'overflow',impedance_ohm=impedance))
print(json.dumps(out))
`],{input:JSON.stringify(cases),encoding:'utf8'}));
const probe=v=>{const p=math._malloc(64);try{math.HEAPF64.set(v.slice(0,8),p/8);return JSON.parse(math.UTF8ToString(math._loaded_port_impedance(p,...v.slice(8))));}finally{math._free(p);}};
mkdirSync('build/portable-two-port',{recursive:true});const reports=[];
for(const [i,c]of cases.entries()){
 const actual=probe(c.v);assert.deepEqual(actual,reference[i],c.name+' exact-rational nodal equations');
 const file='build/portable-two-port/loaded-port.bin',bytes=Buffer.alloc(112);c.v.forEach((v,j)=>bytes.writeDoubleLE(v,8*j));writeFileSync(file,bytes);
 assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--loaded-port',file],{encoding:'utf8'})),actual,c.name+' native/WASM bits');
 assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--loaded-port-rounding',file],{encoding:'utf8'})),{rounding_modes:4,identical:true},c.name+' host fenv');
 reports.push({name:c.name,exactRational:true,nativeWasmExact:true,roundingModes:4});
}
for(const index of [0,8,9,11,12,13]){const v=[...cases[0].v];v[index]=Infinity;assert.ok(probe(v).error);}
assert.deepEqual(probe(cases[0].v),reference[0]);mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/loaded-port.json',JSON.stringify({reference:'Exact Fraction nodal elimination with complex load boundary; IEEE binary64 final rounding',reports,invalidInputRecovery:true},null,2)+'\n');console.log(`Loaded port: ${cases.length} exact-rational circuits, native/WASM bits, short-circuit limits, poles, subnormals and four host rounding modes pass`);
