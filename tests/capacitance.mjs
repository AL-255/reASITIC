import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const cases=JSON.parse(readFileSync('tests/fixtures/capacitance-cases.json','utf8')).cases;
const originals=JSON.parse(readFileSync('tests/fixtures/original-capacitance.json','utf8'));
const text=readFileSync('web/technology/CMOS.tek','utf8'),hash=value=>createHash('sha256').update(value).digest('hex');
assert.equal(hash(text),originals.technology_sha256);assert.equal(hash(readFileSync('tests/fixtures/capacitance-cases.json')),originals.case_definition_sha256);
const kernel=await createKernel(),technology=parseTechnology(text);let exactFields=0,originalPrintedFields=0,rawCacheBytes=0;const reports=[];
function compare(a,b,path='root') {
  if(typeof a==='number'){assert.equal(a,b,path);exactFields++;}
  else if(a && typeof a==='object'){assert.deepEqual(Object.keys(a),Object.keys(b),path);for(const key of Object.keys(a))compare(a[key],b[key],path+'.'+key);}
  else assert.equal(a,b,path);
}
function run(item,unfolded=false,limit=512){return JSON.parse(kernel.UTF8ToString(kernel._simulate_capacitance(item.ground?1:0,item.ground?0:-1,item.frequency_hz??kernel._normalize_value(item.frequency_ghz,5,.1,0),...item.cells,512,512,...item.grid,limit,unfolded?1:0)));}
for(const item of cases) {
  configureKernel(kernel,technology);createGeometry(kernel,item.id,item.keywords);if(item.ground)createGeometry(kernel,2,item.ground,'',{append:true});
  const expected=originals.cases.find(c=>c.name===item.name),input={...item,frequency_hz:expected.frequency_hz};const started=performance.now(),actual=run(input);assert.ok(!actual.error,`${item.name}: ${actual.error}`);
  const native=JSON.parse(execFileSync('build/native-geometry',['--cap',item.id,item.keywords,2,item.ground??'',expected.frequency_hz,...item.cells,...item.grid,0].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));
  compare(actual,native,item.name);assert.equal(actual.green_table_hash,expected.green_table_hash,item.name+' raw original Green tables');rawCacheBytes+=expected.raw_cache_bytes;
  for(const key of ['matrix','potential_matrix']){assert.equal(actual[key].length,expected[key].length,item.name+' '+key);actual[key].forEach((row,i)=>row.forEach((z,j)=>z.forEach((value,k)=>{assert.equal(Number(value.toPrecision(10)),expected[key][i][j][k],`${item.name}.${key}[${i}][${j}][${k}]`);originalPrintedFields++;})));}
  assert.equal(Number((actual.capacitance_f*1e15).toFixed(3)),expected.capacitance_ff_printed,item.name+' CAP total');assert.equal(Number(actual.resistance.toFixed(3)),expected.resistance_printed,item.name+' CAP resistance');originalPrintedFields+=2;
  reports.push({name:item.name,contacts:actual.contacts,groups:actual.groups,matrix_size:actual.matrix.length,raw_cache_bytes:expected.raw_cache_bytes,milliseconds:performance.now()-started});console.log(`${item.name}: native fields exact; original cache bytes and printed CAP matrices match (${reports.at(-1).milliseconds.toFixed(0)} ms)`);
}
// Unfolded matrices use the same potential solve and retain native terminal
// ordering; no printed total exists on this original command path.
for(const item of cases.slice(0,3)) {
  configureKernel(kernel,technology);const before=createGeometry(kernel,item.id,item.keywords),a=run(item,true);
  const b=JSON.parse(execFileSync('build/native-geometry',['--cap',item.id,item.keywords,2,'',kernel._normalize_value(item.frequency_ghz,5,.1,0),...item.cells,...item.grid,1].map(String),{encoding:'utf8'}));compare(a,b,'unfolded');assert.equal(a.capacitance_f,null);assert.equal(a.resistance,null);
  const second=createGeometry(kernel,2,'NAME=B:LEN=100:WID=40:METAL=M1:XORG=100:YORG=200','',{append:true});assert.deepEqual(second.shapes[1],before.shapes[0],'CAP mutated source geometry');
}
configureKernel(kernel,technology);createGeometry(kernel,2,'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100');
const base={frequency_ghz:1,cells:[1000,1000],grid:[8,8]};assert.match(run(base,false,0).error,/contacts/);assert.match(run({...base,grid:[7,8]}).error,/power-of-two/);assert.match(run({...base,cells:[1e-9,1e-9]},false,1).error??'',/^$/);
const heap=kernel.HEAPF64.buffer.byteLength;
for(let i=0;i<30;++i){assert.match(run({...base,grid:[7,8]}).error,/power-of-two/);assert.ok(!run(base).error);}
assert.equal(kernel.HEAPF64.buffer.byteLength,heap,'Repeated CAP solves grew the heap');
configureKernel(kernel,technology);createGeometry(kernel,2,'NAME=A:LEN=100:WID=1:METAL=M1:XORG=100:YORG=100');assert.match(run(base).error,/collapses/);
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/capacitance-parity.json',JSON.stringify({exactFields,originalPrintedFields,rawCacheBytes,cases:reports},null,2)+'\n');
console.log(`CAP: ${exactFields} exact native numeric fields, ${originalPrintedFields} original printed fields, ${rawCacheBytes} raw original Green-table bytes matched by payload hashes.`);
