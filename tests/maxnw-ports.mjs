import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import {configureKernel,parseTechnology,createGeometry} from '../web/technology.mjs';
import {portSweepRecord} from '../web/port-sweep.mjs';
const source=readFileSync('web/technology/CMOS.tek','utf8'),technology=parseTechnology(source),fixture=JSON.parse(readFileSync('tests/fixtures/original-maxnw-ports.json','utf8')),kernel=await make();
assert.equal(createHash('sha256').update(source).digest('hex'),fixture.technology_sha256);
let files=0,historicalExact=0,nativeExact=0;const reports=[],historicalDifferences=[],nativeDifferences=[];
const numeric=s=>s.split('\n').filter(l=>l.trim()&&!/^[#!]/.test(l)).flatMap(l=>l.trim().split(/\s+/).map(Number)).slice(1);
// No CPU profiles choose acceptance. Full original records are retained;
// portable gates use their equations/report accuracy. Optional strict modes
// request historical instruction-bit identity as an additional diagnostic.
function compareRecord(record,expected,family,parameter,differences){
  if(record===expected)return true;
  const headers=s=>s.split('\n').filter(l=>/^[#!]/.test(l));assert.deepEqual(headers(record),headers(expected));
  const frequency=s=>Number(s.trim().split('\n').find(l=>!/^[/#!]/.test(l)).trim().split(/\s+/)[0]);assert.equal(frequency(record),frequency(expected));
  const a=numeric(record),b=numeric(expected),scale=Math.max(...b.map(Math.abs));assert.equal(a.length,b.length);
  a.forEach((v,i)=>{assert.ok(Number.isFinite(v)&&Number.isFinite(b[i]));if(v===0||b[i]===0)assert.ok(Object.is(v,b[i]),'Signed zero changed');});
  const maximum=Math.max(...a.map((v,i)=>Math.abs(v-b[i])));assert.ok(maximum<=scale*1e-10,'Original report accuracy exceeded');
  differences.push({family,parameter,max_absolute_difference:maximum,relative_to_norm:maximum/scale,actual:record,original:expected});return false;
}
const bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');};
for(const item of fixture.cases){
  configureKernel(kernel,technology);const v=item.case,family=item.family;
  const creators=family==='three-port'?v.creators:family==='transformer'?[v.primary,v.secondary]:family==='pad'?[v.device,...v.pads]:[{id:v.id,keywords:v.keywords}];
  let layout;for(const creator of creators.filter(Boolean))layout=createGeometry(kernel,creator.id,creator.keywords,'',{append:Boolean(layout)});
  if(v.ground&&!v.ground.startsWith('@'))layout=createGeometry(kernel,2,v.ground,'',{append:true});
  const index=name=>layout.shapes.findIndex(s=>s.name===name),a=index('A'),b=index('B'),p1=index('P1'),p2=index('P2');
  const ground=v.ground?v.ground.startsWith('@')?v.ground==='@1'?(family==='transformer'?a:p1):v.ground==='@2'?(family==='transformer'?b:p2):a:index('G'):family==='network'?a:-1;
  const prefix=family==='three-port'?[...v.names.map(index),ground]:family==='transformer'?[a,b,ground]:family==='pad'?[a,p1,p2,ground]:[a,ground];
  const cells=[...item.cells,...item.contact_cells].map(v=>kernel._normalize_value(v,7,.1,1)),suffix=[...cells,512,512,...v.grid,512,1024,0,v.layers.length,v.layers[0]??0,v.layers[1]??0,...(family==='transformer'?[kernel._normalize_value(v.termination??0,4,.1,0)]:[]),250000];
  const solver=family==='three-port'?kernel._simulate_three_port:family==='transformer'?kernel._simulate_transformer:family==='pad'?kernel._simulate_pad_network:family==='extended'?kernel._simulate_extended_network:kernel._simulate_network;
  const head=family==='three-port'?['--three',...v.creators.flatMap(c=>[c.id,c.keywords]),v.ground??'']:family==='transformer'?['--calctrans',v.primary.id,v.primary.keywords,v.secondary?.id??-1,v.secondary?.keywords??'',v.ground??'']:family==='pad'?['--pad',...[v.device,...v.pads].flatMap(c=>[c.id,c.keywords]),v.ground??'']:[family==='extended'?'--pix':'--network',v.id,v.keywords,2,v.ground??(family==='network'?'@self':'')];
  const nativeArgs=[...head,1e9,...cells,...v.grid,...(family==='network'||family==='extended'?[512,1024]:[]),0,v.layers.length,v.layers[0]??0,v.layers[1]??0,...(family==='transformer'?[v.termination??0]:[]),250000,...(family==='transformer'?[a,b]:[])];
  const native=JSON.parse(execFileSync('build/native-geometry',nativeArgs.map(String),{encoding:'utf8',env:{...process.env,REASITIC_MAXNW:String(item.max_width_cells)},maxBuffer:16*1024*1024}));assert.ok(!native.error,native.error);
  for(const [key,raw]of Object.entries(item.raw_stages??{})){
    const matrix=native[key],values=matrix[0].flatMap((_,j)=>matrix.flatMap(row=>row[j])),bytes=Buffer.from(raw,'hex');assert.equal(bytes.length,values.length*8);
    if(key==='capacitance_matrix')assert.equal(values.map(bits).join(''),raw,family+' original native raw '+key);
    else {const expected=values.map((_,i)=>bytes.readDoubleLE(i*8)),scale=Math.max(...expected.map(Math.abs));values.forEach((v,i)=>assert.ok(Math.abs(v-expected[i])<=scale*1e-10,'Native inductance differs from original mathematical accuracy'));}
  }
  assert.equal(kernel._set_max_width_cells(item.max_width_cells),1);
  const solve=()=>JSON.parse(kernel.UTF8ToString(solver(...prefix,1e9,...suffix)));
  kernel._set_reference_numerics(1);const historical=solve();kernel._set_reference_numerics(0);assert.ok(!historical.error,family+': '+historical.error);
  for(const [parameter,expected]of Object.entries(item.records)){
    if(compareRecord(portSweepRecord([native],parameter),expected,family,parameter,nativeDifferences))nativeExact++;files++;
    if(compareRecord(portSweepRecord([historical],parameter),expected,family,parameter,historicalDifferences))historicalExact++;
  }
  const actual=solve();assert.ok(!actual.error,family+': '+actual.error);assert.equal(actual.max_width_cells,item.max_width_cells);
  assert.equal(actual.filaments,historical.filaments);
  for(const [parameter,expected]of Object.entries(item.records))compareRecord(portSweepRecord([actual],parameter),expected,family,parameter,[]);
  const aValues=(family==='three-port'?actual:actual.two_port).scattering_matrix.flat(2),hValues=(family==='three-port'?historical:historical.two_port).scattering_matrix.flat(2),scale=Math.max(...hValues.map(Math.abs));
  aValues.forEach((value,i)=>assert.ok(Math.abs(value-hValues[i])<=scale*1e-10,family+' stable production S'));
  let independent;
  if(family==='extended'){
    const reference=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_extended.py'],{input:JSON.stringify(actual),encoding:'utf8'}));assert.equal(reference.mpmath_version,'1.3.0');independent={};
    for(const key of ['matrix','currents','voltages']){const values=actual[key].flat(2),expected=reference[key].flat(2),scale=Math.max(...expected.map(Math.abs));assert.equal(values.length,expected.length);const error=Math.max(...values.map((v,i)=>Math.abs(v-expected[i])));assert.ok(error<=scale*1e-12,'Independent PIX nodal reduction '+key);independent[key]={max_absolute_error:error};}
  }
  const defaultPolicy=()=>{assert.equal(kernel._set_max_width_cells(500),1);return solve();};
  const ordinary=defaultPolicy();assert.ok(!ordinary.error,ordinary.error);assert.equal(ordinary.max_width_cells,undefined);
  assert.notDeepEqual(actual.inductance_admittance_matrix??actual.inductance_impedance_matrix,ordinary.inductance_admittance_matrix??ordinary.inductance_impedance_matrix,family+' MAXNW did not reach the inductance solve');
  assert.equal(kernel._set_max_width_cells(item.max_width_cells),1);assert.deepEqual(solve(),actual,'Changing MAXNW contaminated source mesh history');
  reports.push({family,independent,max_width_cells:item.max_width_cells,filaments:actual.filaments,default_filaments:ordinary.filaments});
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/maxnw-ports.json',JSON.stringify({models:reports.length,original_complete_files:files,native_exact_files:nativeExact,native_rounding_observations:nativeDifferences,wasm_historical_exact_files:historicalExact,historical_differences:historicalDifferences,production_agrees_to_norm:1e-10,reports},null,2)+'\n');
console.log(`MAXNW original ports: ${files} original full-precision S/Y/Z files across ${reports.length} models; ${nativeExact} native files exact, ${historicalExact} WASM historical files exact, ${historicalDifferences.length} retained-rounding observations; stable production, independent PIX reduction and policy recovery pass`);
if(process.argv.includes('--strict-historical'))assert.equal(historicalDifferences.length,0,'Retained x87 instruction identity remains unproven');

if(process.argv.includes('--strict-native'))assert.equal(nativeDifferences.length,0,'Host instruction identity remains unproven');
