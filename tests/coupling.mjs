import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseCreatorCommand} from '../web/creators.mjs';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
import {writeGDS} from '../web/gds.mjs';
const source=readFileSync('tests/fixtures/coupling-cases.json','utf8'),cases=JSON.parse(source).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-coupling.json','utf8')),hash=x=>createHash('sha256').update(x).digest('hex');
assert.equal(hash(source),fixture.case_definition_sha256);
const technologies=Object.fromEntries(Object.keys(fixture.technology_sha256).map(name=>{const text=readFileSync(`web/technology/${name}.tek`,'utf8');assert.equal(hash(text),fixture.technology_sha256[name]);return [name,{text,parsed:parseTechnology(text)}];}));
const bits=v=>{const b=Buffer.alloc(8);b.writeDoubleLE(v);return b.toString('hex');},clean=({warnings,...rest})=>rest;
function physical(layout,count){const segments=values=>values.map(s=>({metal:s.metal,width:s.width,points:s.points,...(s.metal<count?{thickness:s.thickness}:{nx:s.nx,ny:s.ny}),...(s.branches?{branches:segments(s.branches)}:{})}));return layout.shapes.map(s=>({name:s.name,origin:s.origin,segments:segments(s.segments),...(s.hidden?{hidden:true}:{})}));}
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Coupling worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}
const k=await make(),reports=[],hostRounding=[];let exactHistoricalFields=0,worstRelativeError=0;
const solve=(first,second,limit=250000)=>JSON.parse(k.UTF8ToString(k._coupling(first,second,limit)));
try{
  assert.equal((await next()).ready,true);
  for(const c of cases){
    const original=fixture.cases.find(v=>v.name===c.name),technology=technologies[c.technology],creators=c.creators.map(v=>parseCreatorCommand(v.command,v.answers));configureKernel(k,technology.parsed);
    let layout;for(const [i,v]of creators.entries())layout=createGeometry(k,v.id,v.keywords,v.answers,{append:i>0});
    if(c.edits)layout=editGeometry(k,c.edits);
    assert.deepEqual(physical(layout,original.metal_count),original.shapes,c.name+' original pre-analysis geometry');
    const first=layout.shapes.findIndex(s=>s.name===c.first),second=layout.shapes.findIndex(s=>s.name===c.second),before=clean(layout),gds=writeGDS(layout,technology.parsed),estimates=JSON.parse(k.UTF8ToString(k._analyze(1e9))),actual=solve(first,second);assert.ok(!actual.error,actual.error);
    k._set_reference_numerics(1);const historical=solve(first,second);k._set_reference_numerics(0);
    for(const key of ['coefficient','mutual_nh']){
      assert.equal(bits(historical[key]),original[key==='coefficient'?'coefficient_hex':'mutual_hex'],c.name+' original raw '+key);exactHistoricalFields++;
      const error=Math.abs(actual[key]-original[key])/Math.max(1e-30,Math.abs(original[key]));worstRelativeError=Math.max(worstRelativeError,error);assert.ok(error<=1e-10,c.name+' original '+key+' relative error '+error);
      assert.equal(actual[key].toFixed(2),original.printed[key],c.name+' original printed '+key);
    }
    assert.deepEqual(solve(first,second),actual,c.name+' repeat changed the result');
    const unchanged=editGeometry(k,'UNSELECT');assert.deepEqual(clean(unchanged),before,c.name+' estimate changed live records');assert.deepEqual(writeGDS(unchanged,technology.parsed),gds);assert.deepEqual(JSON.parse(k.UTF8ToString(k._analyze(1e9))),estimates,c.name+' estimate changed conductor metrics');
    if(c.technology==='CMOS'&&creators.length===2&&creators.every(v=>v.keywords)){
      const native=JSON.parse(execFileSync('build/native-geometry',['--coupling',creators[0].id,creators[0].keywords,creators[1].id,creators[1].keywords,250000].map(String),{encoding:'utf8'}));
      assert.deepEqual(Object.keys(native),Object.keys(historical));
      for(const key of Object.keys(historical)){
        if(Object.is(native[key],historical[key]))continue;
        assert.ok(['coefficient','mutual_nh'].includes(key),'Host changed coupling metadata');
        assert.ok(Number.isFinite(native[key])&&Number.isFinite(historical[key])&&native[key]!==0&&historical[key]!==0,'Host changed finite classification or signed zero');
        const relative=Math.abs(native[key]-historical[key])/Math.max(Math.abs(native[key]),Math.abs(historical[key]));assert.ok(relative<=1e-12,c.name+' host arithmetic '+key);hostRounding.push({name:c.name,key,software:historical[key],host:native[key],relative});
      }
    }
    for(const [i,creator]of creators.entries()){const response=await send({technology:technology.text,creator,options:{append:i>0}});assert.ok(!response.error,response.error);}
    if(c.edits){const edited=await send({action:'edit',commands:c.edits});assert.ok(!edited.error,edited.error);}
    const response=await send({action:'coupling',first,second});assert.ok(!response.error,response.error);assert.deepEqual(response.coupling,actual,c.name+' worker coupling');
    const after=await send({action:'edit',commands:'UNSELECT'});assert.ok(!after.error,after.error);assert.deepEqual(clean(after.layout),before,c.name+' worker source mutation');
    reports.push({name:c.name,original_mutated:original.original_mutated,segment_pairs:actual.segment_pairs,evaluation_pairs:actual.evaluation_pairs});
  }
  configureKernel(k,technologies.CMOS.parsed);createGeometry(k,2,'NAME=A:LEN=40:WID=5:METAL=M1:XORG=100:YORG=100');const before=createGeometry(k,2,'NAME=B:LEN=40:WID=5:METAL=M1:XORG=100:YORG=130','',{append:true}),expected=solve(1,0,1);
  const simulate=()=>JSON.parse(k.UTF8ToString(k._simulate(1,1e9,1000,1000,1000,1000,8))),simulation=simulate();assert.ok(!simulation.error,simulation.error);
  for(const args of [[-1,0],[0,2],[0,0],[0,1,0],[0,1,1000001]])assert.ok(solve(...args).error);
  for(let i=0;i<20;++i)solve(1,0,1);const bytes=k.HEAPF64.buffer.byteLength;for(let i=0;i<100;++i)assert.deepEqual(solve(1,0,1),expected);assert.equal(k.HEAPF64.buffer.byteLength,bytes,'Coupling workspace leaked');assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before));
  assert.deepEqual(simulate(),simulation,'Coupling changed the following LMAT solve');
  const reversed=solve(0,1,1);assert.equal(reversed.mutual_nh,expected.mutual_nh);assert.equal(reversed.coefficient,expected.coefficient);
  // Neumann's double line integral for two separated, parallel conductors.
  const integral=Number(execFileSync(process.env.MPMATH_PYTHON??'python3',['-c',"import mpmath as m; m.mp.dps=60; print(m.quad(lambda u: 2*(40-u)/m.sqrt(u*u+900),[0,40])*m.mpf('0.0001'))"],{encoding:'utf8'}));assert.ok(Math.abs(expected.mutual_nh-integral)<=Math.abs(integral)*1e-12,'Independent Neumann mutual integral');
  editGeometry(k,'CP A C\nJOIN C A');const same=solve(0,1,2);assert.ok(same.error,'Budget must include each winding self-pair work');
  const bad=await send({action:'coupling',first:0,second:0});assert.match(bad.error,/distinct/);const fractional=await send({action:'coupling',first:.5,second:1});assert.match(fractional.error,/integer/);const recovered=await send({action:'coupling',first:0,second:1});assert.ok(!recovered.error,recovered.error);
  for(const data of [{first:4294967296,second:1},{first:0,second:1,pair_limit:4294967297}])assert.ok((await send({action:'coupling',...data})).error,'Worker integer wrapped at the C ABI');
  if(process.argv.includes('--strict-native'))assert.deepEqual(hostRounding,[],'Strict host instruction identity');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/coupling.json',JSON.stringify({cases:cases.length,exactHistoricalFields,worstRelativeError,sourceImmutable:true,worker:true,independentNeumannIntegral:integral,hostRounding,reports},null,2)+'\n');
  console.log(`COUPLING: ${cases.length} original cases, ${exactHistoricalFields} exact historical fields, production reports, native/worker paths, immutable geometry, budgets, reciprocity and independent Neumann integral pass`);
}finally{await worker.terminate();}
