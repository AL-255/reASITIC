import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
import {portSampleSummary,portSweepRecord,threePortSweepMatrixCSV,portSweepMetricsCSV} from '../web/port-sweep.mjs';
const technology=readFileSync('web/technology/CMOS.tek','utf8'),cases=JSON.parse(readFileSync('tests/fixtures/three-port-sweep-cases.json','utf8')).cases,history=JSON.parse(readFileSync('tests/fixtures/original-three-port-backing.json','utf8')).histories[0];
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Three-port sweep worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}
try{
  assert.equal((await next()).ready,true);let lastRequest,lastResult;
  assert.match((await send({action:'port-sweep',port_kind:'three-port',start_ghz:1,stop_ghz:2,step_ghz:.5})).error,/layout/);
  for(const item of cases){
    const v=item.case;let layout;for(const [i,creator] of v.creators.entries()){const r=await send({technology,creator,options:{append:i>0}});assert.ok(!r.error,r.error);layout=r.layout;}
    if(v.ground&&!v.ground.startsWith('@'))layout=(await send({technology,creator:{id:2,keywords:v.ground},options:{append:true}})).layout;
    const indices=v.names.map(name=>layout.shapes.findIndex(s=>s.name===name)),ground=v.ground?v.ground.startsWith('@')?indices[Number(v.ground[1])-1]:layout.shapes.findIndex(s=>s.name==='G'):-1;
    const request={windings:indices,ground,mesh:{length:v.cells[0],width:v.cells[1],thickness:v.cells[2]},contact_mesh:{length:v.contact_cells[0],width:v.contact_cells[1]},fft_x:v.grid[0],fft_y:v.grid[1],filament_limit:1024,contact_limit:512,eddy:{mode:v.eddy_mode,layers:v.layers},nativeInput:true};
    worker.postMessage({id:++id,...request,action:'port-sweep',port_kind:'three-port',start_ghz:item.range_ghz[0],stop_ghz:item.range_ghz[1],step_ghz:item.range_ghz[2]});const frames=[];
    do{const frame=await next();assert.equal(frame.id,id);assert.ok(!frame.error,frame.error);assert.equal(frame.port_sweep.target,'three-port');assert.equal(frame.port_sweep.index,frames.length);frames.push(frame);}while(!frames.at(-1).port_sweep.done);
    assert.ok(frames.length>1);assert.equal(frames.length,frames[0].port_sweep.total);
    for(const [i,frame] of frames.entries()){
      assert.equal('system_matrix' in frame.three_port,i===frames.length-1);assert.equal('capacitance_matrix' in frame.three_port,i===frames.length-1);assert.ok(frame.three_port.impedance_matrix&&frame.three_port.scattering_matrix&&frame.three_port.admittance_matrix);
      const single=await send({...request,action:'three-port',frequency:frame.three_port.frequency_hz});assert.ok(!single.error,single.error);assert.deepEqual(portSampleSummary(frame.three_port),portSampleSummary(single.three_port),'Sweep reparsed or changed a saved native frequency');
    }
    const samples=frames.map(frame=>portSampleSummary(frame.three_port));for(const parameter of ['s','y','z']){assert.ok(portSweepRecord(samples,parameter).startsWith('\n!\t'+parameter.toUpperCase()+'11'));assert.equal(threePortSweepMatrixCSV(samples,parameter).trimEnd().split('\n').length,1+9*frames.length);}assert.equal(portSweepMetricsCSV(samples).trimEnd().split('\n').length,1+frames.length);
    lastRequest=request;lastResult=frames.at(-1).three_port;
  }
  for(const change of [{step_ghz:0},{step_ghz:1e-30,stop_ghz:1},{start_ghz:2,stop_ghz:1},{sample_limit:1},{sample_limit:1.5},{windings:[3,3,1]}]){
    const r=await send({...lastRequest,action:'port-sweep',port_kind:'three-port',start_ghz:1,stop_ghz:2,step_ghz:.5,...change});assert.ok(r.error,JSON.stringify(change));
    const recovered=await send({...lastRequest,action:'three-port',frequency:lastResult.frequency_hz});assert.deepEqual(recovered.three_port,lastResult);
  }
  let r=await send({...lastRequest,action:'port-sweep',port_kind:'three-port',start_ghz:1,stop_ghz:2,step_ghz:.5,eddy:{mode:1,layers:[0,1],evaluation_limit:1}});assert.match(r.error,/Port sample 1\/3.*budget/);
  r=await send({...lastRequest,action:'port-sweep',port_kind:'three-port',start_ghz:1,stop_ghz:1.5,step_ghz:.25,backing:history});assert.match(r.error,/capture.*one frequency/);
  r=await send({...lastRequest,action:'port-sweep',port_kind:'three-port',start_ghz:2,stop_ghz:2,step_ghz:.25,backing:history});assert.match(r.error,/frequency differs/);
  worker.postMessage({id:++id,...lastRequest,action:'port-sweep',port_kind:'three-port',start_ghz:1,stop_ghz:1,step_ghz:.25,backing:history});r=await next();assert.equal(r.id,id);assert.ok(!r.error,r.error);assert.equal(r.port_sweep.total,1);assert.equal(r.port_sweep.done,true);assert.equal(r.three_port.backing.captured,true);
  assert.deepEqual((await send({...lastRequest,action:'three-port',frequency:lastResult.frequency_hz})).three_port,lastResult,'Native-state replay leaked into the numerical model');
  worker.postMessage({id:++id,...lastRequest,action:'port-sweep',port_kind:'three-port',start_ghz:1,stop_ghz:26,step_ghz:.25});const sweepId=id;let completed=1;r=await next();assert.equal(r.id,sweepId);assert.ok(!r.error,r.error);assert.equal(r.port_sweep.done,false);
  worker.postMessage({id:++id,action:'cancel'});while(!(r=await next()).cancelled){assert.equal(r.id,sweepId);completed++;}assert.equal(r.id,id);assert.ok(completed<101);
  assert.deepEqual((await send({...lastRequest,action:'three-port',frequency:lastResult.frequency_hz})).three_port,lastResult,'Cancellation changed the following solve');
  console.log('Three-port sweep worker: default/separate references, saved frequencies, compact/final matrices, all S/Y/Z exports, single-frequency capture validation, budgets, cancellation and recovery pass');
}finally{await worker.terminate();}
