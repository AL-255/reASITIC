import assert from 'node:assert/strict';import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';import {execFileSync} from 'node:child_process';import {createHash} from 'node:crypto';
import createKernel from '../dist/asitic.js';import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const input=readFileSync('tests/fixtures/pad-cases.json','utf8'),cases=JSON.parse(input).cases,original=JSON.parse(readFileSync('tests/fixtures/original-pad.json','utf8')),text=readFileSync('web/technology/CMOS.tek','utf8'),hash=v=>createHash('sha256').update(v).digest('hex');
assert.equal(hash(input),original.case_definition_sha256);assert.equal(hash(text),original.technology_sha256);
const kernel=await createKernel(),technology=parseTechnology(text),reports=[],rounding=[],failures=[],nativeNonfinite=[];let fields=0,raw=0,printed=0,rawCacheBytes=0;
const known=new Set(['wire-self-ground','wire-subdivision','offset-pads-two-layer','offset-pads-reflection']);
function compare(a,b,path,disagreements){
 if(typeof a==='number'){fields++;if(!Object.is(a,b))disagreements.push({path,actual:a,native:b});}
 else if(a&&typeof a==='object'){assert.deepEqual(Object.keys(a),Object.keys(b),path);for(const key of Object.keys(a))compare(a[key],b[key],path+'.'+key,disagreements);}
 else assert.equal(a,b,path);
}
function rawValues(result,key){return key==='pi'?Object.values(result.pi):['capacitance_matrix','inductance_admittance_matrix','pad_capacitance_matrix'].includes(key)?result[key][0].flatMap((_,j)=>result[key].flatMap(row=>row[j])):result[key].flat(2);}
for(const item of cases){
 configureKernel(kernel,technology);let layout;for(const [i,c]of [item.device,...item.pads].entries())layout=createGeometry(kernel,c.id,c.keywords,'',{append:i>0});
 let device=layout.shapes.findIndex(s=>s.name==='A'),pad1=layout.shapes.findIndex(s=>s.name==='P1'),pad2=layout.shapes.findIndex(s=>s.name==='P2'),ground=-1;
 if(item.ground?.startsWith('@'))ground=item.ground==='@1'?pad1:item.ground==='@2'?pad2:device;
 else if(item.ground){layout=createGeometry(kernel,2,item.ground,'',{append:true});++device;++pad1;++pad2;ground=0;}
 const f=kernel._normalize_value(item.frequency_ghz,5,.1,0),cells=[...item.cells,...item.contact_cells].map(v=>kernel._normalize_value(v,7,.1,1)),start=performance.now();
 const actual=JSON.parse(kernel.UTF8ToString(kernel._simulate_pad_network(device,pad1,pad2,ground,f,...cells,512,512,...item.grid,512,1024,item.eddy_mode,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000)));
 const native=JSON.parse(execFileSync('build/native-geometry',['--pads',item.device.id,item.device.keywords,item.pads[0].id,item.pads[0].keywords,item.pads[1].id,item.pads[1].keywords,item.ground??'',f,...cells,...item.grid,item.eddy_mode,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000].map(String),{encoding:'utf8',maxBuffer:16*1024*1024})),reference=original.cases.find(c=>c.name===item.name);
 try{
  const matrix=Buffer.from(reference.raw_hex.matrix,'hex'),nonfinite=Array.from({length:8},(_,i)=>matrix.readDoubleLE(i*8)).some(v=>!Number.isFinite(v));
  if(nonfinite){assert.match(actual.error,/nonfinite|finite stage/);assert.equal(actual.error,native.error);nativeNonfinite.push(item.name);console.log(item.name+': original raw admittance is nonfinite; WASM and x87 reject without publishing a finite model');continue;}
  assert.ok(!actual.error,actual.error);assert.ok(!native.error,native.error);const disagreements=[];compare(actual,native,item.name,disagreements);
  assert.equal(actual.green_table_hash,reference.green_table_hash);rawCacheBytes+=reference.raw_cache_bytes;
  const rawDisagreements=[];
  for(const [key,hex]of Object.entries(reference.raw_hex)){
   const values=rawValues(actual,key),nativeValues=rawValues(native,key),bytes=Buffer.from(hex,'hex');assert.equal(values.length*8,bytes.length,item.name+' '+key+' size');
   values.forEach((value,i)=>{const expected=bytes.readDoubleLE(i*8);if(!Number.isFinite(expected)){assert.equal(value,null);assert.equal(nativeValues[i],null);return;}
    const n=Buffer.alloc(8);n.writeDoubleLE(nativeValues[i]);assert.equal(n.toString('hex'),bytes.subarray(i*8,i*8+8).toString('hex'),`${item.name} original/x87 ${key}[${i}]`);raw++;
    const a=Buffer.alloc(8);a.writeDoubleLE(value);if(!a.equals(n))rawDisagreements.push({path:`${key}[${i}]`,actual:a.toString('hex'),original:n.toString('hex')});
   });
  }
  for(const key of ['capacitance_matrix','inductance_admittance_matrix','node_capacitance_matrix','pad_capacitance_matrix']){
   assert.equal(actual[key].length,reference[key].length);actual[key].forEach((row,i)=>row.forEach((z,j)=>z.forEach((value,k)=>{assert.equal(Number(value.toPrecision(10)),reference[key][i][j][k],`${item.name}.${key}[${i}][${j}][${k}]`);printed++;})));
  }
  const p=actual.pi;for(const [i,key]of ['quality_first','quality_second','quality_differential'].entries()){assert.equal(Number(p[key].toFixed(3)),Number(reference.quality_printed[i]));printed++;}
  [p.inductance_h/1e-9,p.resistance,p.capacitance_second_f/1e-15,p.resistance_second,p.capacitance_first_f/1e-15,p.resistance_first,actual.resonance_hz===null?null:actual.resonance_hz/1e9].forEach((v,i)=>{assert.equal(v===null?NaN:Number(i===6?v.toFixed(2):v.toPrecision(3)),Number(reference.pi_printed[i]),item.name+' printed pi '+i);printed++;});
  actual.input_impedance.forEach((z,i)=>z.forEach((v,j)=>{assert.equal(Number(v.toFixed(3)),Number(reference.input_impedance_printed[i][j]));printed++;}));
  for(const [i,key]of ['shunt_single','shunt_differential'].entries()){assert.equal(p[key]===null?NaN:Number(p[key].toFixed(6)),Number.parseFloat(reference.shunt_printed[i]));printed++;}
  assert.equal(actual.groups,reference.groups);assert.equal(actual.pad_capacitance_matrix.length,reference.pad_groups);assert.equal(actual.eddy.quadrature_status.reduce((a,b)=>a+b,0),reference.quadpack_nonconvergence_messages);
  if(disagreements.length||rawDisagreements.length){if(!known.has(item.name))console.log(item.name+' first discrepancies '+JSON.stringify({values:disagreements.slice(0,3),raw:rawDisagreements.slice(0,4)}));assert.ok(known.has(item.name),'New unclassified PI4 rounding discrepancy: '+item.name);rounding.push({name:item.name,disagreements,rawDisagreements});console.log(item.name+': UNRESOLVED hardware rounding; original/x87 raw fields exact; '+rawDisagreements.length+' WASM raw fields differ');}
  else console.log(item.name+': exact WASM/x87/original raw fields, printed models and Green tables');
  reports.push({name:item.name,groups:actual.groups,filaments:actual.filaments,contacts:actual.contacts,pad_contacts:actual.pad_contacts,evaluations:actual.eddy.evaluations,milliseconds:performance.now()-start});
 }catch(error){failures.push({name:item.name,message:error.message,actual,native});console.log(item.name+': FAILURE '+error.message);}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/pad-parity.json',JSON.stringify({fields,raw,printed,rawCacheBytes,cases:reports,nativeNonfinite,rounding,failures},null,2)+'\n');
console.log(`PI4: ${fields} x87 comparisons, ${raw} raw original/x87 fields, ${printed} printed fields; ${nativeNonfinite.length} native nonfinite cases; ${rounding.length} UNRESOLVED WASM rounding cases; ${failures.length} other failures`);
assert.equal(failures.length,0,JSON.stringify(failures));if(process.argv.includes('--strict'))assert.equal(rounding.length,0,'Full-goal PI4 identity remains unproven');
