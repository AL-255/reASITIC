import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {portSampleSummary,portSweepRecord,threePortSweepMatrixCSV,portSweepMetricsCSV} from '../web/port-sweep.mjs';
const source='tests/fixtures/three-port-sweep-cases.json',cases=JSON.parse(readFileSync(source,'utf8')).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-three-port-sweep.json','utf8'));
const hash=path=>createHash('sha256').update(readFileSync(path)).digest('hex');assert.equal(hash(source),fixture.case_definition_sha256);assert.equal(hash('web/technology/CMOS.tek'),fixture.technology_sha256);
const kernel=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')),hex=value=>{const b=Buffer.alloc(8);b.writeDoubleLE(value);return b.toString('hex');};let files=0,points=0,independentPoints=0;const reports=[];
for(const item of cases){
  const v=item.case;configureKernel(kernel,technology);let before;
  for(const [i,c] of v.creators.entries())before=createGeometry(kernel,c.id,c.keywords,'',{append:i>0});
  if(v.ground&&!v.ground.startsWith('@'))before=createGeometry(kernel,2,v.ground,'',{append:true});
  const indices=v.names.map(name=>before.shapes.findIndex(s=>s.name===name)),ground=v.ground?v.ground.startsWith('@')?indices[Number(v.ground[1])-1]:before.shapes.findIndex(s=>s.name==='G'):-1;
  const cells=[...v.cells,...v.contact_cells].map(value=>kernel._normalize_value(value,7,.1,1)),suffix=[...cells,512,512,...v.grid,512,1024,v.eddy_mode,v.layers.length,v.layers[0]??0,v.layers[1]??0,250000];
  const plan=JSON.parse(kernel.UTF8ToString(kernel._port_frequency_schedule(...item.range_ghz.map(value=>kernel._normalize_value(value,5,.1,0)),101)));assert.ok(!plan.error,plan.error);
  const solve=hz=>{const r=JSON.parse(kernel.UTF8ToString(kernel._simulate_three_port(...indices,ground,hz,...suffix)));assert.ok(!r.error,item.name+': '+r.error);return r;};
  let samples;
  if(item.reference==='original'){
    const original=fixture.cases.find(c=>c.name===item.name);assert.deepEqual(plan.frequencies_hz.map(hex),original.frequency_hex,item.name+' original saved frequencies');
    kernel._set_reference_numerics(1);samples=plan.frequencies_hz.map(hz=>portSampleSummary(solve(hz)));kernel._set_reference_numerics(0);
    for(const [key,record] of Object.entries(original.records)){assert.equal(portSweepRecord(samples,key[0],key.endsWith('-polar')),record,item.name+' original '+key+' complete numerical section');files++;}
  }
  const production=plan.frequencies_hz.map((hz,i)=>{
    const r=solve(hz);
    assert.equal(r.node_count,r.capacitance_matrix.length);assert.equal(r.node_count,r.inductance_impedance_matrix.length);
    if(samples){const scale=Math.max(...samples[i].impedance_matrix.flat(2).map(Math.abs));r.impedance_matrix.flat(2).forEach((value,j)=>assert.ok(Math.abs(value-samples[i].impedance_matrix.flat(2)[j])<=scale*1e-10,item.name+' production accuracy'));}
    else{
      const reference=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_three_port.py'],{input:JSON.stringify(r),encoding:'utf8'}));
      const scale=Math.max(...reference.impedance_matrix.flat(2).map(Math.abs));r.impedance_matrix.flat(2).forEach((value,j)=>assert.ok(Math.abs(value-reference.impedance_matrix.flat(2)[j])<=scale*1e-10,item.name+' independent nodal accuracy'));independentPoints++;
    }
    return portSampleSummary(r);
  });
  assert.ok(!('system_matrix' in production[0]));assert.ok(!('system_before' in production[0]));assert.ok(production[0].impedance_matrix&&production[0].records&&production[0].headers);
  for(const parameter of ['s','y','z'])assert.equal(threePortSweepMatrixCSV(production,parameter).trimEnd().split('\n').length,1+9*production.length);
  assert.equal(portSweepMetricsCSV(production).trimEnd().split('\n').length,1+production.length);assert.ok(portSweepMetricsCSV(production).startsWith('model,frequency_hz,'));
  assert.throws(()=>portSweepRecord([...production,...production]),/increasing/);
  assert.throws(()=>portSweepRecord([production[0],{...production[1],names:[...production[1].names].reverse()}]),/same port/);
  assert.throws(()=>portSweepRecord([production[0],{...production[1],backing:{captured:true}}]),/same port/);
  assert.throws(()=>portSweepRecord([production[0],{...production[1],headers:{...production[1].headers,z:'changed'}}],'z'),/headers differ/);
  const after=createGeometry(kernel,2,'NAME=VERIFY:LEN=20:WID=4:METAL=M2:XORG=450:YORG=450','',{append:true});assert.deepEqual(after.shapes.slice(1),before.shapes,item.name+' source geometry');
  reports.push({name:item.name,frequencies_hz:plan.frequencies_hz});points+=plan.frequencies_hz.length;
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/three-port-sweep.json',JSON.stringify({cases:reports.length,points,originalFiles:files,independentNodalPoints:independentPoints,relativeTolerance:1e-10,reports},null,2)+'\n');
console.log(`Three-port sweeps: ${points} saved frequencies, ${files} complete original S/Y/Z rectangular/polar sections, ${independentPoints} independent grounded nodal checks, compact history, all-cell CSVs and immutable sources pass`);
