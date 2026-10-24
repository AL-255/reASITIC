import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import make from '../dist/asitic.js';
import {configureKernel,parseTechnology,createGeometry} from '../web/technology.mjs';

const source=readFileSync('web/technology/CMOS.tek','utf8'),technology=parseTechnology(source),fixture=JSON.parse(readFileSync('tests/fixtures/original-maxnw.json','utf8')),kernel=await make(),reports=[];
assert.equal(createHash('sha256').update(source).digest('hex'),fixture.technology_sha256);
for(const item of fixture.cases){
  configureKernel(kernel,technology);const layout=createGeometry(kernel,2,item.keywords,'');
  const cells=[1000,item.width_cell,item.thickness_cell].map(v=>kernel._normalize_value(v,7,.1,1));
  assert.equal(kernel._set_max_width_cells(item.max_width_cells),1);
  const solve=()=>JSON.parse(kernel.UTF8ToString(kernel._simulate(0,1e9,...cells,cells[0],1024)));
  kernel._set_reference_numerics(1);const historical=solve();kernel._set_reference_numerics(0);const actual=solve();
  assert.ok(!historical.error,item.name+': '+historical.error);assert.ok(!actual.error,item.name+': '+actual.error);
  assert.equal(historical.filaments,item.filaments);assert.deepEqual(historical.matrix,item.matrix,item.name+' original complete LMAT matrix');
  const native=JSON.parse(execFileSync('build/native-geometry',['--maxnw',item.keywords,1e9,...cells,item.max_width_cells].map(String),{encoding:'utf8'}));assert.ok(!native.error,native.error);assert.equal(native.filaments,item.filaments);
  assert.equal(actual.filaments,item.filaments);assert.equal(actual.max_width_cells,item.max_width_cells===500?undefined:item.max_width_cells);
  const scale=Math.max(...item.matrix.flat(2).map(Math.abs));
  actual.matrix.flat(2).forEach((v,i)=>{assert.ok(Math.abs(v-item.matrix.flat(2)[i])<=scale*1e-10,item.name+' production matrix');assert.ok(Math.abs(v-native.matrix.flat(2)[i])<=scale*1e-10,item.name+' native/WASM matrix');});
  assert.deepEqual(solve(),actual,'Historical mode contaminated production');
  const next=createGeometry(kernel,2,'NAME=VERIFY:LEN=4:WID=2:METAL=M5:XORG=200:YORG=200','',{append:true});assert.deepEqual(next.shapes.slice(1),layout.shapes,'Width grading mutated source geometry');
  reports.push({name:item.name,filaments:actual.filaments,resistance:actual.resistance,reference_resistance:historical.resistance});
}
configureKernel(kernel,technology);createGeometry(kernel,2,'NAME=A:LEN=40:WID=24:METAL=M5:XORG=100:YORG=100','');
const solve=(width=1)=>JSON.parse(kernel.UTF8ToString(kernel._simulate(0,1e9,1000,width,1,1000,1024)));
for(const limit of [1,2]){assert.equal(kernel._set_max_width_cells(limit),1);assert.match(solve().error,/MAXNW.*nonfinite/);assert.equal(solve(50).filaments,1,'Small valid uniform width cap was rejected');}
assert.equal(kernel._set_max_width_cells(3),1);const expected=solve();
for(const limit of [0,-1,-2147483648]){assert.equal(kernel._set_max_width_cells(limit),0);assert.deepEqual(solve(),expected,'Rejected MAXNW changed the mesh policy');}
assert.match(solve(1e-300).error,/integer range/,'An overflowing native count silently coarsened the mesh');assert.deepEqual(solve(),expected);
assert.equal(kernel._set_max_width_cells(16777217),1);assert.equal(solve().max_width_cells,16777217,'Integer MAXNW passed through float32 scalar parsing');
configureKernel(kernel,technology);createGeometry(kernel,2,'NAME=A:LEN=40:WID=24:METAL=M5:XORG=100:YORG=100','');assert.equal(solve().max_width_cells,undefined,'Technology reset leaked a nondefault width policy');assert.equal(solve().filaments,24);
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/maxnw.json',JSON.stringify({original_cases:reports.length,original_matrices_exact:true,native_wasm_and_production:true,nonfinite_grading_rejected:true,integer_overflow_rejected:true,reports},null,2)+'\n');
console.log(`MAXNW: ${reports.length} original full-precision matrices and counts, native/WASM production agreement, graded/uniform thresholds, integer range, zero-edge rejection, policy recovery and immutable geometry pass`);
