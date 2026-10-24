import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {writeGDS} from '../web/gds.mjs';
const source=readFileSync('tests/fixtures/metal-rules-cases.json','utf8'),cases=JSON.parse(source).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-metal-rules.json','utf8')),technologyText=readFileSync('web/technology/CMOS.tek','utf8'),sha=x=>createHash('sha256').update(x).digest('hex'),number=x=>Object.is(x,-0)?'-0':String(x);
assert.equal(sha(source),fixture.case_definition_sha256);assert.equal(sha(technologyText),fixture.technology_sha256);
function withRules(c){const entries=[...technologyText.matchAll(/^\s*<(?:metal|via)>\s*(\d+)/gmi)],i=entries.findIndex(m=>m[0].toLowerCase().includes('<metal>')&&Number(m[1])===c.metal),end=entries[i+1]?.index??technologyText.length;assert.ok(i>=0);return technologyText.slice(0,end)+`\nSMIN = ${number(c.smin)}\nWMIN = ${number(c.wmin)}\n`+technologyText.slice(end);}
function physical(layout,count){const segments=values=>values.map(s=>({metal:s.metal,width:s.width,points:s.points,...(s.metal<count?{thickness:s.thickness}:{nx:s.nx,ny:s.ny}),...(s.branches?{branches:segments(s.branches)}:{})}));return layout.shapes.map(s=>({name:s.name,origin:s.origin,segments:segments(s.segments),...(s.hidden?{hidden:true}:{})}));}
function rulesMatch(technology,expected){for(const [i,hex]of Object.entries(expected)){const m=technology.metals[Number(i)],bytes=Buffer.alloc(16);bytes.writeDoubleLE(m.minimum_spacing);bytes.writeDoubleLE(m.minimum_width,8);assert.equal(bytes.toString('hex'),hex,'Original normalized metal '+i);}}
const messages={clearance_retries:'Need more space for transitions.',width_warnings:'Transition metal width less than WMIN specified in technology file',turn_retries:'Cannot have more than'};
function diagnostics(text){return Object.fromEntries(Object.entries(messages).map(([key,message])=>[key,text.split(message).length-1]));}
const k=await make(),reports=[];mkdirSync('build/gds-metal-rules',{recursive:true});let physicalFields=0;
const countFields=value=>typeof value==='number'?1:value&&typeof value==='object'?Object.values(value).reduce((n,v)=>n+countFields(v),0):0;
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Metal-rule worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}
try{
  assert.equal((await next()).ready,true);
  for(const c of cases){
    const expected=fixture.cases.find(v=>v.name===c.name),text=withRules(c),technology=parseTechnology(text);configureKernel(k,technology);rulesMatch(technology,expected.metal_rule_hex);
    const direct=createGeometry(k,c.creator.id,c.creator.keywords,c.creator.answers);assert.deepEqual(physical(direct,technology.metals.length),expected.shapes,c.name+' original physical fields');assert.deepEqual(diagnostics(direct.warnings),expected.diagnostics,c.name+' original diagnostics');physicalFields+=countFields(expected.shapes);
    const native=JSON.parse(execFileSync('build/native-geometry',['--creator',c.creator.id,c.creator.keywords,c.creator.answers,'.1','1'].map(String),{encoding:'utf8',env:{...process.env,REASITIC_RULE_METAL:String(c.metal),REASITIC_SMIN:number(c.smin),REASITIC_WMIN:number(c.wmin)}}));assert.deepEqual(physical(native,technology.metals.length),expected.shapes,c.name+' native C++ physical fields');
    const result=await send({technology:text,creator:c.creator});assert.ok(!result.error,result.error);rulesMatch(result.technology,expected.metal_rule_hex);assert.deepEqual(physical(result.layout,technology.metals.length),expected.shapes,c.name+' worker original geometry');assert.deepEqual(diagnostics(result.layout.warnings),expected.diagnostics,c.name+' worker diagnostics');
    const gds=writeGDS(direct,technology);assert.deepEqual(writeGDS(result.layout,result.technology),gds);writeFileSync(`build/gds-metal-rules/${c.name}.gds`,gds);reports.push({name:c.name,diagnostics:expected.diagnostics,metal_rules:technology.metals[c.metal]});
  }
  const target=cases.find(c=>c.name==='clearance-retry'),original=fixture.cases.find(c=>c.name===target.name);configureKernel(k,parseTechnology(withRules(target)));
  for(const args of [[-1,4,12],[32,4,12],[5,4,12],[3,NaN,12],[3,4,Infinity]])assert.ok(JSON.parse(k.UTF8ToString(k._set_metal_rules(...args))).error);
  assert.deepEqual(physical(createGeometry(k,target.creator.id,'',target.creator.answers),5),original.shapes,'Invalid setter changed live rules');
  const legacy=new Proxy(k,{get:(object,key)=>key==='_set_metal_rules'?undefined:Reflect.get(object,key)});
  configureKernel(legacy,parseTechnology(technologyText));const ordinary=fixture.cases.find(c=>c.name==='default');assert.deepEqual(physical(createGeometry(k,target.creator.id,'',target.creator.answers),5),ordinary.shapes,'Reset retained a previous process minimum');
  assert.throws(()=>configureKernel(legacy,parseTechnology(withRules(target))),/cannot apply SMIN/);
  const reset=await send({technology:technologyText,creator:target.creator});assert.ok(!reset.error,reset.error);assert.deepEqual(physical(reset.layout,5),ordinary.shapes,'Worker retained nondefault process rules');
  for(const bad of [NaN,Infinity])assert.throws(()=>parseTechnology(withRules({...target,smin:bad})),/Invalid technology number/);
  configureKernel(k,parseTechnology(withRules(target)));assert.deepEqual(physical(createGeometry(k,target.creator.id,'',target.creator.answers),5),original.shapes,'Rejected configuration contaminated following creation');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/metal-rules.json',JSON.stringify({cases:reports.length,physicalFields,originalMinimaBitsExact:true,directNativeAndWorker:true,diagnosticsExact:true,resetAndRejectionRecovery:true,reports},null,2)+'\n');
  console.log(`Metal process rules: ${reports.length} original SMIN/WMIN cases, ${physicalFields} exact physical fields, native/WASM normalization bits, original clearance/width diagnostics, worker/GDS equivalence and reset/rejection recovery pass`);
}finally{await worker.terminate();}
