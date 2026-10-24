import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const input=readFileSync('tests/fixtures/eddy-cases.json','utf8'),cases=JSON.parse(input).cases,original=JSON.parse(readFileSync('tests/fixtures/original-eddy.json','utf8')),text=readFileSync('web/technology/CMOS.tek','utf8'),hash=value=>createHash('sha256').update(value).digest('hex');
assert.equal(hash(input),original.case_definition_sha256);assert.equal(hash(text),original.technology_sha256);
const kernel=await createKernel(),technology=parseTechnology(text);let fields=0,printedFields=0,evaluations=0;const reports=[],failures=[];
function exact(a,b,path='root'){if(typeof a==='number'){assert.equal(a,b,path);fields++;}else if(a && typeof a==='object'){assert.deepEqual(Object.keys(a),Object.keys(b),path);for(const key of Object.keys(a))exact(a[key],b[key],path+'.'+key);}else assert.equal(a,b,path);}
const printed=value=>Object.is(value,-0)?'-0.00000000000000000000':value.toFixed(20);
for(const item of cases) {
  configureKernel(kernel,technology);createGeometry(kernel,item.id,item.keywords);
  const f=kernel._normalize_value(item.frequency_ghz,5,.1,0),params=[0,f,...item.cells,1024,1,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000],start=performance.now();
  const actual=JSON.parse(kernel.UTF8ToString(kernel._simulate_eddy(...params))),native=JSON.parse(execFileSync('build/native-geometry',['--eddy',item.id,item.keywords,f,...item.cells,1,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000].map(String),{encoding:'utf8',maxBuffer:16*1024*1024,timeout:90000})),reference=original.cases.find(c=>c.name===item.name);
  try {
    exact(actual,native,item.name);
    if(reference.nonfinite){assert.match(actual.error,/nonfinite/);reports.push({name:item.name,original_nonfinite:true,recovered:true});console.log(item.name+': original nonfinite result rejected consistently');continue;}
    assert.ok(!actual.error,item.name+': '+actual.error);assert.equal(actual.filaments,reference.filaments);assert.equal(actual.matrix.length,reference.matrix.length);
    actual.matrix.forEach((row,i)=>row.forEach((z,j)=>z.forEach((value,k)=>{assert.equal(printed(value),printed(reference.matrix[i][j][k]),`${item.name} original matrix [${i}][${j}][${k}]`);printedFields++;})));
    assert.equal(actual.eddy.quadrature_status.reduce((a,b)=>a+b,0),reference.quadpack_nonconvergence_messages,item.name+' native convergence diagnostics');
    evaluations+=actual.eddy.evaluations;reports.push({name:item.name,evaluations:actual.eddy.evaluations,filaments:actual.filaments,conductors:actual.conductors,quadrature_status:actual.eddy.quadrature_status,sample_hash:actual.eddy.sample_hash,value_hash:actual.eddy.value_hash,milliseconds:performance.now()-start});
    console.log(`${item.name}: exact native fields/traces and original LMAT output (${actual.eddy.evaluations} evaluations)`);
  }catch(error){failures.push({name:item.name,error:error.message,actual,native});console.log(item.name+': MISMATCH '+error.message);}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/eddy-parity.json',JSON.stringify({fields,printedFields,evaluations,cases:reports,failures},null,2)+'\n');
console.log(`Eddy: ${fields} native numeric fields, ${printedFields} original printed matrix fields, ${evaluations} traced Green evaluations; ${failures.length} mismatches.`);
assert.equal(failures.length,0,failures.map(c=>c.name+': '+c.error).join('\n'));
