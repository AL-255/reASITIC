import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const kernel=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')),cases=JSON.parse(readFileSync('tests/fixtures/portable-three-port-cases.json','utf8')).cases,reports=[];
const nativeCapture=JSON.parse(readFileSync('tests/fixtures/original-three-port-backing.json','utf8')).histories[0];
const python=process.env.MPMATH_PYTHON??'python3';
function close(actual,expected,scale,message){assert.ok(Math.abs(actual-expected)<=Math.max(scale,1e-300)*1e-10,message+': '+actual+' vs '+expected);}
for(const item of cases){
  const v=item.case;configureKernel(kernel,technology);let before;
  for(const [i,c]of v.creators.entries())before=createGeometry(kernel,c.id,c.keywords,'',{append:i>0});
  before=createGeometry(kernel,2,v.ground,'',{append:true});const indices=v.names.map(name=>before.shapes.findIndex(s=>s.name===name)),ground=before.shapes.findIndex(s=>s.name==='G');
  const hz=kernel._normalize_value(v.frequency_ghz,5,.1,0),cells=[...v.cells,...v.contact_cells].map(value=>kernel._normalize_value(value,7,.1,1));
  const args=[...indices,ground,hz,...cells,512,512,...v.grid,512,1024,v.eddy_mode,v.layers.length,v.layers[0]??0,v.layers[1]??0,250000];
  const solve=()=>JSON.parse(kernel.UTF8ToString(kernel._simulate_three_port(...args))),actual=solve();assert.ok(!actual.error,item.name+': '+actual.error);
  const n=actual.node_count;assert.equal(n,actual.capacitance_matrix.length);assert.equal(n,actual.inductance_impedance_matrix.length);assert.equal(n,actual.winding_groups.reduce((a,b)=>a+b,0));assert.equal(actual.system_matrix.length,2*n);assert.ok(!actual.backing);
  if(item.name==='wires-default-separate-reference'){
    // The original capture contains the same reduced physical matrices before
    // the erroneous larger-stride node read. Validate their real prefixes.
    for(const [key,hex]of [['capacitance_matrix',nativeCapture.capacitance_hex],['inductance_impedance_matrix',nativeCapture.inductance_hex]]){
      const bytes=Buffer.from(hex,'hex'),values=actual[key][0].flatMap((_,j)=>actual[key].flatMap(row=>row[j]));
      const scale=Math.max(...values.map(Math.abs));values.forEach((value,i)=>close(value,bytes.readDoubleLE(i*8),scale,'Original reduced '+key+' prefix '+i));
    }
  }
  const reference=JSON.parse(execFileSync(python,['tools/reference_three_port.py'],{input:JSON.stringify(actual),encoding:'utf8',maxBuffer:16*1024*1024}));assert.equal(reference.mpmath_version,'1.3.0');
  const native=JSON.parse(execFileSync('build/native-geometry',['--three',...v.creators.flatMap(c=>[c.id,c.keywords]),v.ground,hz,...cells,...v.grid,v.eddy_mode,v.layers.length,v.layers[0]??0,v.layers[1]??0,250000].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));assert.ok(!native.error,native.error);assert.equal(native.node_count,n);
  const scale=Math.max(...reference.impedance_matrix.flat(2).map(Math.abs));let error=0;
  actual.impedance_matrix.flat(2).forEach((value,i)=>{const expected=reference.impedance_matrix.flat(2)[i];close(value,expected,scale,item.name+' independent nodal impedance '+i);close(value,native.impedance_matrix.flat(2)[i],scale,item.name+' native/WASM '+i);error=Math.max(error,Math.abs(value-expected));});
  const conversionErrors={};
  for(const key of ['admittance_matrix','scattering_matrix']){
    const expected=reference.conversion[key].flat(2),converted=actual[key].flat(2),magnitude=Math.max(...expected.map(Math.abs));let maximum=0;
    converted.forEach((value,i)=>{assert.ok(Number.isFinite(value),item.name+' '+key+' finite');maximum=Math.max(maximum,Math.abs(value-expected[i]));});
    assert.ok(maximum<=magnitude*1e-14,item.name+' independent '+key+' conversion');conversionErrors[key]=maximum/magnitude;
  }
  for(const [key,expected]of Object.entries(reference.model)){
    const magnitude=Math.max(...expected.filter(v=>v!==null).map(Math.abs),1e-300);
    actual.model[key].forEach((value,i)=>{if(expected[i]===null)assert.equal(value,null);else close(value,expected[i],magnitude,item.name+' model '+key+' '+i);});
  }
  // Changing neighboring allocation contents must not change any numerical
  // input, output, or reported node count.
  const junk=[4096,17408,65536].map(size=>{const p=kernel._malloc(size);assert.ok(p);new Uint8Array(kernel.HEAPF64.buffer,p,size).fill(0xa5);return p;});
  try{assert.deepEqual(solve(),actual,item.name+' depends on neighboring allocations');}finally{junk.forEach(p=>kernel._free(p));}
  kernel._set_reference_numerics(1);assert.match(solve().error,/backing/,'Historical overread still requires its actual process-state input');kernel._set_reference_numerics(0);assert.deepEqual(solve(),actual,'Historical policy contaminated production');
  const after=createGeometry(kernel,2,'NAME=VERIFY:LEN=20:WID=4:METAL=M2:XORG=450:YORG=450','',{append:true});assert.deepEqual(after.shapes.slice(1),before.shapes,item.name+' mutated source geometry');
  reports.push({name:item.name,nodes:n,conversionErrors,maximumAbsoluteImpedanceError:error,referenceDigits:reference.decimal_digits});
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-three-port.json',JSON.stringify({cases:reports.length,reference:'80-decimal-digit independent Schur-complement solve; no native process backing',relativeTolerance:1e-10,conversionRelativeTolerance:1e-14,sourceImmutable:true,allocationIndependent:true,reports},null,2)+'\n');
console.log(`Portable three-port: ${reports.length} separate-reference cases agree with an independent 80-digit nodal reduction and native C++, with S/Y conversions, matrix dimensions, allocation independence, policy recovery and immutable geometry verified`);
