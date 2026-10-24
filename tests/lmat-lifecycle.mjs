import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
const k=await createKernel();configureKernel(k,parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')));
const simulate=(index,frequency=1e9,cells=[1000,1000,1000,1000],limit=256)=>JSON.parse(k.UTF8ToString(k._simulate(index,frequency,...cells,limit)));
assert.match(simulate(0).error,/Generate/);
for(const [kind,label] of [[6,'planar transformer'],[7,'3D transformer']]){
  const p=[100,90,3,2,2,50,8,3,2,0,15,1.5];generateGeometry(k,kind,p);
  const before=JSON.parse(k.UTF8ToString(k._analyze(1e9)));
  for(const winding of [0,1]){
    const actual=simulate(winding);assert.ok(!actual.error,actual.error);
    const native=spawnSync('build/native-geometry',[kind,...p,1e9,1000,1000,1000,1000,winding].map(String),{encoding:'utf8',timeout:30000});assert.equal(native.status,0,native.stderr);
    assert.deepEqual(actual,JSON.parse(native.stdout),label+' winding '+winding);
  }
  assert.match(simulate(2).error,/valid winding/);
  assert.match(simulate(0,1e9,[.00001,3,.5,50]).error,/Longitudinal mesh/);
  assert.match(simulate(0,1e9,[1000,.01,.5,1000],100).error,/Cross-section mesh/);
  assert.match(simulate(0,-1).error,/positive frequency/);
  assert.deepEqual(JSON.parse(k.UTF8ToString(k._analyze(1e9))),before,'Solving or rejecting a mesh mutated the source winding');
}
generateGeometry(k,2,[40,40,5,2,1,50,8,0,-1,0,15,1]);
const expected=simulate(0,2e9,[50,3,.5,50]);for(let i=0;i<8;i++)simulate(0,2e9,[50,3,.5,50]);
const bytes=k.HEAPF64.buffer.byteLength;
for(let i=0;i<100;i++)assert.deepEqual(simulate(0,2e9,[50,3,.5,50]),expected);
assert.equal(k.HEAPF64.buffer.byteLength,bytes,'Repeated solves grew the heap after warmup');
generateGeometry(k,9,[100,90,3,2,1,50,8,3,2,0,15,1]);assert.match(simulate(0).error,/at least one metal/);
console.log('LMAT lifecycle: both transformer windings exact, invalid meshes recover, source analysis unchanged, 100 repeated solves retain bounded memory, via-only diagnosis preserved');
