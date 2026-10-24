import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {portSampleSummary,portSweepRecord,portSweepScatteringCSV,portSweepMetricsCSV} from '../web/port-sweep.mjs';
const source='tests/fixtures/port-sweep-cases.json',definitions=JSON.parse(readFileSync(source,'utf8')).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-port-sweep.json','utf8'));
const hash=path=>createHash('sha256').update(readFileSync(path)).digest('hex');
assert.equal(hash(source),fixture.case_definition_sha256);assert.equal(hash('web/technology/CMOS.tek'),fixture.technology_sha256);
const kernel=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')),hex=value=>{const b=Buffer.alloc(8);b.writeDoubleLE(value);return b.toString('hex');};let files=0,points=0;const reports=[];
for(const item of definitions){
  const v=item.case,family=item.family,original=fixture.cases.find(c=>c.name===item.name);configureKernel(kernel,technology);
  const creators=family==='transformer'?[v.primary,v.secondary]:family==='pad'?[v.device,...v.pads]:[{id:v.id,keywords:v.keywords}];
  let layout;for(const creator of creators.filter(Boolean))layout=createGeometry(kernel,creator.id,creator.keywords,'',{append:Boolean(layout)});
  if(v.ground&&!v.ground.startsWith('@'))layout=createGeometry(kernel,2,v.ground,'',{append:true});
  const index=name=>layout.shapes.findIndex(s=>s.name===name),a=index('A'),b=index('B'),p1=index('P1'),p2=index('P2');
  const ground=v.ground?v.ground.startsWith('@')?v.ground==='@1'?(family==='transformer'?a:p1):v.ground==='@2'?(family==='transformer'?b:p2):a:index('G'):family==='network'?a:-1;
  const prefix=family==='transformer'?[a,b,ground]:family==='pad'?[a,p1,p2,ground]:[a,ground];
  const suffix=[...[...v.cells,...v.contact_cells].map(value=>kernel._normalize_value(value,7,.1,1)),512,512,...v.grid,512,1024,v.eddy_mode,v.layers.length,v.layers[0]??0,v.layers[1]??0,...(family==='transformer'?[kernel._normalize_value(v.termination,4,.1,0)]:[]),250000];
  const solver=family==='transformer'?kernel._simulate_transformer:family==='pad'?kernel._simulate_pad_network:family==='extended'?kernel._simulate_extended_network:kernel._simulate_network;
  const plan=JSON.parse(kernel.UTF8ToString(kernel._port_frequency_schedule(...item.range_ghz.map(value=>kernel._normalize_value(value,5,.1,0)),101)));
  assert.ok(!plan.error,plan.error);assert.deepEqual(plan.frequencies_hz.map(hex),original.frequency_hex,item.name+' original saved native frequencies');
  kernel._set_reference_numerics(1);const samples=plan.frequencies_hz.map(hz=>{const result=JSON.parse(kernel.UTF8ToString(solver(...prefix,hz,...suffix)));assert.ok(!result.error,item.name+': '+result.error);return portSampleSummary(result);});kernel._set_reference_numerics(0);
  for(const [key,record] of Object.entries(original.records)){
    const parameter=key==='model'?(family==='transformer'?'trans':'pi'):key[0];
    assert.equal(portSweepRecord(samples,parameter,key.endsWith('-polar')),record,item.name+' original '+key+' complete numerical file');files++;
  }
  const production=plan.frequencies_hz.map((hz,i)=>{const result=JSON.parse(kernel.UTF8ToString(solver(...prefix,hz,...suffix)));assert.ok(!result.error,result.error);result.two_port.scattering_matrix.flat(2).forEach((value,j)=>assert.equal(value.toPrecision(8),samples[i].two_port.scattering_matrix.flat(2)[j].toPrecision(8),item.name+' production S accuracy'));return portSampleSummary(result);});
  assert.equal(portSweepScatteringCSV(production).trimEnd().split('\n').length,1+4*samples.length);
  assert.equal(portSweepMetricsCSV(production).trimEnd().split('\n').length,1+samples.length);
  assert.ok(portSweepScatteringCSV(production).startsWith('model,frequency_hz,row,column,'));
  assert.ok(portSweepMetricsCSV(production).startsWith('model,frequency_hz,'));
  assert.ok(!('capacitance_matrix' in samples[0]));assert.ok(samples[0].matrix&&samples[0].two_port&&samples[0].eddy);
  assert.throws(()=>portSweepRecord([...samples,...samples]),/increasing/);
  assert.throws(()=>portSweepRecord([samples[0],{...samples[1],name:'different'}]),/same port/);
  assert.throws(()=>portSweepRecord(samples,'z'),/supported/);
  reports.push({name:item.name,frequencies_hz:plan.frequencies_hz});points+=samples.length;
}
assert.throws(()=>portSweepMetricsCSV([]),/at least one/);assert.throws(()=>portSweepScatteringCSV(null),/at least one/);
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/port-sweep.json',JSON.stringify({cases:reports.length,points,originalFiles:files,productionScatteringDigits:8,reports},null,2)+'\n');
console.log(`Port sweeps: ${points} exact native frequencies, ${files} complete original S/polar/Y/model numerical files, production S accuracy, compact samples and CSV validation pass`);
