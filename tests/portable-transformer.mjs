import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import makeMath from '../build/portable-math/gmd.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const k=await make(),math=await makeMath(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')),python=process.env.MPMATH_PYTHON??'python3';
const cases=JSON.parse(readFileSync('tests/fixtures/transformer-cases.json','utf8')).cases,reports=[];
const columnMajor=m=>m[0].flatMap((_,j)=>m.flatMap(row=>row[j]));mkdirSync('build/portable-two-port',{recursive:true});
function compare(a,b,label){const x=a.flat(2),y=b.flat(2),norm=Math.max(...y.map(Math.abs),1e-300);assert.equal(x.length,y.length);assert.ok(x.every(Number.isFinite)&&y.every(Number.isFinite));const error=Math.max(...x.map((v,i)=>Math.abs(v-y[i])))/norm;assert.ok(error<=1e-14,label+' norm error '+error);return error;}
for(const c of cases){
  configureKernel(k,technology);let layout=createGeometry(k,c.primary.id,c.primary.keywords);if(c.secondary)layout=createGeometry(k,c.secondary.id,c.secondary.keywords,'',{append:true});
  let first=layout.shapes.findIndex(s=>s.name==='A'),second=layout.shapes.findIndex(s=>s.name==='B'),ground=-1;
  if(c.ground?.startsWith('@'))ground=c.ground==='@1'?first:second;
  else if(c.ground){layout=createGeometry(k,2,c.ground,'',{append:true});first++;second++;ground=0;}
  const hz=k._normalize_value(c.frequency_ghz,5,.1,0),termination=k._normalize_value(c.termination,4,.1,0),cells=[...c.cells,...c.contact_cells].map(v=>k._normalize_value(v,7,.1,1));
  const actual=JSON.parse(k.UTF8ToString(k._simulate_transformer(first,second,ground,hz,...cells,512,512,...c.grid,512,1024,c.eddy_mode,c.layers.length,c.layers[0]??0,c.layers[1]??0,termination,250000)));assert.ok(!actual.error,c.name+': '+actual.error);
  const reference=JSON.parse(execFileSync(python,['tools/reference_transformer.py'],{input:JSON.stringify(actual),encoding:'utf8',maxBuffer:16*1024*1024}));assert.equal(reference.mpmath_version,'1.3.0');
  const errors={};for(const key of ['matrix','node_capacitance_matrix','impedance_matrix'])errors[key]=compare(actual[key],reference[key],c.name+' '+key);
  const n=actual.groups,values=[...columnMajor(actual.capacitance_matrix),...columnMajor(actual.inductance_admittance_matrix)],bytes=Buffer.alloc(8+values.length*8);bytes.writeInt32LE(n);values.forEach((v,i)=>bytes.writeDoubleLE(v,8+i*8));writeFileSync('build/portable-two-port/transformer.bin',bytes);
  const native=JSON.parse(execFileSync('build/portable-math/gmd-native',['--folded-ports','build/portable-two-port/transformer.bin'],{encoding:'utf8'})),pointer=math._malloc(values.length*8);let software;
  try{math.HEAPF64.set(values,pointer/8);software=JSON.parse(math.UTF8ToString(math._probe_folded_ports(pointer,pointer+16*n*n,n,0,0)));}finally{math._free(pointer);}
  assert.deepEqual(native,software,c.name+' native/WASM node solve');for(const [key,value]of Object.entries(software))assert.deepEqual(actual[key],value,c.name+' production '+key);
  let metricError=0;
  for(const [key,value]of Object.entries(reference.transformer)){
    assert.equal(actual.transformer[key]===null,value===null,c.name+' '+key+' classification');
    if(value!==null){const error=Math.abs(actual.transformer[key]-value)/Math.max(Math.abs(value),1e-300);assert.ok(error<=1e-10,c.name+' independent '+key+' error '+error);metricError=Math.max(metricError,error);}
  }
  reports.push({name:c.name,groups:n,errors,maximumMetricRelativeError:metricError});console.log(c.name+': independent circuit, transformer labels, native/WASM bits and production integration pass');
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-transformer.json',JSON.stringify({reference:'80-digit vertex elimination and independent 2x2 inverse',reports,nativeWasmBitsIdentical:true},null,2)+'\n');
console.log(`Portable CALCTRANS: ${reports.length} independent circuit and model checks pass`);
