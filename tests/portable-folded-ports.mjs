import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
const k=await make(),python=process.env.MPMATH_PYTHON??'python3';mkdirSync('build/portable-two-port',{recursive:true});
const columnMajor=m=>m[0].flatMap((_,j)=>m.flatMap(row=>row[j]));
function probe(cap,y,n,pads=null,padSize=2){const values=[...cap,...y,...(pads??[])],p=k._malloc(values.length*8);try{k.HEAPF64.set(values,p/8);return JSON.parse(k.UTF8ToString(k._probe_folded_ports(p,p+cap.length*8,n,pads?p+(cap.length+y.length)*8:0,pads?padSize:0)));}finally{k._free(p);}}
function host(cap,y,n,pads=null,padSize=2){const values=[...cap,...y,...(pads??[])],bytes=Buffer.alloc(8+values.length*8);bytes.writeInt32LE(n);bytes.writeInt32LE(pads?padSize:0,4);values.forEach((v,i)=>bytes.writeDoubleLE(v,8+i*8));writeFileSync('build/portable-two-port/folded.bin',bytes);return JSON.parse(execFileSync('build/portable-math/gmd-native',['--folded-ports','build/portable-two-port/folded.bin'],{encoding:'utf8'}));}
const single=probe([.125,.25],[1024,-512],1);assert.deepEqual(single,{matrix:[[[1024,-512],[-1024,512]],[[-1024,512],[1024.125,-511.75]]],node_capacitance_matrix:[[[.125,.25]]]});assert.deepEqual(single,host([.125,.25],[1024,-512],1));
// Subtracting the rounded with-pad Y22 loses the 400-bit-smaller real term.
// Exact nodal assembly and unrounded pad subtraction preserve it.
const cap=[2**-400,2**-30],y=[1024,0],pads=[1024,0,0,0,0,0,1024,0],cancel=probe(cap,y,1,pads);
assert.deepEqual(cancel,host(cap,y,1,pads));assert.deepEqual(cancel.matrix[1][1],[2**-400,2**-30]);assert.equal(cancel.with_pad_admittance_matrix[1][1][0]-1024,0);
assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--folded-rounding','build/portable-two-port/folded.bin'],{encoding:'utf8'})),{rounding_modes:4,identical:true});
const tiny=Number.MIN_VALUE,subnormal=probe([tiny,0],[tiny,0],1);assert.deepEqual(subnormal,host([tiny,0],[tiny,0],1));assert.deepEqual(subnormal.matrix,[[[tiny,0],[-tiny,0]],[[-tiny,0],[2*tiny,0]]]);
assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--folded-rounding','build/portable-two-port/folded.bin'],{encoding:'utf8'})),{rounding_modes:4,identical:true});
const rational=JSON.parse(execFileSync('python3',['-c',`from fractions import Fraction as F
import json
a,b,c,d=F(1024),F(512),F(1,2**100),F(1,2**30);u=a+b+c;den=u*u+d*d
pair=lambda x,y:[float(x-y*u/den),float(y*d/den)]
print(json.dumps([[pair(b,b*b),pair(0,a*b)],[pair(0,a*b),pair(a,a*a)]]))`],{encoding:'utf8'}));
const two=probe([0,0,0,0,0,0,2**-100,2**-30],[1024,0,0,0,0,0,512,0],2);assert.deepEqual(two.matrix,rational);
const circuits=[
  {cap:[[[1/16,1/8],[-1/128,-1/64]],[[-1/128,-1/64],[1/8,1/4]]],y:[[[1,-1/4],[1/8,-1/16]],[[1/8,-1/16],[2,-1/2]]]},
  {cap:[[[1/16,1/8],[-1/128,-1/64]],[[-1/128,-1/64],[1/8,1/4]]],y:[[[1,-1/4],[1/8,-1/16]],[[1/8,-1/16],[2,-1/2]]],pads:[[[1/128,1/64],[1/256,1/512]],[[1/256,1/512],[1/64,1/128]]]},
  {cap:[[[1/16,1/8],[-1/128,-1/64],[-1/256,-1/128]],[[-1/128,-1/64],[1/8,1/4],[-1/64,-1/128]],[[-1/256,-1/128],[-1/64,-1/128],[1/4,1/2]]],y:[[[1,-1/4],[1/8,-1/16],[1/32,-1/64]],[[1/8,-1/16],[2,-1/2],[1/16,-1/32]],[[1/32,-1/64],[1/16,-1/32],[3,-1]]]},
  // The numerical input may retain small asymmetries from its earlier solve;
  // solve both drives instead of silently copying the off-diagonal port cell.
  {cap:[[[1/16,1/8],[-1/128,-1/64]],[[-1/256,-1/128],[1/8,1/4]]],y:[[[1,-1/4],[1/8,-1/16]],[[1/16,-1/32],[2,-1/2]]]}
];
let checks=4,maxError=0;
for(const c of circuits){
  const n=c.cap.length,base=probe(columnMajor(c.cap),columnMajor(c.y),n,c.pads?columnMajor(c.pads):null);assert.ok(!base.error,base.error);
  for(const exponent of [-900,-450,0,450,900]){
    const scale=2**exponent,scaled=m=>m.map(row=>row.map(z=>z.map(v=>v*scale))),cap=scaled(c.cap),y=scaled(c.y),pads=c.pads?scaled(c.pads):undefined;
    const actual=probe(columnMajor(cap),columnMajor(y),n,pads?columnMajor(pads):null);assert.deepEqual(actual,host(columnMajor(cap),columnMajor(y),n,pads?columnMajor(pads):null));
    assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--folded-rounding','build/portable-two-port/folded.bin'],{encoding:'utf8'})),{rounding_modes:4,identical:true});
    for(const key of Object.keys(base))assert.deepEqual(actual[key].map(row=>row.map(z=>z.map(v=>v/scale))),base[key],'Admittance-unit scaling changed '+key);
    const reference=JSON.parse(execFileSync(python,['tools/reference_two_port.py','--nodes-only'],{input:JSON.stringify({capacitance_matrix:cap,inductance_admittance_matrix:y,...(pads?{pad_capacitance_matrix:pads}:{}),reference_decimal_digits:400}),encoding:'utf8'}));
    for(const key of Object.keys(base)){const a=actual[key].flat(2),b=reference[key].flat(2),norm=Math.max(...b.map(Math.abs),1e-300),error=Math.max(...a.map((v,i)=>Math.abs(v-b[i])))/norm;assert.ok(error<=2e-15,key+' independent norm error '+error);maxError=Math.max(maxError,error);}
    checks++;
  }
}
const zero=probe([0,0],[0,0],1);assert.deepEqual(zero.matrix,[[[0,0],[0,0]],[[0,0],[0,0]]]);
for(const [cap,y,n]of [[[NaN,0],[1,0],1],[[1,0],[Infinity,0],1],[[1,0],[1,0],0],[[1,0],[1,0],1025],[Array(8).fill(0),Array(8).fill(0),2]])assert.ok(probe(cap,y,n).error);
assert.ok(probe([1,0],[1,0],1,[NaN,0,0,0,0,0,1,0]).error);assert.deepEqual(probe([.125,.25],[1024,-512],1),single,'Rejected solve contaminated recovery');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-folded-ports.json',JSON.stringify({reference:'Exact one-branch and rational two-branch limits; independent 400-digit vertex elimination',checks,scaleExponents:[-900,-450,0,450,900],maxError,padCancellationComponentBits:400,nativeWasmBitsIdentical:true,hostRoundingModes:4,rejectionRecovery:true},null,2)+'\n');
console.log(`Folded ports: ${checks} analytic/scaled circuits, 1800-bit unit range, 400-bit pad cancellation, native/WASM bits, four rounding modes and recovery pass; maximum norm error ${maxError}`);
