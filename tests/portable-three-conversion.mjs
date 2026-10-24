import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
import makeKernel from '../dist/asitic.js';
const math=await make(),kernel=await makeKernel(),cases=[],matrix=fn=>Array.from({length:9},(_,k)=>fn(Math.floor(k/3),k%3)).flat();
for(const exponent of [-1000,-64,0,64,500,1000])for(const reactive of [false,true]){
 const s=2**exponent;cases.push({name:`diagonal ${reactive?'reactive':'resistive'} 2^${exponent}`,z:matrix((i,j)=>i===j?reactive?[0,s]:[s,0]:[0,0]),lossless:reactive});
 cases.push({name:`rank one ${reactive?'reactive':'resistive'} 2^${exponent}`,z:matrix(()=>reactive?[0,s]:[s,0]),lossless:reactive});
}
for(const s of [Number.MIN_VALUE,Number.MAX_VALUE])cases.push({name:'rank one '+s,z:matrix(()=>[s,0])});
cases.push({name:'zero impedance',z:Array(18).fill(0),lossless:true});
cases.push({name:'near reference pole',z:matrix((i,j)=>i===j?[-50,0]:[0,0])});
cases.push({name:'asymmetric ports',z:[2,.5,.25,-.125,.5,.0625,-.75,.25,3,-.5,.125,.25,.25,.5,-.5,-.125,4,.75]});
let state=0x33434f4e;const random=()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/2**32;};
for(let i=0;i<24;++i)cases.push({name:'asymmetric exponent range '+i,z:Array.from({length:18},()=>((random()-.5)*2)*2**Math.floor((random()-.5)*2000))});
for(const h of JSON.parse(readFileSync('tests/fixtures/original-three-port-backing.json','utf8')).histories){const b=Buffer.from(h.expected_impedance_hex,'hex'),z=Array.from({length:18},(_,i)=>b.readDoubleLE(i*8));if(z.every(Number.isFinite))cases.push({name:'captured '+h.case.name,z});}
const reference=JSON.parse(execFileSync('python3',['-c',`
import json,sys,math
from fractions import Fraction as F
zero=(F(0),F(0));one=(F(1),F(0));g=F(.02)
def add(a,b):return (a[0]+b[0],a[1]+b[1])
def neg(a):return (-a[0],-a[1])
def sub(a,b):return add(a,neg(b))
def mul(a,b):return (a[0]*b[0]-a[1]*b[1],a[0]*b[1]+a[1]*b[0])
def scale(a,s):return (a[0]*s,a[1]*s)
def div(a,b):
 n=b[0]**2+b[1]**2
 return ((a[0]*b[0]+a[1]*b[1])/n,(a[1]*b[0]-a[0]*b[1])/n)
def solve(A,B):
 a=[list(A[i])+list(B[i]) for i in range(3)]
 for k in range(3):
  pivot=next((i for i in range(k,3) if a[i][k]!=zero),None)
  if pivot is None:return None
  a[k],a[pivot]=a[pivot],a[k];p=a[k][k];a[k]=[div(x,p) for x in a[k]]
  for i in range(3):
   if i!=k:
    factor=a[i][k];a[i]=[sub(a[i][j],mul(factor,a[k][j])) for j in range(6)]
 return [row[3:] for row in a]
def rounded(x):
 try:v=float(x)
 except OverflowError:return None
 return v if math.isfinite(v) else None
out=[]
for case in json.load(sys.stdin):
 # Restore IEEE binary64 semantics for JSON's large integer spellings.
 v=[F(float(x)) for x in case['z']];Z=[[tuple(v[2*(3*i+j):2*(3*i+j)+2]) for j in range(3)] for i in range(3)];I=[[one if i==j else zero for j in range(3)] for i in range(3)]
 Y=solve(Z,I)
 # Traveling waves: V=(a+b)/g and I=a-b. V=Z*I implies
 # (I+gZ)*b=(gZ-I)*a. Solve for all three independent incident waves.
 plus=[[add(I[i][j],scale(Z[i][j],g)) for j in range(3)] for i in range(3)];minus=[[sub(scale(Z[i][j],g),I[i][j]) for j in range(3)] for i in range(3)];S=solve(plus,minus)
 if case.get('lossless') and S is not None:
  for i in range(3):
   for j in range(3):
    gram=zero
    for k in range(3):gram=add(gram,mul(S[i][k],(S[j][k][0],-S[j][k][1])))
    assert gram==I[i][j]
 order=[1,0,2]
 def output(M):return {'matrix':[[[None,None] if M is None else [rounded(x) for x in M[i][j]] for j in order] for i in order]}
 out.append([output(Y),output(S)])
print(json.dumps(out))
`],{input:JSON.stringify(cases),encoding:'utf8',maxBuffer:8*1024*1024}));
const mathematical=v=>v===0?0:Array.isArray(v)?v.map(mathematical):v&&typeof v==='object'?Object.fromEntries(Object.entries(v).map(([k,x])=>[k,mathematical(x)])):v,p=math._malloc(144),q=kernel._malloc(144),reports=[];
function direct(z,mode){math.HEAPF64.set(z,p/8);return JSON.parse(math.UTF8ToString(math._probe_three_conversion(p,mode)));}
function integrated(z,mode,historical=false){kernel._set_reference_numerics(historical?1:0);kernel.HEAPF64.set(z,q/8);return JSON.parse(kernel.UTF8ToString(kernel._probe_three_port_conversion(q,mode)));}
mkdirSync('build/portable-three-conversion',{recursive:true});
try{
 for(const [i,c]of cases.entries())for(const mode of [0,1]){
  const actual=direct(c.z,mode);assert.deepEqual(mathematical(actual),mathematical(reference[i][mode]),c.name+' exact Gaussian wave/voltage solve');
  const file='build/portable-three-conversion/matrix.bin',bytes=Buffer.alloc(144);c.z.forEach((v,j)=>bytes.writeDoubleLE(v,8*j));writeFileSync(file,bytes);
  assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--three-conversion',mode,file].map(String),{encoding:'utf8'})),actual,c.name+' native/WASM bits');
  assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--three-conversion-rounding',mode,file].map(String),{encoding:'utf8'})),{rounding_modes:4,identical:true},c.name+' host fenv');
  assert.deepEqual(integrated(c.z,mode),actual,c.name+' production conversion');reports.push({name:c.name,mode:mode?'S':'Y',exactRational:true,nativeWasmExact:true,roundingModes:4});
 }
 const rank=cases.find(c=>c.name==='rank one resistive 2^64'),old=integrated(rank.z,1,true),stable=integrated(rank.z,1);assert.notDeepEqual(old,stable,'Historical rounded identity unexpectedly retained');assert.ok(stable.matrix.flat(2).every(Number.isFinite));assert.deepEqual(integrated(rank.z,1),stable,'Historical mode contaminated production');
 const file='build/portable-three-conversion/history.bin',bytes=Buffer.alloc(144);rank.z.forEach((v,i)=>bytes.writeDoubleLE(v,8*i));writeFileSync(file,bytes);assert.deepEqual(JSON.parse(execFileSync('build/native-geometry',['--three-conversion','1',file],{encoding:'utf8'})),old,'Historical native/WASM cancellation reproduction');
 assert.ok(direct(Array(18).fill(Infinity),1).error);assert.deepEqual(mathematical(direct(cases[0].z,1)),mathematical(reference[0][1]));const heap=math.HEAPF64.buffer.byteLength;for(let i=0;i<128;++i)direct(cases[i%cases.length].z,i%2);assert.equal(math.HEAPF64.buffer.byteLength,heap);
 mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-three-conversion.json',JSON.stringify({reference:'Exact Fraction Gaussian elimination of voltage/current and traveling-wave boundaries; exact unitary test for lossless inputs',reports,historicalFailure:{impedance:rank.z,historical:old.matrix,stable:stable.matrix},rejectionRecovery:true,boundedHeap:true},null,2)+'\n');console.log(`Three-port conversion: ${cases.length} circuits / ${reports.length} S and Y reductions, exact rational wave/voltage equations, native/WASM bits, four rounding modes and original rank-one failure isolation pass`);
}finally{math._free(p);kernel._free(q);}
