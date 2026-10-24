import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry,generateGeometry,editGeometry} from '../web/technology.mjs';
import {optimizationPlan,createOptimizationCandidate} from '../web/optimization.mjs';
const k=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));
configureKernel(k,technology);
const fixture=JSON.parse(readFileSync('tests/fixtures/original-creator-ranges.json','utf8'));
const original=fixture.cases.find(c=>c.name==='cmos-square-fraction').shapes[0];
const physical=s=>({origin:s.origin,segments:s.segments.map(({metal,width,thickness,points,nx,ny})=>({metal,width,points,...(metal<technology.metals.length?{thickness}:{nx,ny})}))});
const square=[400,400,1,1,65.25,200,4,3,2,0,15,66];
let layout=generateGeometry(k,0,square,{x:256,y:256});assert.deepEqual(physical(layout.shapes[0]),physical(original),'Legacy builder original high-turn geometry');
// The recovered native enumerator, the WASM planner and original direct
// creator must all admit the same descriptor above the former 64-turn limit.
const keywords='FNAME=range.out:NAME=OPT:FREQ=1:LMIN=.00001:LMAX=1000:MINQ=.00001:AMIN=400:AINC=1:AMAX=400:SMIN=1:SINC=1:SMAX=1:WMIN=1:WINC=1:WMAX=1:METAL=M4:EXIT=M3:SIDES=4:SIMTYPE=2';
const plan=optimizationPlan(k,keywords,technology.domain,{candidate_limit:1000});
assert.deepEqual(plan,JSON.parse(execFileSync('build/native-geometry',['--grid',keywords,'1000'],{encoding:'utf8'})),'Native/WASM original range enumeration');
const descriptor=plan.candidates.find(v=>v[7]===65.25);assert.ok(descriptor,'Original enumerator did not retain a high-turn candidate');
layout=createOptimizationCandidate(k,'OPT',descriptor);const direct=createGeometry(k,0,`NAME=A:LEN=400:WID=400:W=1:S=1:N=65.25:METAL=M4:EXIT=M3:XORG=${descriptor[8]}:YORG=${descriptor[9]}`);
assert.deepEqual(physical(layout.shapes[0]),physical(direct.shapes[0]),'Optimizer builder differs from original direct creator');
const clean=({warnings,...rest})=>rest;
const keep=createGeometry(k,2,'NAME=KEEP:LEN=20:WID=3:METAL=M1');
assert.throws(()=>createGeometry(k,0,'NAME=HUGE:LEN=400:WID=400:W=.0001:S=.0001:N=100000:METAL=M4:EXIT=M3','',{append:true,snap:false}),/record budget/);
assert.throws(()=>createGeometry(k,3,'PNAME=P:SNAME=S:LEN=400:WID=400:W=.0001:S=.0001:NP=1:NS=100000:METAL=M4:EXIT=M3','',{append:true,snap:false}),/record budget/);
assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(keep),'Record-budget rejection damaged source');
// This descriptor fits the per-winding record estimate, but its composite
// copies exceed the transaction allocation budget during actual construction.
assert.throws(()=>createGeometry(k,18,'NAME=HUGE:LEN=400:WID=400:W=.001:S=.001:N=10000:METAL=M5:EXIT=M1','',{append:true,snap:false}),/creation memory budget/);
assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(keep),'Allocation-budget rejection damaged source');
assert.throws(()=>generateGeometry(k,8,[1000000,1000000,1,1,10000,200,4,4,0,0,15,1]),/creation memory budget/);
assert.throws(()=>generateGeometry(k,0,[...square.slice(0,4),1e300,...square.slice(5)]),/record budget/);
assert.throws(()=>generateGeometry(k,6,[...square.slice(0,11),1e300]),/record budget/);
assert.throws(()=>generateGeometry(k,1,[500,500,.1,.1,64,250,256,3,-1,0,15,1]),/export size limit/);
for(let i=0;i<8;++i)generateGeometry(k,0,square,{x:256,y:256});const heap=k.HEAPF64.buffer.byteLength;
for(let i=0;i<24;++i)layout=generateGeometry(k,0,square,{x:256,y:256});
assert.equal(k.HEAPF64.buffer.byteLength,heap,'High-turn creation or rejected transaction leaked heap');assert.deepEqual(physical(layout.shapes[0]),physical(original),'Recovery after allocation/export failures');
const html=readFileSync('web/index.html','utf8');for(const id of ['turns','secondary'])assert.ok(!html.match(new RegExp(`<input id="${id}"[^>]*max="64"`)),'Form retained old turn ceiling');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/creator-range-admission.json',JSON.stringify({plannerCandidates:plan.candidates.length,maximumPlannerTurns:Math.max(...plan.candidates.map(v=>v[7])),legacyOriginalPhysicalParity:true,optimizerOriginalCreatorParity:true,recordBudget:true,allocationBudget:true,exportRollback:true,boundedHeap:heap},null,2)+'\n');
console.log(`Creator ranges: ${plan.candidates.length} native/WASM candidates through ${Math.max(...plan.candidates.map(v=>v[7]))} turns, original high-turn legacy geometry, original/optimizer builder identity, allocation/record/export recovery and bounded heap pass`);
