import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import makeMath from '../build/portable-math/gmd.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const k=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')),python=process.env.MPMATH_PYTHON??'python3',reports=[],failures=[];
const math=await makeMath(),columnMajor=m=>m[0].flatMap((_,j)=>m.flatMap(row=>row[j]));mkdirSync('build/portable-two-port',{recursive:true});
const cases=JSON.parse(readFileSync('tests/fixtures/extended-cases.json','utf8')).cases;
for(const item of JSON.parse(readFileSync('tests/fixtures/original-maxnw-ports.json','utf8')).cases.filter(c=>c.family==='extended'))cases.push({...item.case,name:'MAXNW-'+item.max_width_cells,cells:item.cells,contact_cells:item.contact_cells,max_width_cells:item.max_width_cells});
const relative=(a,b)=>Math.abs(a-b)/Math.max(Math.abs(a),Math.abs(b),1e-300);
function matrixError(actual,expected,label){
  assert.deepEqual(actual.map(row=>row.length),expected.map(row=>row.length),label+' shape');const a=actual.flat(2),b=expected.flat(2);assert.equal(a.length,b.length,label+' size');
  assert.ok(a.every(Number.isFinite)&&b.every(Number.isFinite),label+' finite classification');
  const scale=b.reduce((m,v)=>Math.max(m,Math.abs(v)),1e-300),error=a.reduce((e,v,i)=>Math.max(e,Math.abs(v-b[i])),0)/scale;assert.ok(error<=1e-10,label+' norm error '+error);return error;
}
for(const c of cases){
  configureKernel(k,technology);createGeometry(k,c.id,c.keywords);let winding=0,ground=c.self_ground?0:-1;
  if(c.ground){createGeometry(k,2,c.ground,'',{append:true});winding=1;ground=0;}
  assert.equal(k._set_max_width_cells(c.max_width_cells??500),1);
  const hz=k._normalize_value(c.frequency_ghz,5,.1,0),cells=[...c.cells,...c.contact_cells].map(v=>k._normalize_value(v,7,.1,1));
  const actual=JSON.parse(k.UTF8ToString(k._simulate_extended_network(winding,ground,hz,...cells,512,512,...c.grid,512,1024,c.eddy_mode??0,c.layers.length,c.layers[0]??0,c.layers[1]??0,250000)));
  try{
    assert.ok(!actual.error,c.name+': '+actual.error);
    const expected=JSON.parse(execFileSync(python,['tools/reference_extended.py'],{input:JSON.stringify(actual),encoding:'utf8',maxBuffer:16*1024*1024}));assert.equal(expected.mpmath_version,'1.3.0');assert.equal(expected.groups,actual.groups);
    const n=actual.groups,values=[...columnMajor(actual.capacitance_matrix),...columnMajor(actual.inductance_impedance_matrix)],bytes=Buffer.alloc(8+values.length*8);bytes.writeInt32LE(n);values.forEach((v,i)=>bytes.writeDoubleLE(v,8+i*8));writeFileSync('build/portable-two-port/extended.bin',bytes);
    const native=JSON.parse(execFileSync('build/portable-math/gmd-native',['--extended-shunts','build/portable-two-port/extended.bin'],{encoding:'utf8'}));
    const nativePorts=JSON.parse(execFileSync('build/portable-math/gmd-native',['--extended-ports','build/portable-two-port/extended.bin'],{encoding:'utf8'}));
    const pointer=math._malloc(values.length*8);let software,softwarePorts;
    try{math.HEAPF64.set(values,pointer/8);software=JSON.parse(math.UTF8ToString(math._probe_extended_pi_shunts(pointer,pointer+16*n*n,n)));softwarePorts=JSON.parse(math.UTF8ToString(math._probe_extended_ports(pointer,pointer+16*n*n,n)));}finally{math._free(pointer);}
    assert.deepEqual(software,native,c.name+' native/WASM shunts');assert.deepEqual(actual.pi_shunts,software,c.name+' production shunts');
    assert.deepEqual(softwarePorts,nativePorts,c.name+' native/WASM port solve');
    for(const key of ['matrix','currents','voltages'])assert.deepEqual(actual[key],softwarePorts[key],c.name+' production '+key);
    for(const key of ['first','second'])software[key].forEach((v,i)=>assert.ok(relative(v,expected.circuit_shunts[key][i])<=1e-10,c.name+' independent '+key+' shunt '+i));
    const stageErrors={};for(const key of ['matrix','currents','voltages']){stageErrors[key]=matrixError(actual[key],expected[key],c.name+' '+key);assert.ok(stageErrors[key]<=1e-14,c.name+' reduced '+key+' error '+stageErrors[key]);}
    const metricErrors={};for(const [key,value]of Object.entries(expected.circuit_metrics.pi)){
      assert.equal(actual.pi[key]===null,value===null,c.name+' '+key+' classification');
      if(value!==null)metricErrors[key]=relative(actual.pi[key],value);
    }
    const maximumMetricError=Math.max(...Object.values(metricErrors));reports.push({name:c.name,groups:actual.groups,stageErrors,metricErrors,maximumMetricError});
    assert.ok(maximumMetricError<=1e-10,c.name+' circuit metrics '+maximumMetricError);
    assert.equal(actual.resonance_hz===null,expected.circuit_metrics.resonance_hz===null,c.name+' resonance classification');if(actual.resonance_hz!==null)assert.ok(relative(actual.resonance_hz,expected.circuit_metrics.resonance_hz)<=1e-10,c.name+' resonance');
    matrixError(actual.input_impedance,expected.circuit_metrics.input_impedance,c.name+' input impedance');
    console.log(c.name+': independent PIX reduction and full-precision metrics pass');
  }catch(error){failures.push({name:c.name,message:error.message,actual});console.log(c.name+': '+error.message);}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-extended.json',JSON.stringify({reference:'80-digit current elimination and independent pi extraction',reports,failures},null,2)+'\n');
assert.equal(failures.length,0,failures.map(v=>v.name+': '+v.message).join('\n'));
console.log(`Portable PIX: ${reports.length} physical circuit reductions, node states and full precision metrics pass`);
