import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,cpSync,mkdtempSync,rmSync,mkdirSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {pathToFileURL} from 'node:url';
import {Worker} from 'node:worker_threads';
import {writeGDS} from '../web/gds.mjs';
const work=mkdtempSync(join(tmpdir(),'reasitic-opt-start-')),manifest=JSON.parse(readFileSync('dist/build.json','utf8'));cpSync(join('dist','releases',manifest.release),work,{recursive:true});
let source=readFileSync(join(work,'kernel.mjs'),'utf8'),needle='export default createKernelFactory();';assert.ok(source.includes(needle));source=source.replace(needle,`const factory=createKernelFactory();let starts=0;
export default async function(){const instance=await factory();if(++starts===2){globalThis.postMessage({testPrivateKernelWaiting:true});await new Promise(resolve=>globalThis.releasePrivateKernel=resolve);}return instance;}`);writeFileSync(join(work,'kernel.mjs'),source);
source=readFileSync(join(work,'worker.mjs'),'utf8');needle='if(!done)await new Promise(resolve=>setTimeout(resolve,0));';const begin=source.indexOf("if(action==='optimize')"),end=source.indexOf("if(action==='optimization-apply')",begin),section=source.slice(begin,end);assert.equal(section.split(needle).length-1,1);source=source.slice(0,begin)+section.replace(needle,'if(!done)await new Promise(resolve=>{globalThis.continueOptimization=resolve;postMessage({testBetweenCandidates:true});});')+source.slice(end);writeFileSync(join(work,'worker.mjs'),source);
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(pathToFileURL(join(work,'worker.mjs')).href)});parentPort.on('message',data=>{if(data.testReleasePrivate){globalThis.releasePrivateKernel();setTimeout(()=>parentPort.postMessage({testStaleKernelSettled:true}),0);}else if(data.testContinue){globalThis.continueOptimization();}else self.onmessage({data});});`,worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Optimizer startup interleaving timeout')),60000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;function start(data){worker.postMessage({id:++id,...data});return id;}async function send(data){const request=start(data),response=await next();assert.equal(response.id,request);assert.ok(!response.error,response.error);return response;}
try{
  assert.equal((await next()).ready,true);const technology=readFileSync('web/technology/CMOS.tek','utf8'),initial=(await send({technology,creator:{id:2,keywords:'NAME=KEEP:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100'}})).layout;
  const c=JSON.parse(readFileSync('tests/fixtures/optimization-cases.json','utf8')).cases[0],original=JSON.parse(readFileSync('tests/fixtures/original-optimization.json','utf8')).cases.find(v=>v.name===c.name),request={action:'optimize',technology,keywords:c.keywords,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:128,fft_y:128,contact_limit:128,filament_limit:256,eddy:{mode:0,layers:[]},options:{candidate_limit:3,search:'SWEEP'}};
  const obsolete=start(request);assert.equal((await next()).testPrivateKernelWaiting,true);const active=start(request);let count=0,released=false;
  while(true){
    const response=await next();assert.notEqual(response.id,obsolete,'Obsolete initialization emitted a result');assert.equal(response.id,active);assert.ok(!response.error,response.error);const r=response.optimization,s=r.sample,e=original.candidates[count];assert.ok(s&&!s.error,s?.error);assert.deepEqual(s.descriptor,e.descriptor);for(const [key,index]of [['inductance_h',1],['resistance',2],['quality_first',7]])assert.ok(Math.abs(s.pi[key]-e.pi[index])<=Math.abs(e.pi[index])*1e-8,'Active original candidate '+count+' '+key);count++;
    if(r.done)break;assert.equal((await next()).testBetweenCandidates,true);
    if(!released){worker.postMessage({testReleasePrivate:true});assert.equal((await next()).testStaleKernelSettled,true);released=true;}
    worker.postMessage({testContinue:true});
  }
  assert.equal(count,3);const clean=({warnings,...rest})=>rest;assert.deepEqual(clean((await send({action:'edit',commands:'UNSELECT'})).layout),clean(initial));const applied=await send({action:'optimization-apply',index:2});assert.deepEqual(writeGDS(applied.layout,applied.technology),writeGDS({shapes:[{name:'OPT',...original.candidates[2].geometry}]},applied.technology));
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/optimizer-startup.json',JSON.stringify({controlledLateInitialization:true,obsoleteRequestSilent:true,activeOriginalCandidates:count,privateStatePreserved:true,liveLayoutPreserved:true,originalGdsApplication:true},null,2)+'\n');console.log('Optimizer startup: controlled stale initialization cannot replace an active search; all three original candidates, live layout and applied original GDS remain correct');
}finally{await worker.terminate();rmSync(work,{recursive:true,force:true});}
