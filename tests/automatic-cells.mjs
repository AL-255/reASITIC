import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import createKernel from '../dist/asitic.js';
import {configureKernel,parseTechnology,createGeometry} from '../web/technology.mjs';
import {portSweepRecord} from '../web/port-sweep.mjs';
import {createHash} from 'node:crypto';
import {solverCells} from '../web/mesh-policy.mjs';

const kernel=await createKernel(),source=readFileSync('web/technology/CMOS.tek','utf8'),technology=parseTechnology(source),fixture=JSON.parse(readFileSync('tests/fixtures/original-automatic-cells.json','utf8'));
assert.equal(createHash('sha256').update(source).digest('hex'),fixture.technology_sha256);
const bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');},number=hex=>Buffer.from(hex,'hex').readDoubleLE();
const values=p=>[p.mesh.length,p.mesh.width,p.mesh.thickness,p.contact_mesh.length,p.contact_mesh.width];
const oracleInputs=[],oracleActual=[];let original=0,native=0;
for(const item of fixture.cases){
  const tech=structuredClone(technology),top=tech.metals.at(-1);top.thickness=item.thickness??top.thickness;top.sheet_milliohms=item.sheet_milliohms??top.sheet_milliohms;configureKernel(kernel,tech);
  for(const row of item.updates){
    const [hz,a,b,t,r]=['frequency','alpha','beta','thickness','resistance'].map(k=>number(row[k+'_hex']));
    const actual=JSON.parse(kernel.UTF8ToString(kernel._automatic_cell_limits(hz,a,b))),host=JSON.parse(execFileSync('build/native-geometry',['--automatic',hz,a,b,top.thickness,top.sheet_milliohms].map(String),{encoding:'utf8'}));
    assert.deepEqual(actual,host,'Software/native updater mismatch');native++;
    assert.equal(values(actual).map(bits).join(''),row.limits_hex,item.name+' original five raw limits');original++;
    oracleInputs.push([hz,a,b,t,r]);oracleActual.push(actual);
  }
}
// The original low-bit audit is explicit and isolated from production.
let originalFiles=0;
kernel._set_reference_numerics(1);
for(const item of fixture.cases){
  const tech=structuredClone(technology);tech.domain={...tech.domain,x:fixture.domain_um,y:fixture.domain_um,cells_x:fixture.fft_cells,cells_y:fixture.fft_cells};
  tech.metals.at(-1).thickness=item.thickness??1;tech.metals.at(-1).sheet_milliohms=item.sheet_milliohms??30;configureKernel(kernel,tech);
  createGeometry(kernel,2,'NAME=A:LEN=20:WID=2:METAL=M5:XORG=4:YORG=8','',{domain:tech.domain});
  const samples=[];
  for(const row of item.updates.slice(0,3)){
    const hz=number(row.frequency_hex),selection=solverCells(kernel,{automatic_cells:{alpha:item.alpha,beta:item.beta},nativeInput:true},hz,'network');
    const result=JSON.parse(kernel.UTF8ToString(kernel._simulate_network(0,0,hz,...selection.cells,fixture.domain_um,fixture.domain_um,fixture.fft_cells,fixture.fft_cells,128,256,0,0,0,0,250000)));assert.ok(!result.error,result.error);samples.push(result);
  }
  assert.equal(portSweepRecord(samples,'s'),item.scattering_record,item.name+' original full-precision S file');originalFiles++;
}
kernel._set_reference_numerics(0);
configureKernel(kernel,technology);
// Sweep well beyond UI frequencies and include small/large coefficients. No
// host libm or instruction-specific tolerance profile participates in policy.
for(let i=0;i<150;i++){
  const hz=10**(-180+360*i/149),alpha=10**(-20+(i%41)),beta=10**(-20+((i*7)%41));
  const actual=JSON.parse(kernel.UTF8ToString(kernel._automatic_cell_limits(hz,alpha,beta))),host=JSON.parse(execFileSync('build/native-geometry',['--automatic',hz,alpha,beta].map(String),{encoding:'utf8'}));
  assert.ok(!actual.error,actual.error);assert.deepEqual(actual,host);native++;
  oracleInputs.push([hz,alpha,beta,1,number('b81e85eb51b89e3f')]);oracleActual.push(actual);
}
const expected=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_automatic_cells.py'],{input:JSON.stringify(oracleInputs),encoding:'utf8'}));
let maxRelative=0;
for(const [i,[length,width]]of expected.entries())for(const [a,b]of [[oracleActual[i].mesh.length,length],[oracleActual[i].mesh.width,width]]){
  const relative=Math.abs(a-b)/Math.abs(b);maxRelative=Math.max(maxRelative,relative);assert.ok(relative<=Number.EPSILON,'Independent wavelength/skin-depth formula');
}
const selection=solverCells(kernel,{nativeInput:true,grid:10,snap:true,automatic_cells:{alpha:.03,beta:.6},mesh:{length:NaN,width:NaN,thickness:NaN},contact_mesh:{length:NaN,width:NaN}},1e9,'network');
assert.equal(selection.policy.alpha,Math.fround(.03));assert.equal(selection.policy.beta,Math.fround(.6));
assert.equal(selection.cells[1],selection.policy.mesh.width);assert.notEqual(selection.cells[1],kernel._normalize_value(selection.cells[1],7,10,1),'Automatic mesh was snapped/reparsed');
const expectedGood=JSON.parse(kernel.UTF8ToString(kernel._automatic_cell_limits(1e9,.03,.6)));
for(const input of [[0,.03,.6],[-1,.03,.6],[Infinity,.03,.6],[1e9,0,.6],[1e9,.03,NaN],[Number.MIN_VALUE,.03,.6]]){
  assert.ok(JSON.parse(kernel.UTF8ToString(kernel._automatic_cell_limits(...input))).error);
  assert.deepEqual(JSON.parse(kernel.UTF8ToString(kernel._automatic_cell_limits(1e9,.03,.6))),expectedGood);
}
// Select the last technology metal, independent of the winding's layer.
const edited=structuredClone(technology);edited.metals[0].thickness=100;edited.metals[0].sheet_milliohms=10000;configureKernel(kernel,edited);assert.deepEqual(JSON.parse(kernel.UTF8ToString(kernel._automatic_cell_limits(1e9,.03,.6))),expectedGood);
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/automatic-cells.json',JSON.stringify({original_updates:original,original_scattering_files:originalFiles,native_wasm_cases:native,independent_references:expected.length,max_relative_error:maxRelative},null,2)+'\n');
console.log(`Automatic cells: ${original} original raw updates, ${originalFiles} original full-precision S files, ${native} native/WASM cases, ${expected.length} independent physics references; no dimension reparsing, last-metal rule and rejection recovery pass`);
