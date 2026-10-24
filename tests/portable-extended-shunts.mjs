import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
const k=await make();mkdirSync('build/portable-two-port',{recursive:true});
function probe(cap,z,n){const p=k._malloc((cap.length+z.length)*8);try{k.HEAPF64.set([...cap,...z],p/8);return JSON.parse(k.UTF8ToString(k._probe_extended_pi_shunts(p,p+cap.length*8,n)));}finally{k._free(p);}}
function host(cap,z,n){const values=[...cap,...z],bytes=Buffer.alloc(8+values.length*8);bytes.writeInt32LE(n);values.forEach((v,i)=>bytes.writeDoubleLE(v,8+i*8));writeFileSync('build/portable-two-port/extended-analytic.bin',bytes);return JSON.parse(execFileSync('build/portable-math/gmd-native',['--extended-shunts','build/portable-two-port/extended-analytic.bin'],{encoding:'utf8'}));}
// One midpoint capacitor splits equally between both endpoints regardless of
// nonzero branch impedance. Preserve 400 bits of component magnitude range.
const singleCap=[2**-400,2**-30],singleZ=[.1625,.25],single=probe(singleCap,singleZ,1);
assert.deepEqual(single,host(singleCap,singleZ,1));for(const key of ['first','second'])assert.deepEqual(single[key],[2**-401,2**-31]);
for(const z of [[1,0],[0,1],[-.5,.75],[1e-200,1e200]])assert.deepEqual(probe(singleCap,z,1),single,'One-branch cancellation depends on impedance');
// Two identical midpoint capacitors c and unit series impedances: symmetry
// gives I0=-I2=c/(1+c/4), while the interior chain current is zero.
const expected=JSON.parse(execFileSync('python3',['-c',`from fractions import Fraction as F
import json
a,b=F(1,16),F(1,8);u,v=1+a/4,b/4;den=u*u+v*v
print(json.dumps([float((a*u+b*v)/den),float((b*u-a*v)/den)]))`],{encoding:'utf8'}));
const cap=[1/16,1/8,0,0,0,0,1/16,1/8],z=[1,0,0,0,0,0,1,0],two=probe(cap,z,2);
assert.deepEqual(two,host(cap,z,2));for(const key of ['first','second'])assert.deepEqual(two[key],expected,'Exact rational midpoint circuit');
assert.deepEqual(probe(Array(8).fill(0),z,2).first,[0,0]);
assert.ok(probe(cap,Array(8).fill(0),2).error);assert.ok(probe(cap,z,0).error);assert.ok(probe(cap,z,1025).error);assert.ok(probe([NaN,...cap.slice(1)],z,2).error);assert.ok(probe([1,0],[0,0],1).error);
assert.deepEqual(probe(cap,z,2),two,'Failed extended solve contaminated the next request');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-extended-shunts.json',JSON.stringify({reference:'Exact one-midpoint limit and rational symmetric two-midpoint circuit',single,two,nativeWasmBitsIdentical:true,rejectionRecovery:true},null,2)+'\n');
console.log('PIX common-mode shunts: exact midpoint circuits, 400-bit component range, impedance independence, native/WASM bit identity and invalid/singular recovery pass');
