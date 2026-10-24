import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
const k=await make();mkdirSync('build/portable-two-port',{recursive:true});
function probe(cap,ind,n=2){
  const values=[...cap,...ind],p=k._malloc(values.length*8);
  try{k.HEAPF64.set(values,p/8);return JSON.parse(k.UTF8ToString(k._probe_pi_shunts(p,p+cap.length*8,n,0,0)));}finally{k._free(p);}
}
function host(cap,ind,n=2){
  const values=[...cap,...ind],bytes=Buffer.alloc(8+values.length*8);bytes.writeInt32LE(n);values.forEach((v,i)=>bytes.writeDoubleLE(v,8+i*8));writeFileSync('build/portable-two-port/analytic.bin',bytes);
  return JSON.parse(execFileSync('build/portable-math/gmd-native',['--shunts','build/portable-two-port/analytic.bin'],{encoding:'utf8'}));
}
// Two resistive branches (1024 S and 512 S) meet at a capacitor with
// C-admittance 2^-100 + j*2^-30 S. The endpoint row sums are A*C/(A+B+C)
// and B*C/(A+B+C). Exact rational arithmetic supplies independent references.
const reference=JSON.parse(execFileSync('python3',['-c',`
import json
from fractions import Fraction as F
a,b,c,d=F(1024),F(512),F(1,2**100),F(1,2**30)
s=a+b+c;den=s*s+d*d
pair=lambda x:[float(x*(c*s+d*d)/den),float(x*d*(a+b)/den)]
print(json.dumps(dict(first=pair(b),second=pair(a),rounded_near=float(a-a*a*s/den),rounded_cross=float(-a*b*s/den))))
`],{encoding:'utf8'}));
const cap=[0,0,0,0,0,0,2**-100,2**-30],ind=[1024,0,0,0,0,0,512,0];
const result=probe(cap,ind);assert.ok(!result.error,result.error);assert.deepEqual(result,host(cap,ind));
for(const key of ['first','second'])assert.deepEqual(result[key],reference[key],'Exact rational common-mode '+key);
assert.equal(reference.rounded_near+reference.rounded_cross,0,'Analytic case no longer demonstrates cancellation in rounded Y');
assert.ok(result.second[0]>0&&result.second[0]<1e-20,'Small physical conductance was lost');
const zero=Array(8).fill(0);assert.deepEqual(probe(zero,ind).first,[0,0]);assert.deepEqual(probe(zero,ind).second,[0,0]);
assert.ok(probe(zero,zero).error);assert.ok(probe(cap,ind,0).error);assert.ok(probe(cap,ind,1025).error);assert.ok(probe([...cap.slice(0,7),Infinity],ind).error);
assert.deepEqual(probe(cap,ind),result,'Rejected solve contaminated following arithmetic');
const single=probe([.125,.25],[1024,-512],1);assert.deepEqual(single.first,[0,0]);assert.deepEqual(single.second,[.125,.25]);assert.deepEqual(single,host([.125,.25],[1024,-512],1));
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-pi-shunts.json',JSON.stringify({reference:'Exact rational two-resistor/one-capacitor circuit',roundedPortConductance:reference.rounded_near+reference.rounded_cross,commonMode:result,nativeWasmBitsIdentical:true,rejectionRecovery:true},null,2)+'\n');
console.log('Common-mode PI shunts: exact rational tiny-conductance circuit, native/WASM bit identity, one-segment/zero-capacitance limits, singular/nonfinite guards and recovery pass');
