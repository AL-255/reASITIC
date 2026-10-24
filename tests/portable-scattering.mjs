import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
import makeKernel from '../dist/asitic.js';
const math=await make(),kernel=await makeKernel(),cases=[];
for(const exponent of [-1000,-64,-1,0,64,500,1000])for(const imaginary of [false,true]){
 const s=2**exponent,z=imaginary?[0,s]:[s,0];cases.push({name:`floating ${imaginary?'reactive':'resistive'} branch 2^${exponent}`,y:[...z,...z.map(v=>-v),...z.map(v=>-v),...z],floating:true});
}
for(const s of [Number.MIN_VALUE,Number.MAX_VALUE,2**64+4096])cases.push({name:'floating branch '+s,y:[s,0,-s,0,-s,0,s,0],floating:true});
cases.push({name:'open ports',y:Array(8).fill(0)});
cases.push({name:'singular reference',y:[-.02,0,0,0,0,0,-.02,0]});
cases.push({name:'asymmetric original diagonal',y:[.25,.125,-.125,.0625,-.0625,.03125,.5,-.125]});
let state=0x53434154;const random=()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/2**32;};
for(let i=0;i<64;++i)cases.push({name:'asymmetric scale range '+i,y:Array.from({length:8},()=>((random()-.5)*2)*2**Math.floor((random()-.5)*2000))});
const physical=new Set();
for(const c of JSON.parse(readFileSync('tests/fixtures/original-two-port.json','utf8')).cases)for(const row of c.inputs){
 if(physical.has(row.y_hex))continue;physical.add(row.y_hex);const bytes=Buffer.from(row.y_hex,'hex'),y=Array.from({length:8},(_,i)=>bytes.readDoubleLE(8*i));
 if(y.every(Number.isFinite))cases.push({name:'original physical '+c.family+'/'+c.name,y});
}
const reference=JSON.parse(execFileSync('python3',['-c',`
import json,sys,math
from fractions import Fraction as F
zero=(F(0),F(0));g=F(.02);one=(F(1),F(0))
def add(a,b):return (a[0]+b[0],a[1]+b[1])
def neg(a):return (-a[0],-a[1])
def sub(a,b):return add(a,neg(b))
def mul(a,b):return (a[0]*b[0]-a[1]*b[1],a[0]*b[1]+a[1]*b[0])
def scale(a,s):return (a[0]*s,a[1]*s)
def div(a,b):
 n=b[0]**2+b[1]**2
 if not n:return (None,None)
 return ((a[0]*b[0]+a[1]*b[1])/n,(a[1]*b[0]-a[0]*b[1])/n)
def rounded(x):
 if x is None:return None
 try:r=float(x)
 except OverflowError:return None
 return r if math.isfinite(r) else None
out=[]
# JSON may spell an IEEE value as a large decimal integer. Decode it back to
# binary64 before constructing the exact rational, as the C++ input does.
for case in json.load(sys.stdin):
 y=[F(float(v)) for v in case['y']];b,d,c,a=[tuple(y[i:i+2]) for i in (0,2,4,6)]
 det=sub(mul(a,b),mul(c,d));den=add(add(det,scale(add(a,b),g)),(g*g,F(0)))
 # Expanded original polynomials; the first diagonal intentionally retains
 # ASITIC's a*a term rather than replacing it with a*b.
 n11=add(sub(mul(c,d),mul(a,a)),(g*g,F(0)))
 n22=add(add(neg(det),scale(sub(a,b),g)),(g*g,F(0)))
 s=[div(n11,den),div(scale(d,-2*g),den),div(scale(c,-2*g),den),div(n22,den)]
 if case.get('floating'):
  # Independent traveling-wave boundary for a floating series branch:
  # incident port 1 has V1=(1+r)/g, I1=1-r, V2=t/g, I2=-t;
  # I1=-I2 and I1=Ybranch*(V1-V2).
  branch=a;reflection=div((g,F(0)),add(scale(branch,2),(g,F(0))));transmission=div(scale(branch,2),add(scale(branch,2),(g,F(0))))
  assert s==[reflection,transmission,transmission,reflection]
 values=[[rounded(v) for v in z] for z in s];out.append(dict(scattering_matrix=[values[:2],values[2:]]))
print(json.dumps(out))
`],{input:JSON.stringify(cases),encoding:'utf8'}));
// Exact rationals have no signed zero. Only the mathematical oracle ignores
// zero signs; native/WASM, host-mode and export checks remain bit-sensitive.
const mathematical=v=>v===0?0:Array.isArray(v)?v.map(mathematical):v&&typeof v==='object'?Object.fromEntries(Object.entries(v).map(([k,x])=>[k,mathematical(x)])):v;
const pointer=math._malloc(64),mainPointer=kernel._malloc(136),reports=[];mkdirSync('build/portable-two-port',{recursive:true});
function probe(y){math.HEAPF64.set(y,pointer/8);return JSON.parse(math.UTF8ToString(math._probe_scattering(pointer)));}
function integrated(y,reference=false){kernel._set_reference_numerics(reference?1:0);kernel.HEAPF64.set([...y,...Array(9).fill(0)],mainPointer/8);return JSON.parse(kernel.UTF8ToString(kernel._probe_two_port_output(mainPointer,mainPointer+64,0,1e9)));}
try{
 for(const [i,c]of cases.entries()){
  const actual=probe(c.y);assert.deepEqual(mathematical(actual),mathematical(reference[i]),c.name+' exact-rational expanded formulas');
  const file='build/portable-two-port/scattering.bin',bytes=Buffer.alloc(64);c.y.forEach((v,j)=>bytes.writeDoubleLE(v,8*j));writeFileSync(file,bytes);
  assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--scattering',file],{encoding:'utf8'})),actual,c.name+' native/WASM bits');
  assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--scattering-rounding',file],{encoding:'utf8'})),{rounding_modes:4,identical:true},c.name+' host fenv');
  const main=integrated(c.y);assert.deepEqual(main.scattering_matrix,actual.scattering_matrix,c.name+' production integration');
  const numerical=main.records.s.trim().split(/\s+/).slice(1).map(Number),s=actual.scattering_matrix,ordered=[...s[0][0],...s[1][0],...s[0][1],...s[1][1]];
  numerical.forEach((v,j)=>ordered[j]===null?assert.ok(!Number.isFinite(v)):assert.equal(v,Object.is(ordered[j],-0)?-0:Number(ordered[j].toExponential(15)),c.name+' C++ record uses same evaluator'));
  if(c.floating)assert.ok(actual.scattering_matrix.flat(2).every(Number.isFinite),c.name+' floating circuit must remain finite');
  reports.push({name:c.name,exactRational:true,nativeWasmExact:true,roundingModes:4,productionIntegrated:true});
 }
 const reproduced=cases.find(c=>c.name==='floating resistive branch 2^64'),old=integrated(reproduced.y,true);assert.ok(old.scattering_matrix.flat(2).every(v=>v===null),'Historical diagnostic must retain the cancellation failure');
 assert.deepEqual(integrated(reproduced.y).scattering_matrix,reference[cases.indexOf(reproduced)].scattering_matrix,'Diagnostic arithmetic contaminated production');
 assert.ok(probe([Infinity,0,0,0,0,0,0,0]).error);assert.deepEqual(mathematical(probe(cases[0].y)),mathematical(reference[0]));
 const heap=math.HEAPF64.buffer.byteLength;for(let i=0;i<256;++i)probe(cases[i%cases.length].y);assert.equal(math.HEAPF64.buffer.byteLength,heap,'Scattering conversion leaked workspace');
 mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-scattering.json',JSON.stringify({reference:'Exact Fraction expanded original polynomials; independent traveling-wave boundary for floating branches',reports,historicalFailure:{admittance:reproduced.y,historical:old.scattering_matrix,stable:reference[cases.indexOf(reproduced)].scattering_matrix},diagnosticIsolation:true,rejectionRecovery:true,boundedHeap:true},null,2)+'\n');console.log(`Scattering: ${cases.length} exact-rational circuits, native/WASM bits, four host rounding modes, finite floating branches, integrated C++ records and historical diagnostic isolation pass`);
}finally{math._free(pointer);kernel._free(mainPointer);}
