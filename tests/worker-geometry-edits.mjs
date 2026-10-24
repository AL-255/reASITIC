import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
import make from '../dist/asitic.js';
import {configureKernel,parseTechnology,createGeometry,editGeometry} from '../web/technology.mjs';
const technology=readFileSync('web/technology/CMOS.tek','utf8'),cases=JSON.parse(readFileSync('tests/fixtures/geometry-edit-cases.json','utf8')).cases,kernel=await make();
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Geometry worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const r=await next();assert.equal(r.id,id);return r;}
async function checked(data){const r=await send(data);assert.ok(!r.error,r.error);return r;}
const request={action:'simulate',frequency:1e9,winding:0,mesh:{length:1000,width:1000,thickness:1000,subdivision_trigger:1000},filament_limit:1024};
try{
  assert.equal((await next()).ready,true);assert.match((await send({action:'edit',commands:'MOVE A 1 2'})).error,/layout/);
  for(const item of cases){
    await checked({technology,creator:item.creator});configureKernel(kernel,parseTechnology(technology));createGeometry(kernel,item.creator.id,item.creator.keywords);
    const edited=await checked({action:'edit',commands:item.commands});assert.equal(edited.edited,true);assert.deepEqual(edited.layout,editGeometry(kernel,item.commands));
    const result=await checked(request),direct=JSON.parse(kernel.UTF8ToString(kernel._simulate(0,1e9,1000,1000,1000,1000,1024)));assert.deepEqual(result.simulation,direct,item.name+' edited layout did not reach the math kernel');
  }
  const expected=(await checked({action:'edit',commands:'SELECT A'})).layout,simulation=(await checked(request)).simulation;
  for(const commands of ['MOVE A 1 2\nCOPY A A','MOVE A 1 2\nSELECT MISSING','CAP A 1','MOVE A NaN 0','SELECT A\0ERASE A']){
    assert.ok((await send({action:'edit',commands})).error);assert.deepEqual((await checked({action:'edit',commands:'MOVEX A 0'})).layout,expected);assert.deepEqual((await checked(request)).simulation,simulation);
  }
  const empty=(await checked({action:'edit',commands:'ERASE A'})).layout;assert.deepEqual(empty.shapes,[]);assert.match((await send(request)).error,/Generate|valid winding/);
  const appended=await checked({technology,creator:{id:2,keywords:'NAME=NEW:LEN=40:WID=4:METAL=M5:XORG=100:YORG=100'},options:{append:true}});assert.equal(appended.layout.shapes[0].name,'NEW');assert.ok((await checked(request)).simulation.inductance_h>0);
  console.log('Geometry worker: all original edit batches, committed layout simulation, atomic rejection, selection, deletion to empty and append recovery pass');
}finally{await worker.terminate();}
