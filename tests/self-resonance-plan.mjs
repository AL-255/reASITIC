import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import make from '../dist/asitic.js';
import {resonancePlan,capacitorPortMatrix,resonanceHistoryCSV} from '../web/self-resonance.mjs';
const k=await make(),definitions=readFileSync('tests/fixtures/self-resonance-cases.json','utf8'),cases=JSON.parse(definitions).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-self-resonance.json','utf8'));
assert.equal(createHash('sha256').update(definitions).digest('hex'),fixture.case_definition_sha256);
const number=hex=>Buffer.from(hex,'hex').readDoubleLE(),bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');};
mkdirSync('build/self-resonance',{recursive:true});let original=0,hostChecks=0;const reports=[];
function host(settings,samples){
  const values=samples.flatMap(s=>[s.frequency_hz,...s.matrix.flat(2)]),buffer=Buffer.alloc(values.length*8);values.forEach((v,i)=>buffer.writeDoubleLE(v,i*8));writeFileSync('build/self-resonance/history.bin',buffer);
  return JSON.parse(execFileSync('build/native-geometry',['--selfres',settings.first,settings.second,settings.step,settings.differential?1:0,settings.fast?1:0,settings.native_iterations??10,settings.sample_budget??101,'build/self-resonance/history.bin'].map(String),{encoding:'utf8'}));
}
for(const item of fixture.cases){
  if(item.native_fault){assert.equal(item.native_fault,'SIGSEGV');continue;}
  const c=cases.find(c=>c.name===item.name),settings={first:k._normalize_value(c.bounds_ghz[0],5,.1,0),second:k._normalize_value(c.bounds_ghz[1],5,.1,0),step:k._normalize_value(c.step_ghz,5,.1,0),differential:c.mode==='differential',fast:c.fast,native_iterations:c.native_iterations},samples=[];
  for(const expected of item.samples){
    const plan=resonancePlan(k,settings,samples);assert.equal(plan.status,'need_sample');assert.equal(bits(plan.next_frequency_hz),expected.frequency_hex,item.name+' original next frequency');
    assert.deepEqual(host(settings,samples),plan,item.name+' native/WASM scalar search');hostChecks++;
    const values=Array.from({length:8},(_,i)=>Buffer.from(expected.admittance_hex,'hex').readDoubleLE(i*8));samples.push({frequency_hz:number(expected.frequency_hex),matrix:[[values.slice(0,2),values.slice(2,4)],[values.slice(4,6),values.slice(6,8)]]});original++;
  }
  const plan=resonancePlan(k,settings,samples);assert.equal(bits(plan.frequency_hz??-1),item.return_frequency_hex[0],item.name+' original returned frequency');assert.deepEqual(plan,host(settings,samples));hostChecks++;
  assert.equal(plan.native_message,item.printed_result[0]);assert.deepEqual(resonancePlan(k,{...settings,native_iterations:999},samples),plan,'The original ignored iteration argument changed the search');
  const csv=resonanceHistoryCSV(samples,plan);assert.equal(csv.trimEnd().split('\n').length,samples.length+1);reports.push({name:item.name,analyses:samples.length,status:plan.status,frequency_hz:plan.frequency_hz,last_analyzed_hz:plan.last_analyzed_hz,convergence:plan.convergence,observed_sign_change:plan.observed_sign_change});
}
// Exercise fast quantization, endpoints, equal/reversed bounds and the native
// 100-iteration/101-analysis edge using an analytic, finite impedance model.
for(const settings of [{first:1,second:10,step:.001},{first:10,second:1,step:.05,fast:true},{first:3,second:3,step:.1},{first:1e-200,second:1e200,step:Number.MIN_VALUE},{first:1,second:10,step:.001,sample_budget:2}]){
  const samples=[];let plan=resonancePlan(k,settings,samples);
  while(plan.status==='need_sample'){
    const f=plan.next_frequency_hz,x=settings.step===Number.MIN_VALUE?-1:3-f,y=[1/(1+x*x),-x/(1+x*x)];samples.push({frequency_hz:f,matrix:[[y,[0,0]],[[0,0],y]]});plan=resonancePlan(k,settings,samples);
    assert.deepEqual(plan,host(settings,samples));hostChecks++;
  }
  if(settings.step===Number.MIN_VALUE){assert.equal(plan.status,'native_limit');assert.equal(samples.length,101);assert.equal(plan.trace.length,100);assert.match(resonanceHistoryCSV(samples,plan).trim().split('\n').at(-1),/,false,,/);}
  if(settings.sample_budget===2)assert.equal(plan.status,'sample_limit');
}
const good={first:1,second:10,step:.1},expected=resonancePlan(k,good,[]);
for(const change of [{first:0},{second:Infinity},{step:0},{sample_budget:0},{sample_budget:102},{sample_budget:1.5}])assert.throws(()=>resonancePlan(k,{...good,...change},[]));
assert.throws(()=>resonancePlan(k,good,[{frequency_hz:5,matrix:[[[1,0],[0,0]],[[0,0],[1,0]]]}]),/sequence/);
assert.throws(()=>resonancePlan(k,good,[{frequency_hz:expected.next_frequency_hz,matrix:[[[0,0],[0,0]],[[0,0],[0,0]]]}]),/finite input impedance/);
assert.deepEqual(resonancePlan(k,good,[]),expected);
assert.deepEqual(capacitorPortMatrix(k,{matrix:[[[1,2],[3,4]],[[3,4],[5,6]]]}),[[[8,10],[-3,-4]],[[-3,-4],[4,6]]]);
assert.throws(()=>capacitorPortMatrix(k,{matrix:[[[1,0]]]}),/2x2/);
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/self-resonance-plan.json',JSON.stringify({original_queries:original,native_wasm_plans:hostChecks,reports},null,2)+'\n');
console.log(`SELFRES C++ search: ${original} original saved queries/returns and ${hostChecks} exact native/WASM plans, ignored iteration argument, fast quantization, 101-analysis cap, sample budgets and rejection recovery pass`);
