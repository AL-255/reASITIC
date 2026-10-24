import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseCreatorCommand} from '../web/creators.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {writeGDS} from '../web/gds.mjs';

const ranges=process.argv.includes('--ranges'),definitionName=ranges?'creator-range':'creator-coverage',proofName=ranges?'creator-ranges':'creator-coverage';
const definitions=readFileSync(`tests/fixtures/${definitionName}-cases.json`,'utf8'),cases=JSON.parse(definitions).cases,fixture=JSON.parse(readFileSync(`tests/fixtures/original-${proofName}.json`,'utf8'));
const hash=x=>createHash('sha256').update(x).digest('hex');
assert.equal(hash(definitions),fixture.case_definition_sha256);
const technologies=Object.fromEntries(Object.keys(fixture.technology_sha256).map(name=>{
  const text=readFileSync(`web/technology/${name}.tek`,'utf8');assert.equal(hash(text),fixture.technology_sha256[name]);return [name,{text,parsed:parseTechnology(text)}];
}));
function physical(layout,count){
  const segments=values=>values.map(s=>({metal:s.metal,width:s.width,points:s.points,...(s.metal<count?{thickness:s.thickness}:{nx:s.nx,ny:s.ny}),...(s.branches?{branches:segments(s.branches)}:{})}));
  return layout.shapes.map(s=>({name:s.name,origin:s.origin,segments:segments(s.segments),...(s.hidden?{hidden:true}:{})}));
}
const countFields=value=>typeof value==='number'?1:value&&typeof value==='object'?Object.values(value).reduce((n,v)=>n+countFields(v),0):0;
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Creator coverage worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);assert.ok(!result.error,result.error);return result;}
const kernel=await make(),mesh={length:1000,width:1000,thickness:1000,subdivision_trigger:1000},reports=[];
let fields=0,matrices=0,worstMatrixError=0;
mkdirSync(`build/gds-${proofName}`,{recursive:true});
try{
  assert.equal((await next()).ready,true);
  for(const c of cases){
    const original=fixture.cases.find(v=>v.name===c.name),technology=technologies[c.technology],creator=parseCreatorCommand(c.command,c.answers);
    configureKernel(kernel,technology.parsed);
    const actual=createGeometry(kernel,creator.id,creator.keywords,creator.answers);
    assert.deepEqual(physical(actual,original.metal_count),original.shapes,c.name+' original physical geometry');fields+=countFields(original.shapes);
    if(c.technology==='CMOS'){
      const native=JSON.parse(execFileSync('build/native-geometry',['--creator',creator.id,creator.keywords,creator.answers,'.1','1'].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));
      assert.deepEqual(physical(native,original.metal_count),original.shapes,c.name+' native C++ physical geometry');
    }
    for(const shape of actual.shapes)assert.ok(Math.abs(shape.resistance_dc-Number(original.resistance_dc_printed[shape.name]))<=5.1e-7,c.name+' original printed DC resistance');
    const generated=await send({technology:technology.text,creator});
    assert.deepEqual(physical(generated.layout,original.metal_count),original.shapes,c.name+' worker original physical geometry');
    const gds=writeGDS(actual,technology.parsed);assert.deepEqual(writeGDS(generated.layout,generated.technology),gds,c.name+' worker GDS');writeFileSync(`build/gds-${proofName}/${c.name}.gds`,gds);
    for(const [name,expected]of Object.entries(original.matrices??{})){
      const winding=actual.shapes.findIndex(s=>s.name===name),solved=JSON.parse(kernel.UTF8ToString(kernel._simulate(winding,1e9,1000,1000,1000,1000,1024)));
      assert.ok(!solved.error,solved.error);assert.equal(solved.matrix.length,expected.length,c.name+' matrix size');assert.equal(solved.filaments,expected.length,c.name+' original filament count');
      const reference=expected.flat(2),values=solved.matrix.flat(2),scale=reference.reduce((m,v)=>Math.max(m,Math.abs(v)),0);assert.equal(values.length,reference.length);
      const relativeError=values.reduce((error,v,i)=>{assert.ok(Number.isFinite(v));return Math.max(error,Math.abs(v-reference[i])/scale);},0);
      // LMAT prints ten significant digits; compare the whole matrix at that precision.
      assert.ok(relativeError<=1e-10,`${c.name}/${name} original LMAT relative error ${relativeError}`);worstMatrixError=Math.max(worstMatrixError,relativeError);
      const result=await send({action:'simulate',winding,frequency_ghz:1,mesh,filament_limit:1024});assert.deepEqual(result.simulation.matrix,solved.matrix,c.name+' worker matrix');matrices++;
    }
    reports.push({name:c.name,technology:c.technology,shapes:actual.shapes.length,original_creator_exit_status:original.creator_exit_status});
  }
  mkdirSync('build/evidence',{recursive:true});writeFileSync(`build/evidence/${proofName}.json`,JSON.stringify({cases:cases.length,physicalFields:fields,matrices,worstMatrixError,directAndWorker:true,reports},null,2)+'\n');
  console.log(`Creator coverage: ${cases.length} original cases on both process stacks, ${fields} exact physical fields, ${matrices} original LMAT matrices, DC resistance, worker and GDS equivalence pass`);
}finally{await worker.terminate();}
