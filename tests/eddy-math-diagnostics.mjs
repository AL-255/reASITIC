// Fidelity measurement, not an assertion of universal microcode identity.
// --strict fails on every observed difference and remains a completion check.
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdirSync,writeFileSync} from 'node:fs';
import createKernel from '../dist/asitic.js';
const kernel=await createKernel(),args=value=>Object.is(value,-0)?'-0':String(value);
let state=0x510e527f;const random=()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/2**32;};
for(let i=0;i<512*3;++i)random();
const powers=[-0,0,5e-324,1e-300,1e-200,1e-20,.001,.1,.7,.7071067811865476,1,1+2**-52,1-2**-53,1.29,2,100,1e100,1e200,Infinity,-1,NaN];for(let i=0;i<512;++i)powers.push(2**((random()-.5)*1800));
const differences=[];
for(const x of powers){
  const process=spawnSync('build/native-geometry',['--math',9,args(x),0,0].map(String),{encoding:'utf8'});assert.equal(process.status,0,process.stderr);
  const native=JSON.parse(process.stdout),wasm=JSON.parse(kernel.UTF8ToString(kernel._probe_math(9,x,0,0)));
  assert.equal(Boolean(wasm.error),Boolean(native.error),'Error-power nonfinite classification');
  try{assert.deepEqual(wasm,native);}catch{differences.push({input:args(x),native,wasm});}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/eddy-math-diagnostics.json',JSON.stringify({scope:'Hardware FYL2X/FYL2XP1/F2XM1 error-power composition; unresolved differences prevent universal identity',cases:powers.length,differences},null,2)+'\n');
console.log(`UNRESOLVED hardware error-power diagnostic: ${powers.length} cases; ${differences.length} finite differences. These are not waived as full-goal completion.`);
if(process.argv.includes('--strict'))assert.equal(differences.length,0,JSON.stringify(differences,null,2));
