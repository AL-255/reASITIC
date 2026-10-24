import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import make from '../dist/asitic.js';
import {configureKernel,parseTechnology,createGeometry,editGeometry} from '../web/technology.mjs';
import {parseCreatorCommand} from '../web/creators.mjs';
import {polygons,writeGDS} from '../web/gds.mjs';
const text=readFileSync('web/technology/CMOS.tek','utf8'),technology=parseTechnology(text),definitions=readFileSync('tests/fixtures/geometry-edit-cases.json','utf8'),cases=JSON.parse(definitions).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-geometry-edits.json','utf8')),kernel=await make();
const hash=s=>createHash('sha256').update(s).digest('hex');assert.equal(hash(text),fixture.technology_sha256);assert.equal(hash(definitions),fixture.case_definition_sha256);
let fields=0,matrices=0;const reports=[];
function physical(actual,expected,path){
  for(const [key,value]of Object.entries(expected)){
    if(Array.isArray(value)){assert.equal(actual[key].length,value.length,path+'.'+key);value.forEach((v,i)=>{if(v&&typeof v==='object'){if(Array.isArray(v)){assert.deepEqual(actual[key][i],v,path+'.'+key+'.'+i);fields+=v.length;}else physical(actual[key][i],v,path+'.'+key+'.'+i);}else {assert.equal(actual[key][i],v);fields++;}});}
    else {assert.equal(actual[key],value,path+'.'+key);fields++;}
  }
}
const clean=({warnings,...rest})=>rest;
for(const item of cases){
  configureKernel(kernel,technology);createGeometry(kernel,item.creator.id,item.creator.keywords);
  const result=editGeometry(kernel,item.commands),original=fixture.cases.find(c=>c.name===item.name);assert.equal(result.shapes.length,original.shapes.length);
  result.shapes.forEach((shape,i)=>physical(shape,original.shapes[i],item.name+'.'+shape.name));
  for(const [name,expected]of Object.entries(original.matrices)){
    const index=result.shapes.findIndex(s=>s.name===name),solved=JSON.parse(kernel.UTF8ToString(kernel._simulate(index,1e9,1000,1000,1000,1000,1024)));assert.ok(!solved.error,solved.error);assert.equal(solved.filaments,expected.filaments,item.name+' original edited filament count');
    const scale=Math.max(...expected.matrix.flat(2).map(Math.abs));assert.equal(solved.matrix.length,expected.matrix.length);solved.matrix.flat(2).forEach((v,j)=>assert.ok(Math.abs(v-expected.matrix.flat(2)[j])<=scale*1e-10,item.name+' original edited LMAT'));matrices++;
  }
  const native=JSON.parse(execFileSync('build/native-geometry',['--edit',item.creator.id,item.creator.keywords,item.commands,.1,1].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));assert.ok(!native.error,native.error);assert.deepEqual(clean(result),native,item.name+' native/WASM edit state');
  configureKernel(kernel,technology);createGeometry(kernel,item.creator.id,item.creator.keywords);let stepped;
  for(const command of item.commands.split('\n'))stepped=editGeometry(kernel,command);
  assert.deepEqual(clean(stepped),clean(result),item.name+' cloning between commands changed graph links');
  const gds=writeGDS(result,technology);assert.ok(gds.length>0);mkdirSync('build/gds-edits',{recursive:true});writeFileSync('build/gds-edits/'+item.name+'.gds',gds);
  const expectedGDS=writeGDS({shapes:original.shapes},technology);assert.deepEqual(gds,expectedGDS,item.name+' original GDS geometry');
  reports.push({name:item.name,shapes:result.shapes.length,polygons:[...polygons(result,technology)].length});
}
let creatorCopies=0;
for(const fixture of JSON.parse(readFileSync('tests/fixtures/original-creator-binary.json','utf8')).cases){
  configureKernel(kernel,technology);const c=parseCreatorCommand(fixture.command,fixture.answers),before=createGeometry(kernel,c.id,c.keywords,c.answers);assert.deepEqual(clean(editGeometry(kernel,'UNSELECT')),clean(before),fixture.family+' raw clone changed creator geometry');creatorCopies++;
}
const paired=cases.find(c=>c.name==='paired-move').creator;
configureKernel(kernel,technology);createGeometry(kernel,paired.id,paired.keywords);const joinedPair=editGeometry(kernel,'SELECT S\nJOIN P S\nMOVE P 10 20');assert.deepEqual(joinedPair.shapes.map(s=>s.name),['P']);assert.equal(joinedPair.selected,'P');assert.deepEqual(clean(editGeometry(kernel,'MOVEX P 0')),clean(joinedPair),'Removed partner remained in graph links');
const square=cases.find(c=>c.name==='wire-conversion').creator;
configureKernel(kernel,technology);createGeometry(kernel,square.id,square.keywords);const wires=editGeometry(kernel,'SELECT A\nSPTOWIRE A');assert.equal(wires.selected,undefined,'Wire conversion retained a deleted selection');assert.deepEqual(clean(editGeometry(kernel,'MOVEX A_0 0')),clean(wires));
configureKernel(kernel,technology);createGeometry(kernel,2,'NAME=A:LEN=80:WID=12:METAL=M1:XORG=100:YORG=100');const before=editGeometry(kernel,'CP A B\nBEFRIEND A B\nSELECT A');
const analyze=()=>JSON.parse(kernel.UTF8ToString(kernel._analyze(1e9))),analysis=analyze();
for(const script of ['MOVE A 2 3\nJOIN A A','MOVE A 2 3\nERASE A A','MOVE A 2 3\nCOPY A B','RENAME A B','SELECT MISSING','MOVE . banana 1','SPLIT A 0 C','JOINSHUNT A A','UNFRIEND A A','CP A C\nBEFRIEND A C','ROTATE A Infinity','MOVE A 1e99 0','MOVE A 1 2\nSAVE file','MOVE A 1 2\nCOPY A '+('X'.repeat(80)),'COPY A C\nJOIN A B C A','MOVE A 1 2\n'+('MOVEX A 0\n'.repeat(128))]){
  assert.throws(()=>editGeometry(kernel,script),undefined,script);assert.deepEqual(clean(editGeometry(kernel,'MOVEX A 0')),clean(before),'Rejected batch damaged the layout');assert.deepEqual(analyze(),analysis,'Rejected batch changed estimates');
}
for(let i=0;i<100;++i)assert.deepEqual(clean(editGeometry(kernel,'MOVEX A 0')),clean(before));const bytes=kernel.HEAPF64.buffer.byteLength;for(let i=0;i<100;++i)editGeometry(kernel,'MOVEX A 0');assert.equal(kernel.HEAPF64.buffer.byteLength,bytes,'Repeated transactions grew memory after warmup');
const hidden=editGeometry(kernel,'HIDE A B');assert.equal([...polygons(hidden,technology,{visibleOnly:true})].length,0);assert.equal([...polygons(hidden,technology)].length,2,'Display visibility changed physical GDS export');
const empty=editGeometry(kernel,'ERASE A B');assert.deepEqual(empty.shapes,[]);assert.throws(()=>editGeometry(kernel,'SELECT A'),/Generate a layout/);createGeometry(kernel,2,'NAME=A:LEN=20:WID=2:METAL=M5:XORG=100:YORG=100','',{append:true});
// A failure after native mutations and serialization must release every new
// allocation and retain the old layout, not just reject obvious bad inputs.
configureKernel(kernel,technology);const large=createGeometry(kernel,1,'NAME=A:RAD=200:SIDES=64:W=1:S=1:N=16:METAL=M5:XORG=100:YORG=100');const growth='CP A B\nJOIN A B\n'.repeat(4);
assert.throws(()=>editGeometry(kernel,growth),/export buffer/);assert.deepEqual(clean(editGeometry(kernel,'MOVEX A 0')),clean(large));const peak=kernel.HEAPF64.buffer.byteLength;
assert.throws(()=>editGeometry(kernel,growth),/export buffer/);assert.deepEqual(clean(editGeometry(kernel,'MOVEX A 0')),clean(large));assert.equal(kernel.HEAPF64.buffer.byteLength,peak,'Late rollback leaked allocations');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/geometry-edits.json',JSON.stringify({cases:reports.length,creator_graph_copies:creatorCopies,original_physical_fields:fields,original_lmat_matrices:matrices,native_wasm_exact:true,original_gds_exact:true,stepped_graph_links:true,rollback_and_lifetime:true,reports},null,2)+'\n');
console.log(`Geometry editing: ${reports.length} original batches, ${fields} exact physical fields, ${matrices} original LMAT matrices, native/WASM and GDS identity, branch/partner links, atomic rollback and lifetime checks pass`);
