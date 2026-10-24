import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
import assert from 'node:assert/strict';
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap)));
function next(){return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{worker.terminate();reject(Error('Worker response timeout'));},10000);worker.once('message',data=>{clearTimeout(timer);resolve(data);});worker.once('error',reject);});}
try{
  assert.equal((await next()).ready,true);
  const technology=readFileSync(new URL('../web/technology/CMOS.tek',import.meta.url),'utf8');
  let response=next();worker.postMessage({id:1,kind:0,parameters:[100,90,3,2,2,50,8,3,2,0,15,1.5],technology});
  const data=await response;assert.equal(data.id,1);assert.equal(data.layout.shapes.length,1);assert.ok(data.layout.shapes[0].segments.length>8);
  response=next();worker.postMessage({id:3,action:'analyze',frequency:2e9});const estimate=await response;assert.ok(estimate.analysis.windings[0].inductance_nh>0);
  response=next();worker.postMessage({id:2,kind:0,parameters:[100,90,0,2,2,50,8,3,2,0,15,1.5],technology});assert.match((await response).error,/Invalid/);
  console.log('Module worker: WASM startup, technology configuration, geometry response and validation errors pass');
}finally{await worker.terminate();}
