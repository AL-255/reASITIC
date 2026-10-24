import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
const kernel=await createKernel(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));configureKernel(kernel,technology);
let fields=0,differences=0,maxRelative=0;const report=[];
for(const [name,kind] of [['square',0],['polygon',1],['wire',2],['symmetric-square',3],['symmetric-polygon',4],['ring',5],['transformer',6],['3d-transformer',7],['multimetal',8],['via',9]]){
  const parameters=[100,90,3,kind===5?20:2,2,50,8,3,2,0,15,1.5];generateGeometry(kernel,kind,parameters);
  for(const frequency of [1e6,1e8,1e9,1e10,1e11]){
    const native=spawnSync('build/native-geometry',[kind,...parameters,frequency].map(String),{encoding:'utf8',timeout:20000});assert.equal(native.status,0,native.stderr);
    const expected=JSON.parse(native.stdout),actual=JSON.parse(kernel.UTF8ToString(kernel._analyze(frequency)));assert.ok(!actual.error,`${name}: ${actual.error}`);
    const errors=[];expected.windings.forEach((w,i)=>{for(const key of ['inductance_nh','resistance_hf']){const a=w[key],b=actual.windings[i][key];fields++;if(!Object.is(a,b)){const relative=Math.abs(a-b)/Math.max(1e-30,Math.abs(a));maxRelative=Math.max(maxRelative,relative);differences++;errors.push({key,native:a,wasm:b,relative});assert.ok(relative<=1e-12,`${name} ${key}: ${a} vs ${b}`);}}});
    report.push({name,frequency,errors});
  }
}
writeFileSync('build/evidence/estimates.json',JSON.stringify({fields,differences,maxRelative,cases:report},null,2)+'\n');
console.log(`Original INDUCTANCE / RESISHF: ${fields} fields over 50 cases; ${differences} differing fields; maximum relative difference ${maxRelative}`);
assert.equal(differences,0,'Original conductor-estimate parity regressed');
