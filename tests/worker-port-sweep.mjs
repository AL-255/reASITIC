import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync} from 'node:fs';
import {portSampleSummary,portSweepRecord,portSweepScatteringCSV,portSweepMetricsCSV} from '../web/port-sweep.mjs';
const technology=readFileSync('web/technology/CMOS.tek','utf8'),cases=JSON.parse(readFileSync('tests/fixtures/port-sweep-cases.json','utf8')).cases;
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Port worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}
try{
  assert.equal((await next()).ready,true);
  assert.match((await send({action:'port-sweep',start_ghz:1,stop_ghz:2,step_ghz:.5})).error,/layout/);
  let lastRequest,lastResult;
  for(const item of cases){
    const v=item.case,family=item.family,creators=family==='transformer'?[v.primary,v.secondary]:family==='pad'?[v.device,...v.pads]:[{id:v.id,keywords:v.keywords}];let layout;
    for(const creator of creators.filter(Boolean)){const response=await send({technology,creator,options:{append:Boolean(layout)}});assert.ok(!response.error,response.error);layout=response.layout;}
    if(v.ground&&!v.ground.startsWith('@'))layout=(await send({technology,creator:{id:2,keywords:v.ground},options:{append:true}})).layout;
    const index=name=>layout.shapes.findIndex(s=>s.name===name),a=index('A'),b=index('B'),p1=index('P1'),p2=index('P2');
    const ground=v.ground?v.ground.startsWith('@')?v.ground==='@1'?(family==='transformer'?a:p1):v.ground==='@2'?(family==='transformer'?b:p2):a:index('G'):family==='network'?a:-1;
    const target=family==='transformer'?'transformer':'network',request={port_kind:target,model:family==='pad'?'PI4':family==='extended'?'PIX':'PI2',winding:a,primary:a,secondary:b,pads:[p1,p2],ground,termination_ohm:v.termination??0,mesh:{length:v.cells[0],width:v.cells[1],thickness:v.cells[2]},contact_mesh:{length:v.contact_cells[0],width:v.contact_cells[1]},fft_x:v.grid[0],fft_y:v.grid[1],filament_limit:1024,contact_limit:512,eddy:{mode:v.eddy_mode,layers:v.layers},nativeInput:true};
    worker.postMessage({id:++id,...request,action:'port-sweep',start_ghz:item.range_ghz[0],stop_ghz:item.range_ghz[1],step_ghz:item.range_ghz[2]});const frames=[];
    do{const frame=await next();assert.equal(frame.id,id);assert.ok(!frame.error,frame.error);assert.equal(frame.port_sweep.target,target);assert.equal(frame.port_sweep.index,frames.length);frames.push(frame);}while(!frames.at(-1).port_sweep.done);
    assert.equal(frames.length,frames[0].port_sweep.total);assert.ok(frames.length>1);
    for(const [i,frame] of frames.entries()){
      assert.equal('capacitance_matrix' in frame[target],i===frames.length-1,'Full matrices only for final sample');
      const single=await send({...request,action:target,frequency:frame[target].frequency_hz});assert.ok(!single.error,single.error);
      const {dispatch,...sample}=portSampleSummary(frame[target]);assert.deepEqual(sample,portSampleSummary(single[target]),'Sweep sample differs from the same saved native single frequency');
      if(target==='network'&&request.model!=='PI4')assert.equal(dispatch.model,request.model);
    }
    const samples=frames.map(frame=>portSampleSummary(frame[target]));assert.ok(portSweepRecord(samples,'s').startsWith('\n# GHZ S RI '));assert.equal(portSweepScatteringCSV(samples).trimEnd().split('\n').length,1+4*samples.length);assert.equal(portSweepMetricsCSV(samples).trimEnd().split('\n').length,1+samples.length);
    lastRequest=request;const {dispatch,...last}=frames.at(-1)[target];lastResult=last;
  }
  for(const change of [{step_ghz:0},{step_ghz:-1},{step_ghz:1e-30,stop_ghz:1},{start_ghz:2,stop_ghz:1},{sample_limit:1},{sample_limit:1.5},{port_kind:'invalid'}]){
    const result=await send({...lastRequest,action:'port-sweep',start_ghz:1,stop_ghz:2,step_ghz:.5,...change});assert.ok(result.error,JSON.stringify(change));
    const recovered=await send({...lastRequest,action:lastRequest.port_kind,frequency:lastResult.frequency_hz});assert.deepEqual(recovered[lastRequest.port_kind],lastResult,'Rejected range contaminated solver');
  }
  const rejected=await send({...lastRequest,action:'port-sweep',start_ghz:1,stop_ghz:2,step_ghz:.5,eddy:{mode:1,layers:[0,1],evaluation_limit:1}});assert.match(rejected.error,/Port sample 1\/3.*budget/);
  worker.postMessage({id:++id,...lastRequest,action:'port-sweep',start_ghz:1,stop_ghz:101,step_ghz:1});const sweepId=id;let completed=0,result=await next();assert.ok(!result.error,result.error);assert.equal(result.id,sweepId);assert.equal(result.port_sweep.done,false);completed++;
  worker.postMessage({id:++id,action:'cancel'});
  while(!(result=await next()).cancelled){assert.equal(result.id,sweepId);completed++;}
  assert.equal(result.id,id);assert.ok(completed<101,'Port cancellation did not stop remaining samples');
  const recovered=await send({...lastRequest,action:lastRequest.port_kind,frequency:lastResult.frequency_hz});assert.deepEqual(recovered[lastRequest.port_kind],lastResult,'Cancellation contaminated following solve');
  console.log('Port worker: all four solvers, exact saved frequencies, compact streaming/final matrices, full export history, range/budget recovery and cooperative cancellation pass');
}finally{await worker.terminate();}
