import {twoPortRecord,twoPortScatteringCSV} from '../web/network.mjs';
import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
const technology=readFileSync('web/technology/CMOS.tek','utf8');
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap)));
function next(){return new Promise((resolve,reject)=>{const cleanup=()=>{clearTimeout(timer);worker.off('message',message);worker.off('error',error);};const message=data=>{cleanup();resolve(data);},error=e=>{cleanup();reject(e);},timer=setTimeout(()=>error(Error('Network worker timeout')),15000);worker.once('message',message);worker.once('error',error);});}
async function send(data){const pending=next();worker.postMessage(data);return pending;}
try{
 assert.equal((await next()).ready,true);const request={action:'network',frequency_ghz:1,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:8,fft_y:8,eddy:{mode:0,layers:[0,1]},nativeInput:true};
 let result=await send({id:1,...request});assert.match(result.error,/layout/);
 result=await send({id:2,technology,creator:{id:2,keywords:'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100'}});assert.equal(result.layout.shapes.length,1);
 result=await send({id:3,...request});assert.ok(!result.error,result.error);const expected=result.network;assert.equal(expected.two_port.reference_ohm,50);assert.equal(twoPortScatteringCSV(expected).trim().split('\n').length,5);assert.ok(twoPortRecord(expected,'s').startsWith('\n# GHZ S RI '));assert.ok(twoPortRecord(expected,'y',true).startsWith('\n# GHZ Y MA '));assert.equal(expected.pi.resistance,0.16249999999999998);assert.equal(expected.pi.shunt_differential,null);assert.equal(expected.mesh.fft_x,8);
 result=await send({id:4,...request,eddy:{mode:1,layers:[0,1],evaluation_limit:1}});assert.match(result.error,/budget/);result=await send({id:5,...request});assert.deepEqual(result.network,expected);
 result=await send({id:6,...request,eddy:{mode:1}});assert.equal(result.network.eddy.layer_count,1);assert.ok(result.network.eddy.quadrature_status[5]>0);assert.match(result.warnings,/did not converge/);
 result=await send({id:7,...request,ground:0});assert.equal(result.network.filaments,9);result=await send({id:8,...request});assert.deepEqual(result.network,expected);
 result=await send({id:9,...request,eddy:{mode:0,layers:[99]}});assert.match(result.error,/layers/);result=await send({id:10,...request});assert.deepEqual(result.network,expected);
 result=await send({id:11,action:'simulate',frequency:1e9,mesh:{length:1000,width:1000,thickness:1000,subdivision_trigger:1000}});assert.ok(!result.error,result.error);assert.equal(result.simulation.resistance,.1625);
 console.log('Network worker: complete original solve, native inputs/layers, visible nonconvergence, self-ground isolation, rejected-budget recovery and following LMAT solve pass');
}finally{await worker.terminate();}
