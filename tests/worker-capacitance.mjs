import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap)));
function next(){return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('CAP worker timeout')),10000);worker.once('message',data=>{clearTimeout(timer);resolve(data);});worker.once('error',reject);});}
async function send(data){const pending=next();worker.postMessage(data);return pending;}
try {
  assert.equal((await next()).ready,true);
  let result=await send({id:1,action:'capacitance',frequency_ghz:1});assert.match(result.error,/Generate/);
  const technology=readFileSync('web/technology/CMOS.tek','utf8');result=await send({id:2,technology,creator:{id:2,keywords:'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100'}});assert.equal(result.layout.shapes.length,1);
  const request={action:'capacitance',frequency_ghz:1,mesh:{length:1000,width:1000},fft_x:8,fft_y:8,contact_limit:128,nativeInput:true};
  result=await send({id:3,...request});assert.ok(!result.error,result.error);assert.equal(result.capacitance.green_table_hash,'d9ec835ccc4a4f7e');assert.equal(result.capacitance.capacitance_f,3.168713072636854e-13);assert.equal(result.capacitance.contacts,1);
  result=await send({id:4,...request,ground:0});assert.equal(result.capacitance.contacts,1,'Native explicit same-device ground');
  result=await send({id:5,...request,unfolded:true});assert.equal(result.capacitance.unfolded,true);assert.equal(result.capacitance.capacitance_f,null);
  result=await send({id:6,...request,contact_limit:0});assert.match(result.error,/contacts/);
  result=await send({id:7,...request});assert.equal(result.capacitance.green_table_hash,'d9ec835ccc4a4f7e');
  result=await send({id:8,action:'simulate',frequency_ghz:1,mesh:{length:1000,width:1000,thickness:1000,subdivision_trigger:1000}});assert.ok(result.simulation && !result.error,result.error);assert.equal(result.simulation.conductors,1);
  console.log('CAP worker: full original solve, exact raw tables/totals, explicit ground acquisition, unfolded output, rejection recovery and following LMAT solve pass');
} finally {await worker.terminate();}
