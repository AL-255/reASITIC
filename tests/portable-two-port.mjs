import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import makeMath from '../build/portable-math/gmd.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';

const kernel=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));
const math=await makeMath();mkdirSync('build/portable-two-port',{recursive:true});
const columnMajor=matrix=>matrix[0].flatMap((_,j)=>matrix.flatMap(row=>row[j]));
const bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');};
const python=process.env.MPMATH_PYTHON??'python3',reports=[],failures=[];
const cases=['network','pad'].flatMap(model=>JSON.parse(readFileSync(`tests/fixtures/${model}-cases.json`,'utf8')).cases.map(item=>({model,item})));
const originalPads=JSON.parse(readFileSync('tests/fixtures/original-pad.json','utf8')).cases;
const relative=(actual,expected)=>Math.abs(actual-expected)/Math.max(Math.abs(actual),Math.abs(expected),1e-300);
function compareMatrix(actual,expected,label){
  assert.deepEqual(actual.map(row=>row.length),expected.map(row=>row.length),label+' dimensions');
  const values=expected.flat(2),got=actual.flat(2),scale=values.reduce((scale,v)=>Math.max(scale,Math.abs(v)),1e-300);let error=0;
  assert.equal(got.length,values.length,label+' scalar count');
  got.forEach((value,i)=>{assert.ok(Number.isFinite(value),label+' finite');error=Math.max(error,Math.abs(value-values[i]));});
  assert.ok(error/scale<=1e-10,label+' norm error '+error/scale);return error/scale;
}
for(const {model,item:c} of cases){
  const name=(model==='pad'?'PI4/':'PI2/')+c.name;
  configureKernel(kernel,technology);let layout;
  const creators=model==='pad'?[c.device,...c.pads]:[{id:c.id,keywords:c.keywords}];
  for(const [i,creator] of creators.entries())layout=createGeometry(kernel,creator.id,creator.keywords,'',{append:i>0});
  let device=layout.shapes.findIndex(s=>s.name==='A'),ground=-1,pads;
  if(model==='pad')pads=['P1','P2'].map(name=>layout.shapes.findIndex(s=>s.name===name));
  if(c.ground?.startsWith('@'))ground=c.ground==='@1'?pads[0]:c.ground==='@2'?pads[1]:device;
  else if(c.ground){layout=createGeometry(kernel,2,c.ground,'',{append:true});++device;if(pads)pads=pads.map(i=>i+1);ground=0;}
  else if(c.self_ground)ground=device;
  const hz=kernel._normalize_value(c.frequency_ghz,5,.1,0),cells=[...c.cells,...c.contact_cells].map(v=>kernel._normalize_value(v,7,.1,1));
  const args=[ground,hz,...cells,512,512,...c.grid,512,1024,c.eddy_mode,c.layers.length,c.layers[0]??0,c.layers[1]??0,250000];
  const actual=JSON.parse(kernel.UTF8ToString(model==='pad'?kernel._simulate_pad_network(device,...pads,...args):kernel._simulate_network(device,...args)));
  try{
    if(model==='pad'){
      const original=originalPads.find(v=>v.name===c.name),bytes=Buffer.from(original.raw_hex.matrix,'hex');
      if(Array.from({length:8},(_,i)=>bytes.readDoubleLE(i*8)).some(v=>!Number.isFinite(v))){assert.match(actual.error,/nonfinite|finite stage/);reports.push({name,rejectedOriginalNonfinite:true});continue;}
    }
    assert.ok(!actual.error,name+': '+actual.error);
    const reference=JSON.parse(execFileSync(python,['tools/reference_two_port.py'],{input:JSON.stringify(actual),encoding:'utf8',maxBuffer:16*1024*1024}));
    assert.equal(reference.mpmath_version,'1.3.0');assert.equal(reference.groups,actual.groups);
    const n=actual.groups,pads=actual.pad_capacitance_matrix?.length??0,values=[...columnMajor(actual.capacitance_matrix),...columnMajor(actual.inductance_admittance_matrix),...(pads?columnMajor(actual.pad_capacitance_matrix):[])],bytes=Buffer.alloc(8+values.length*8);
    bytes.writeInt32LE(n);bytes.writeInt32LE(pads,4);values.forEach((v,i)=>bytes.writeDoubleLE(v,8+i*8));writeFileSync('build/portable-two-port/shunts.bin',bytes);
    const native=JSON.parse(execFileSync('build/portable-math/gmd-native',['--shunts','build/portable-two-port/shunts.bin'],{encoding:'utf8'}));
    const nativePorts=JSON.parse(execFileSync('build/portable-math/gmd-native',['--folded-ports','build/portable-two-port/shunts.bin'],{encoding:'utf8'}));
    const pointer=math._malloc(values.length*8);let software,softwarePorts;
    try{math.HEAPF64.set(values,pointer/8);software=JSON.parse(math.UTF8ToString(math._probe_pi_shunts(pointer,pointer+16*n*n,n,pads?pointer+32*n*n:0,pads)));softwarePorts=JSON.parse(math.UTF8ToString(math._probe_folded_ports(pointer,pointer+16*n*n,n,pads?pointer+32*n*n:0,pads)));}finally{math._free(pointer);}
    assert.deepEqual(native,software,name+' native/WASM common-mode shunts');assert.deepEqual(software,actual.pi_shunts,name+' production common-mode integration');
    assert.deepEqual(nativePorts,softwarePorts,name+' native/WASM endpoint solve');
    for(const [key,value]of Object.entries(softwarePorts))assert.deepEqual(actual[key],value,name+' production '+key);
    for(const key of ['first','second'])software[key].forEach((v,i)=>{const expected=reference.circuit_shunts[key][i];assert.ok(relative(v,expected)<=1e-10,name+' independent '+key+' shunt '+i);assert.equal(bits(v),bits(native[key][i]));});
    const stages={};for(const key of ['node_capacitance_matrix','with_pad_admittance_matrix','matrix'])if(reference[key]){stages[key]=compareMatrix(actual[key],reference[key],name+' '+key);assert.ok(stages[key]<=1e-14,name+' endpoint reduction '+key+' error '+stages[key]);}
    const metrics=reference.reported_metrics;let metricError=0,circuitMetricError=0;const circuitMetricErrors={};
    for(const [key,value]of Object.entries(metrics.pi)){
      if(value===null)assert.equal(actual.pi[key],null,name+' '+key+' classification');
      else {assert.ok(actual.pi[key]!==null,name+' '+key+' finite');const error=relative(actual.pi[key],value);assert.ok(error<=1e-10,name+' '+key+' metric error '+error);metricError=Math.max(metricError,error);}
      const circuit=reference.circuit_metrics.pi[key];
      assert.equal(circuit===null,actual.pi[key]===null,name+' circuit '+key+' classification');
      if(circuit!==null&&actual.pi[key]!==null){circuitMetricErrors[key]=relative(actual.pi[key],circuit);assert.ok(circuitMetricErrors[key]<=1e-10,name+' circuit '+key+' error '+circuitMetricErrors[key]);circuitMetricError=Math.max(circuitMetricError,circuitMetricErrors[key]);}
    }
    if(metrics.resonance_hz===null)assert.equal(actual.resonance_hz,null,name+' resonance classification');
    else assert.ok(relative(actual.resonance_hz,metrics.resonance_hz)<=1e-10,name+' resonance');
    const impedanceError=compareMatrix(actual.input_impedance,metrics.input_impedance,name+' input impedance');
    const circuit=reference.circuit_metrics;assert.equal(circuit.resonance_hz===null,actual.resonance_hz===null,name+' circuit resonance classification');
    const circuitResonanceError=circuit.resonance_hz===null?0:relative(actual.resonance_hz,circuit.resonance_hz);assert.ok(circuitResonanceError<=1e-10,name+' circuit resonance error '+circuitResonanceError);
    const circuitImpedanceError=compareMatrix(actual.input_impedance,circuit.input_impedance,name+' circuit input impedance');
    reports.push({name,groups:actual.groups,stages,maximumReportedMetricRelativeError:metricError,maximumCircuitMetricRelativeError:circuitMetricError,circuitMetricErrors,impedanceError,circuitResonanceError,circuitImpedanceError});
    console.log(name+': independent vertex elimination, pad subtraction and full precision metrics pass');
  }catch(error){failures.push({name,message:error.message,actual});console.log(name+': '+error.message);}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-two-port.json',JSON.stringify({reference:'80-digit endpoint Schur complement; physical C and branch Y inputs',relativeTolerance:1e-10,reports,failures},null,2)+'\n');
assert.equal(failures.length,0,failures.map(v=>v.name+': '+v.message).join('\n'));
console.log(`Portable PI2/PI4: ${reports.length} cases, independent circuit matrices and full precision metrics; ${failures.length} failures`);
