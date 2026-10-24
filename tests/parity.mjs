import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
import {writeGDS,polygons} from '../web/gds.mjs';
const kernel=await createKernel(),technology=parseTechnology(readFileSync(new URL('../web/technology/CMOS.tek',import.meta.url),'utf8'));
configureKernel(kernel,technology);mkdirSync('build/evidence',{recursive:true});
const cases=[];
for(const [name,kind] of [['square',0],['polygon',1],['wire',2],['symmetric-square',3],['symmetric-polygon',4],['ring',5],['transformer',6],['3d-transformer',7],['multimetal',8],['via',9]]){
  for(const rotation of [0,.37])cases.push({name:`${name}-${rotation}`,kind,parameters:[100,90,3,kind===5?20:2,2,50,8,3,2,rotation,15,1.5]});
}
let mismatches=0,components=0;
const report=[];
function compare(a,b,path,errors){
  if(typeof a==='number'){components++;if(!Object.is(a,b)){assert.ok(Math.abs(b-a)<=1e-12+1e-13*Math.abs(a),path+' exceeds the regression tolerance');mismatches++;errors.push({path,native:a,wasm:b,delta:b-a});}return;}
  if(Array.isArray(a)){assert.equal(b.length,a.length,path);a.forEach((x,i)=>compare(x,b[i],`${path}[${i}]`,errors));return;}
  if(a && typeof a==='object'){for(const key of Object.keys(a))compare(a[key],b[key],`${path}.${key}`,errors);return;}
  assert.equal(b,a,path);
}
for(const item of cases){
  const run=spawnSync('build/native-geometry',[item.kind,...item.parameters].map(String),{encoding:'utf8',timeout:20000});
  assert.equal(run.status,0,`${item.name}: native failed ${run.stderr}`);
  const original=JSON.parse(run.stdout),candidate=generateGeometry(kernel,item.kind,item.parameters);delete candidate.warnings;
  assert.ok(!original.error,`${item.name}: ${original.error}`);const errors=[];compare(original,candidate,item.name,errors);
  const gds=writeGDS(candidate,technology);assert.ok(gds.length>100);assert.ok([...polygons(candidate,technology)].length>0);
  writeFileSync(`build/evidence/${item.name}.gds`,gds);
  writeFileSync(`build/evidence/${item.name}.json`,JSON.stringify(candidate));
  report.push({name:item.name,exact:!errors.length,mismatches:errors.length,examples:errors.slice(0,4)});
  console.log(`${item.name}: ${errors.length?errors.length+' differing numeric fields':'exact'}`);
}
const summary={components,mismatches,cases:report};writeFileSync('build/evidence/parity.json',JSON.stringify(summary,null,2)+'\n');
console.log(`${components} numeric fields; ${mismatches} mismatches. See build/evidence/parity.json.`);
// A milestone is explicitly not the final fidelity gate. --strict is the gate
// for declaring numerical parity; it cannot silently accept known differences.
if(process.argv.includes('--strict'))assert.equal(mismatches,0,'Original numerical parity is incomplete');
for(const row of report)if(row.name!=='symmetric-polygon-0')assert.ok(row.exact,`${row.name} regressed`);
assert.ok(report.find(r=>r.name==='symmetric-polygon-0').mismatches<=24);
assert.throws(()=>generateGeometry(kernel,0,[100,90,0,2,2,50,8,3,2,0,15,1.5]));
