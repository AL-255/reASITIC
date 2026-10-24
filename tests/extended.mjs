import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import createKernel from './reference-kernel.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const input=readFileSync('tests/fixtures/extended-cases.json','utf8'),cases=JSON.parse(input).cases,original=JSON.parse(readFileSync('tests/fixtures/original-extended.json','utf8')),text=readFileSync('web/technology/CMOS.tek','utf8'),hash=value=>createHash('sha256').update(value).digest('hex');
assert.equal(hash(input),original.case_definition_sha256);assert.equal(hash(text),original.technology_sha256);
const kernel=await createKernel(),technology=parseTechnology(text);let fields=0,printed=0,raw=0;const reports=[],failures=[];
function exact(a,b,path='root'){if(typeof a==='number'){assert.equal(a,b,path);fields++;}else if(a && typeof a==='object'){assert.deepEqual(Object.keys(a),Object.keys(b),path);for(const key of Object.keys(a))exact(a[key],b[key],path+'.'+key);}else assert.equal(a,b,path);}
for(const item of cases){
  configureKernel(kernel,technology);createGeometry(kernel,item.id,item.keywords);if(item.ground)createGeometry(kernel,2,item.ground,'',{append:true});
  const f=kernel._normalize_value(item.frequency_ghz,5,.1,0),winding=item.ground?1:0,ground=item.ground?0:item.self_ground?0:-1;
  const params=[winding,ground,f,...item.cells,...item.contact_cells,512,512,...item.grid,512,1024,item.eddy_mode,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000];
  const start=performance.now(),actual=JSON.parse(kernel.UTF8ToString(kernel._simulate_extended_network(...params)));
  let native;
  if(item.self_ground){ // A distinct flag is passed through the native harness.
    native=JSON.parse(execFileSync('build/native-geometry',['--pix',item.id,item.keywords,2,'@self',f,...item.cells,...item.contact_cells,...item.grid,512,1024,item.eddy_mode,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));
  }else native=JSON.parse(execFileSync('build/native-geometry',['--pix',item.id,item.keywords,2,item.ground??'',f,...item.cells,...item.contact_cells,...item.grid,512,1024,item.eddy_mode,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));
  const reference=original.cases.find(c=>c.name===item.name);
  try{
    exact(actual,native,item.name);assert.equal(actual.green_table_hash,reference.green_table_hash,item.name+' raw Green table bytes');assert.ok(!actual.error,item.name+': '+actual.error);
    for(const key of ['capacitance_matrix']){assert.equal(actual[key].length,reference[key].length);actual[key].forEach((row,i)=>row.forEach((z,j)=>z.forEach((value,k)=>{assert.equal(Number(value.toPrecision(10)),reference[key][i][j][k],`${item.name}.${key}[${i}][${j}][${k}]`);printed++;})));}
    for(const [key,hex] of Object.entries(reference.raw_hex)){
      const source=key==='pi'?Object.values(actual.pi):key==='capacitance_matrix'||key==='inductance_impedance_matrix'?actual[key][0].flatMap((_,j)=>actual[key].flatMap(row=>row[j])):actual[key].flat(2);
      const bytes=Buffer.from(hex,'hex');assert.equal(source.length*8,bytes.length,item.name+' raw '+key+' size');
      source.forEach((value,i)=>{const native=bytes.readDoubleLE(i*8);if(Number.isFinite(native)){const got=Buffer.alloc(8);got.writeDoubleLE(value);assert.equal(got.toString('hex'),bytes.subarray(i*8,i*8+8).toString('hex'),`${item.name} raw ${key}[${i}]`);raw++;}else assert.equal(value,null,item.name+' undefined '+key);});
    }
    const p=actual.pi;
    for(const [i,key]of ['quality_first','quality_second','quality_differential'].entries()){assert.equal(Number(p[key].toFixed(3)),Number(reference.quality_printed[i]),item.name+' '+key);printed++;}
    const metrics=[p.inductance_h/1e-9,p.resistance,p.capacitance_second_f/1e-15,p.resistance_second,p.capacitance_first_f/1e-15,p.resistance_first,actual.resonance_hz===null?null:actual.resonance_hz/1e9];
    metrics.forEach((value,i)=>{const got=value===null?NaN:Number(i===6?value.toFixed(2):value.toPrecision(3));assert.equal(got,Number(reference.pi_printed[i]),item.name+' printed pi '+i);printed++;});
    actual.input_impedance.forEach((z,i)=>z.forEach((value,j)=>{assert.equal(Number(value.toFixed(3)),Number(reference.input_impedance_printed[i][j]),item.name+' printed Zin');printed++;}));
    for(const [i,key] of ['shunt_single','shunt_differential'].entries()){const expected=Number.parseFloat(reference.shunt_printed[i]);assert.equal(p[key]===null?NaN:Number(p[key].toFixed(6)),expected,item.name+' printed '+key);printed++;}
    assert.equal(actual.eddy.quadrature_status.reduce((a,b)=>a+b,0),reference.quadpack_nonconvergence_messages);
    reports.push({name:item.name,groups:actual.groups,filaments:actual.filaments,contacts:actual.contacts,evaluations:actual.eddy.evaluations,milliseconds:performance.now()-start});console.log(item.name+': native fields exact; original stage matrices and pi/impedance reports match');
  }catch(error){failures.push({name:item.name,message:error.message,actual,native});console.log(item.name+': MISMATCH '+error.message);}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/extended-parity.json',JSON.stringify({fields,printed,raw,cases:reports,failures},null,2)+'\n');
console.log(`PIX: ${fields} native numeric fields; ${printed} original printed fields; ${raw} original raw binary64 fields; ${failures.length} failures`);assert.equal(failures.length,0,failures.map(c=>c.name+': '+c.message).join('\n'));
