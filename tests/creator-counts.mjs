import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry,generateGeometry,editGeometry} from '../web/technology.mjs';
import {optimizationPlan,createOptimizationCandidate} from '../web/optimization.mjs';
const k=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));configureKernel(k,technology);
const original=JSON.parse(readFileSync('tests/fixtures/original-creator-counts.json')).cases.find(c=>c.name==='cmos-polygon-512').shapes[0];
const physical=s=>({origin:s.origin,segments:s.segments.map(({metal,width,thickness,points,nx,ny})=>({metal,width,points,...(metal<technology.metals.length?{thickness}:{nx,ny})}))});
const parameters=[200,200,3,2,1.25,100,512,3,-1,0,15,1];
const legacy=generateGeometry(k,1,parameters,{x:256,y:256});assert.deepEqual(physical(legacy.shapes[0]),physical(original),'Legacy high-side original polygon');
const keywords=JSON.parse(readFileSync('tests/fixtures/optimization-cases.json')).cases[0].keywords.replace('SIDES=4','SIDES=512');
const plan=optimizationPlan(k,keywords,technology.domain,{candidate_limit:1000});assert.ok(plan.candidates.length>0);assert.ok(plan.candidates.every(c=>c[1]===512));
assert.deepEqual(plan,JSON.parse(execFileSync('build/native-geometry',['--grid',keywords,'1000'],{encoding:'utf8'})),'Original native/WASM high-side grid enumeration');
const descriptor=plan.candidates[0],candidate=createOptimizationCandidate(k,'OPT',descriptor),direct=createGeometry(k,1,`NAME=A:RAD=${descriptor[4]}:W=${descriptor[5]}:S=${descriptor[6]}:N=${descriptor[7]}:SIDES=512:METAL=M4:XORG=${descriptor[8]}:YORG=${descriptor[9]}`);
assert.deepEqual(physical(candidate.shapes[0]),physical(direct.shapes[0]),'Optimizer high-side original creator identity');
const keep=createGeometry(k,2,'NAME=KEEP:LEN=20:WID=3:METAL=M1'),clean=({warnings,...rest})=>rest;
for(const [id,line]of [[1,'NAME=HUGE:RAD=100:W=3:S=2:N=1:SIDES=1000000:METAL=M4'],[22,'NAME=HUGE:RAD=100:W=3:GAP=37:SIDES=1000000:METAL=M4']]){
  assert.throws(()=>createGeometry(k,id,line,'',{append:true}),/record budget/);assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(keep),'Oversized side count damaged source');
}
assert.throws(()=>generateGeometry(k,1,[...parameters.slice(0,6),1e300,...parameters.slice(7)]),/Invalid/);
assert.throws(()=>generateGeometry(k,5,[200,200,3,37,1,100,1000000,3,-1,0,15,1]),/record budget/);
assert.throws(()=>createGeometry(k,14,'NAME=A:NX=0:NY=300:VIA=VIA2'),/arguments/);
assert.throws(()=>createGeometry(k,14,'NAME=A:NX=2147483648:NY=1:VIA=VIA2'),/arguments/);
const via=createGeometry(k,14,'NAME=A:NX=300:NY=301:VIA=VIA2:XORG=256:YORG=256');assert.deepEqual([via.shapes[0].segments[0].nx,via.shapes[0].segments[0].ny],[300,301]);
for(let i=0;i<8;i++)generateGeometry(k,1,parameters);const heap=k.HEAPF64.buffer.byteLength;for(let i=0;i<24;i++)generateGeometry(k,1,parameters);assert.equal(k.HEAPF64.buffer.byteLength,heap,'High-side replacement leaked heap');
const html=readFileSync('web/index.html','utf8');for(const id of ['sides','via-nx','via-ny'])assert.ok(!html.match(new RegExp(`<input id="${id}"[^>]*max="256"`)),'Form retained count ceiling');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/creator-count-admission.json',JSON.stringify({polygonSides:512,viaCounts:[300,301],originalLegacyGeometry:true,nativeWasmGridCandidates:plan.candidates.length,optimizerCreatorParity:true,recordBudget:true,integerConversionGuard:true,boundedHeap:heap},null,2)+'\n');
console.log(`Creator counts: original 512-side legacy geometry, ${plan.candidates.length} native/WASM grid candidates, optimizer/direct identity, large via counts, record budgets, integer guards and bounded heap pass`);
