import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
import assert from 'node:assert/strict';
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap)));
const inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Simulation worker timed out')),20000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
function send(data){worker.postMessage(data);}
try{
    assert.equal((await next()).ready,true);
    send({id:1,kind:2,parameters:[40,40,5,2,1,50,8,0,-1,0,15,1],options:{nativeInput:true,grid:.1,snap:true},technology:readFileSync('web/technology/CMOS.tek','utf8')});assert.equal((await next()).layout.shapes.length,1);
    const coarse={length:1000,width:1000,thickness:1000,subdivision_trigger:1000};
    send({id:2,action:'simulate',frequency_ghz:1,mesh:coarse,nativeInput:true});let result=await next();assert.ok(!result.error,result.error);assert.deepEqual(result.simulation.matrix,[[[.52,.1620359035362161]]]);assert.equal(result.simulation.filaments,1);
    send({id:3,action:'sweep',frequencies_ghz:[.1,1,10],mesh:coarse,nativeInput:true});
    for(let i=0;i<3;i++){result=await next();assert.ok(!result.error,result.error);assert.equal(result.id,3);assert.equal(result.sweep.index,i);assert.equal(result.sweep.done,i===2);assert.equal('matrix' in result.simulation,i===2);assert.equal(result.simulation.frequency_hz,[1e8,1e9,1e10][i]);}
    send({id:4,action:'sweep',frequencies_ghz:Array.from({length:101},(_,i)=>.1+i/10),mesh:{length:50,width:.5,thickness:.1,subdivision_trigger:50},nativeInput:true});
    result=await next();assert.ok(result.sweep && !result.sweep.done);send({id:5,action:'cancel'});
    let completed=1;while(!(result=await next()).cancelled){assert.equal(result.id,4);completed++;}
    assert.equal(result.id,5);assert.ok(completed<101,'Cancellation did not stop the remaining samples');
    send({id:6,action:'simulate',frequency:1e9,mesh:coarse,filament_limit:0});result=await next();assert.equal(result.id,6);assert.match(result.error,/filament limit/);
    send({id:7,action:'simulate',frequency:1e9,mesh:coarse});result=await next();assert.equal(result.id,7);assert.deepEqual(result.simulation.matrix,[[[.52,.1620359035362161]]]);
    console.log('Simulation worker: exact original wire matrix, native frequency inputs, streamed summaries/final matrix, cooperative cancellation and rejected-mesh recovery pass');
}finally{await worker.terminate();}
