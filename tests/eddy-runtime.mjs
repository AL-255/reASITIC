import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdirSync,writeFileSync} from 'node:fs';
import createKernel from '../dist/asitic.js';
const kernel=await createKernel(),args=value=>Object.is(value,-0)?'-0':String(value),values=[-0,0,5e-324,-5e-324,1e-300,-1e-300,1e-20,-1e-20,.1,-.1,.126,-.126,.85546875,-.85546875,Math.PI/2,-Math.PI/2,2.426264,-2.426264,Math.PI,-Math.PI,1e3,-1e3,1e8,-1e8,105414350,-105414350,1e100,-1e100,1e300,-1e300,Number.MAX_VALUE,-Number.MAX_VALUE,Infinity,-Infinity,NaN];
let state=0x510e527f;const random=()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/2**32;};
for(let i=0;i<512;++i){values.push((random()-.5)*20000);values.push((random()<.5?-1:1)*2**((random()-.5)*2000));}
const failures=[];let trigCases=0;
for(const x of values)for(const cosine of [0,1]) {
  const process=spawnSync('build/native-geometry',['--trig',cosine,args(x)].map(String),{encoding:'utf8'});assert.equal(process.status,0,process.stderr);
  const native=JSON.parse(process.stdout),actual=JSON.parse(kernel.UTF8ToString(kernel._probe_runtime_trig(cosine,x)));trigCases++;
  try{assert.deepEqual(actual,native);}catch{failures.push({kind:'glibc trig',cosine,input:args(x),native,wasm:actual});}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/eddy-runtime-trig.json',JSON.stringify({trigCases,differences:failures.length,failures},null,2)+'\n');
console.log(`Eddy runtime: ${trigCases} actual i386 libm sin/cos cases; ${failures.length} differences`);
assert.equal(failures.length,0,JSON.stringify(failures.slice(0,8),null,2));
