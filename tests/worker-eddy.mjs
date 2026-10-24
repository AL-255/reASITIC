import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
import {parseTechnology} from '../web/technology.mjs';
const technology=readFileSync('web/technology/CMOS.tek','utf8');assert.deepEqual(parseTechnology(technology).eddy_layers,[0]);assert.deepEqual(parseTechnology(technology.replace('eddy = 0','eddy = 0\neddy = 1')).eddy_layers,[0,1]);assert.equal(parseTechnology(technology.replace('eddy = 0','eddy = 0\neddy = 1\neddy = 0')).warnings.length,1);
assert.throws(()=>parseTechnology(technology.replace('eddy = 0','eddy = 99')),/outside/);
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap)));function next(){return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Eddy worker timeout')),10000);worker.once('message',data=>{clearTimeout(timer);resolve(data);});worker.once('error',reject);});}async function send(data){const pending=next();worker.postMessage(data);return pending;}
try{
 assert.equal((await next()).ready,true);let result=await send({id:1,technology,creator:{id:2,keywords:'NAME=A:LEN=40:WID=5:METAL=M1:XORG=10:YORG=20'}});assert.equal(result.layout.shapes.length,1);
 const request={action:'simulate',frequency_ghz:1,mesh:{length:1000,width:1000,thickness:1000,subdivision_trigger:1000},eddy:{mode:1,layers:[0,1],evaluation_limit:250000},nativeInput:true};
 result=await send({id:2,...request});assert.equal(result.simulation.resistance,0.5317507628192227);assert.equal(result.simulation.eddy.value_hash,'a417bc964861d850');const expected=result.simulation;
 result=await send({id:3,...request,eddy:{mode:1}});assert.equal(result.simulation.eddy.layer_count,1);assert.equal(result.simulation.eddy.quadrature_status[5],1);assert.match(result.warnings,/did not converge/);
 result=await send({id:4,...request,eddy:{mode:1,layers:[0,1],evaluation_limit:1}});assert.match(result.error,/budget/);
 result=await send({id:5,...request});assert.deepEqual(result.simulation,expected);
 worker.postMessage({id:6,...request,action:'sweep',frequencies_ghz:[.1,1,2]});for(let i=0;i<3;++i){result=await next();assert.ok(!result.error,result.error);assert.equal(result.sweep.index,i);assert.ok(result.simulation.eddy.evaluations>0);assert.equal('matrix' in result.simulation,i===2);}
 result=await send({id:7,...request,eddy:{mode:0,layers:[]}});assert.equal(result.simulation.resistance,.52);assert.equal(result.simulation.eddy.evaluations,0);
 console.log('Eddy worker: repeated technology layers, full native solve, visible nonconvergence, budget recovery, streamed sweeps and disabled mode pass');
}finally{await worker.terminate();}
