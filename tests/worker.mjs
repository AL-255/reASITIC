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
  const bicmos=readFileSync(new URL('../web/technology/BiCMOS.tek',import.meta.url),'utf8');
  response=next();worker.postMessage({id:4,kind:0,parameters:[100,90,3,2,2,50,8,2,1,0,15,1.5],technology:bicmos});assert.equal((await response).layout.shapes.length,1);
  response=next();worker.postMessage({id:5,kind:0,parameters:[100,90,3,2,2,50,8,2,2,0,15,1.5],technology:bicmos});assert.match((await response).error,/must differ/);
  response=next();worker.postMessage({id:6,creator:{id:4,keywords:'NAME=PLATE:LEN=40:WID=5:METAL1=M2:METAL2=M1'},technology});const plate=await response;assert.equal(plate.layout.shapes[0].segments.length,2);
  response=next();worker.postMessage({id:7,creator:{id:10,answers:'P\nS\n80 80\n3\n8\n2\nM4\nM3\n100 100\n0\n'},technology});assert.match((await response).error,/another line/);
  response=next();worker.postMessage({id:8,creator:{id:14,keywords:'NAME=V:NX=5:NY=7:VIA=VIA2'},technology});const via=await response;assert.equal(via.layout.shapes[0].segments[0].nx,5);assert.equal(via.layout.shapes[0].segments[0].ny,7);
  console.log('Module worker: WASM startup, technology configuration, geometry response and validation errors pass');
}finally{await worker.terminate();}
