import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
const math=await make(),folder='build/portable-two-port';mkdirSync(folder,{recursive:true});
const cases=[
  {name:'floating series RL',v:[.25,-2,0,0,0,0]},
  {name:'one tiny shunt',v:[1024,-512,2**-600,2**-300,0,0]},
  {name:'two tiny shunts',v:[1024,-512,2**-400,2**-300,2**-401,2**-301]},
  {name:'unequal 800-bit shunts',v:[2**-400,-(2**-500),2**400,2**400,2**-400,2**-400]},
  {name:'small reactive series component',v:[1,-(2**-400),0,2**-399,0,0]},
  {name:'lossless series',v:[0,-2,0,0,0,0]},
  {name:'canceling shunts',v:[1,-2,0,.5,0,-.5]},
  {name:'exact grounded resonance',v:[0,-2,0,2,.25,.5]},
  {name:'smallest binary64 series conductance',v:[Number.MIN_VALUE,0,0,0,0,0]},
];
for(const exponent of [-900,-450,0,450,900])cases.push({name:'uniform admittance scale '+exponent,v:[.25,-.5,2**-40,2**-20,2**-42,2**-21].map(v=>v*2**exponent)});
assert.equal(1024+2**-400,1024,'Tiny-shunt case must demonstrate lost rounded Y information');
// Independent exact-rational 2x2 nodal matrix inversion. No branch-reduced
// denominator or software floating-point solver is used by this reference.
const reference=JSON.parse(execFileSync('python3',['-c',`
import json,sys,math
from fractions import Fraction as F
zero=(F(0),F(0));one=(F(1),F(0))
def add(a,b):return (a[0]+b[0],a[1]+b[1])
def neg(a):return (-a[0],-a[1])
def sub(a,b):return add(a,neg(b))
def mul(a,b):return (a[0]*b[0]-a[1]*b[1],a[0]*b[1]+a[1]*b[0])
def div(a,b):
 d=b[0]**2+b[1]**2
 if not d:return (None,None)
 return ((a[0]*b[0]+a[1]*b[1])/d,(a[1]*b[0]-a[0]*b[1])/d)
def rounded(x):
 if x is None:return None
 try:v=float(x)
 except OverflowError:return None
 return v if math.isfinite(v) else None
out=[]
for case in json.load(sys.stdin):
 v=list(map(F,case['v']));s,f,n=[tuple(v[i:i+2]) for i in (0,2,4)]
 a,b,c=add(s,f),neg(s),add(s,n)
 det=sub(mul(a,c),mul(b,b))
 if f==n==zero:diff=div(one,s)
 else:diff=div(add(add(a,c),add(b,b)),det)
 quality=None if diff[0] in (None,0) or diff[1] is None else rounded(diff[1]/diff[0])
 out.append(dict(input_impedance=[[rounded(x) for x in z] for z in [div(one,c),div(one,a),diff]],quality_differential=quality))
print(json.dumps(out))
`],{input:JSON.stringify(cases),encoding:'utf8'}));
function probe(values){const p=math._malloc(48);try{math.HEAPF64.set(values,p/8);return JSON.parse(math.UTF8ToString(math._probe_pi_impedance(p)));}finally{math._free(p);}}
const reports=[];
for(const [index,c] of cases.entries()){
  const file=folder+'/pi-impedance.bin',bytes=Buffer.alloc(48);c.v.forEach((v,i)=>bytes.writeDoubleLE(v,i*8));writeFileSync(file,bytes);
  const actual=probe(c.v),native=JSON.parse(execFileSync('build/portable-math/gmd-native',['--pi-impedance',file],{encoding:'utf8'}));
  assert.deepEqual(actual,native,c.name+' native/WASM bits');assert.deepEqual(actual,reference[index],c.name+' exact rational nodal inverse');
  assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--pi-impedance-rounding',file],{encoding:'utf8'})),{rounding_modes:4,identical:true},c.name+' host fenv');
  reports.push({name:c.name,exactRational:true,nativeWasmBitsIdentical:true,roundingModes:4});
}
assert.ok(probe([Infinity,0,0,0,0,0]).error);assert.ok(probe([1,0,NaN,0,0,0]).error);
assert.deepEqual(probe(cases[0].v),reference[0],'Recovery after invalid inputs');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-pi-impedance.json',JSON.stringify({reference:'Exact Fraction nodal 2x2 inverse; floating RL circuit limit',reports,rejectionRecovery:true},null,2)+'\n');
console.log(`Pi branch impedance: ${cases.length} exact-rational circuits, native/WASM bits and four host rounding modes pass`);
