import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
import {optimizationPlan,createOptimizationCandidate,optimizationAccept,optimizationCSV} from '../web/optimization.mjs';
import {parseCreatorCommand} from '../web/creators.mjs';
import {writeGDS} from '../web/gds.mjs';
const source=readFileSync('tests/fixtures/optimization-cases.json','utf8'),cases=JSON.parse(source).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-optimization.json','utf8')),hash=x=>createHash('sha256').update(x).digest('hex');assert.equal(hash(source),fixture.case_definition_sha256);
const k=await make(),reports=[];k._set_potential_memo(1);let count=0,nonfinite=0,accepted=0,fields=0,maximumCircuitError=0,maximumOriginalCircuitError=0;
const names=['frequency_hz','inductance_h','resistance','capacitance_second_f','capacitance_first_f','resistance_second','resistance_first','quality_first','quality_second','quality_differential','shunt_single','shunt_differential'];
const physical=layout=>({origin:layout.shapes[0].origin,segments:layout.shapes[0].segments.map(s=>({metal:s.metal,width:s.width,points:s.points,...(s.metal<layout.metal_count?{thickness:s.thickness}:{nx:s.nx,ny:s.ny})}))});
const clean=({warnings,...rest})=>rest;
const solve=hz=>JSON.parse(k.UTF8ToString(k._simulate_network(0,-1,hz,1000,1000,1000,1000,1000,512,512,128,128,128,256,0,0,0,0,250000)));
mkdirSync('build/gds-optimization',{recursive:true});
const lifecycleOnly=process.argv.includes('--lifecycle-only');
if(!lifecycleOnly)for(const c of cases){
  const text=readFileSync(`web/technology/${c.technology}.tek`,'utf8');assert.equal(hash(text),fixture.technology_sha256[c.technology]);const technology=parseTechnology(text);configureKernel(k,technology);
  if(c.prelude){const creator=parseCreatorCommand(c.prelude);createGeometry(k,creator.id,creator.keywords);}
  const expected=fixture.cases.find(v=>v.name===c.name),plan=optimizationPlan(k,c.keywords,technology.domain,{candidate_limit:1000});
  assert.equal(plan.frequency_hz,expected.normalized.FREQUENCY_HZ);for(const [key,native]of [['inductance_min','LMIN'],['inductance_max','LMAX'],['minimum_quality','MINQ']])assert.equal(plan[key],expected.normalized[native]);
  assert.deepEqual(plan.candidates,expected.candidates.map(v=>v.descriptor),c.name+' original candidate order/descriptor');
  if(c.technology==='CMOS'&&!c.prelude){const native=JSON.parse(execFileSync('build/native-geometry',['--grid',c.keywords,'1000'],{encoding:'utf8'}));assert.deepEqual(native,plan,c.name+' native/WASM plan');}
  const samples=[];let matched=0;
  for(const [index,descriptor]of plan.candidates.entries()){
    const original=expected.candidates[index],layout=createOptimizationCandidate(k,plan.name,descriptor);layout.metal_count=technology.metals.length;assert.deepEqual(physical(layout),original.geometry,c.name+' original candidate geometry '+index);fields+=original.geometry.segments.reduce((n,s)=>n+Object.values(s).flat(2).filter(v=>typeof v==='number').length,2);
    if(c.technology==='CMOS'){
      const native=JSON.parse(execFileSync('build/native-geometry',['--grid-create',plan.name,...descriptor,'0'].map(String),{encoding:'utf8'}));native.metal_count=technology.metals.length;assert.deepEqual(physical(native),original.geometry,c.name+' native candidate '+index);
    }
    writeFileSync(`build/gds-optimization/${c.name}-${index}.gds`,writeGDS(layout,technology));
    const actual=solve(plan.frequency_hz),sample={index,descriptor,accepted:false,geometry_valid:true,frequency_hz:plan.frequency_hz};
    if(original.pi.some(v=>typeof v!=='number')){assert.ok(actual.error,c.name+' original nonfinite candidate must reject');nonfinite++;sample.error=actual.error;}
    else{
      assert.ok(!actual.error,c.name+': '+actual.error);sample.pi=actual.pi;sample.accepted=optimizationAccept(k,plan,actual.pi);
      k._set_reference_numerics(1);const historical=solve(plan.frequency_hz);k._set_reference_numerics(0);assert.ok(!historical.error,historical.error);
      const reference=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_two_port.py'],{input:JSON.stringify(actual),encoding:'utf8',maxBuffer:16*1024*1024}));assert.equal(reference.mpmath_version,'1.3.0');
      for(const [i,key]of names.entries()){
        const scale=Math.max(1e-300,Math.abs(original.pi[i]));assert.ok(Math.abs(historical.pi[key]-original.pi[i])<=scale*1e-10,`${c.name}/${index} original historical ${key}`);
        const independent=reference.circuit_metrics.pi[key],circuitScale=Math.max(1e-300,Math.abs(independent)),error=Math.abs(actual.pi[key]-independent)/circuitScale;assert.ok(error<=1e-10,`${c.name}/${index} independent circuit ${key}: ${error}`);maximumCircuitError=Math.max(maximumCircuitError,error);maximumOriginalCircuitError=Math.max(maximumOriginalCircuitError,Math.abs(original.pi[i]-independent)/circuitScale);
      }
      if(sample.accepted){
        const row=expected.accepted_printed[matched++];assert.ok(row,c.name+' extra accepted candidate');
        const values=[...descriptor.slice(4,8),actual.pi.inductance_h*1e9,actual.pi.resistance,actual.pi.capacitance_second_f*1e15,actual.pi.capacitance_first_f*1e15,actual.pi.resistance_second,actual.pi.resistance_first,actual.pi.quality_first,actual.pi.quality_second,actual.pi.quality_differential];
        values.forEach((v,j)=>assert.ok(Math.abs(v-Number(row[j]))<=.00500001,`${c.name}/${index} original printed column ${j}: ${v} vs ${row[j]}`));accepted++;
      }
    }
    samples.push(sample);count++;
  }
  assert.equal(matched,expected.accepted_printed.length,c.name+' original accepted count');console.log(c.name+': original candidates, geometry, models and acceptance pass');reports.push({name:c.name,samples});
}
const technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));configureKernel(k,technology);
const before=createGeometry(k,2,'NAME=KEEP:LEN=40:WID=8:METAL=M1'),valid=cases[0].keywords;
for(const keywords of ['::',valid.replace('AINC=10','AINC=0'),valid.replace('SIMTYPE=2','SIMTYPE=1'),valid.replace('SIDES=4','SIDES=257'),valid.replace('NAME=OPT','NAME=KEEP')])assert.throws(()=>optimizationPlan(k,keywords,technology.domain));
assert.throws(()=>optimizationPlan(k,valid,technology.domain,{candidate_limit:2}),/budget/);assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before),'Rejected plan changed source');
const plan=optimizationPlan(k,valid,technology.domain),descriptor=plan.candidates[0];
for(const bad of [[...descriptor.slice(0,4),-1,...descriptor.slice(5)],[...descriptor.slice(0,7),64,...descriptor.slice(8)]])assert.throws(()=>createOptimizationCandidate(k,'BAD',bad));
assert.throws(()=>createOptimizationCandidate(k,'TOO_BIG',[1,256,3,2,250,.1,.1,64,100,100]),/export size/);
assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before),'Rejected replacement changed source');
const composed=createOptimizationCandidate(k,'OPT',descriptor,true);assert.equal(composed.shapes.length,2);assert.deepEqual(composed.shapes[1],before.shapes[0]);assert.throws(()=>createOptimizationCandidate(k,'KEEP',descriptor,true),/already/);assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(composed));
for(let i=0;i<20;++i)createOptimizationCandidate(k,'OPT',plan.candidates[i%3]);const heap=k.HEAPF64.buffer.byteLength;for(let i=0;i<60;++i)createOptimizationCandidate(k,'OPT',plan.candidates[i%3]);assert.equal(k.HEAPF64.buffer.byteLength,heap,'Candidate replacement leaked heap');
assert.throws(()=>optimizationPlan(k,valid+'\0',technology.domain),/NUL/);assert.throws(()=>createOptimizationCandidate(k,'BAD\0',descriptor),/NUL/);
const csv=optimizationCSV(reports.flatMap(r=>r.samples));assert.equal(csv.trim().split('\n').length,count+1);assert.match(csv,/accepted/);
mkdirSync('build/evidence',{recursive:true});writeFileSync(`build/evidence/optimization${lifecycleOnly?'-lifecycle':''}.json`,JSON.stringify({cases:lifecycleOnly?0:cases.length,candidates:count,accepted,originalNonfinite:nonfinite,physicalFields:fields,maximumCircuitError,maximumOriginalCircuitError,memo:JSON.parse(k.UTF8ToString(k._potential_memo_stats())),reports},null,2)+'\n');
console.log(lifecycleOnly?'SWEEP admission, replacement/append rollback, late export rejection and bounded-heap lifecycle pass':`SWEEP: ${cases.length} original cases, ${count} exact descriptors/geometries, ${accepted} accepted reports, ${nonfinite} native nonfinite rejections, native plans, original model metrics, admission/rollback and bounded-heap replacement pass`);
