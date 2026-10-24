import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {resonanceHistoryCSV} from '../web/self-resonance.mjs';
const technology=readFileSync('web/technology/CMOS.tek','utf8'),definitions=JSON.parse(readFileSync('tests/fixtures/self-resonance-cases.json','utf8')).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-self-resonance.json','utf8'));
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('SELFRES worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const r=await next();assert.equal(r.id,id);return r;}
async function checked(data){const r=await send(data);assert.ok(!r.error,r.error);return r;}
const bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');};let queries=0;const reports=[];
try{
  assert.equal((await next()).ready,true);assert.match((await send({action:'resonance',start_ghz:1,end_ghz:2,step_ghz:.01})).error,/layout/);let lastRequest,lastResult;
  for(const c of definitions){
    const reference=fixture.cases.find(v=>v.name===c.name);let text=technology;for(const [key,value]of [['fftx',c.grid[0]],['ffty',c.grid[1]]])text=text.replace(new RegExp('^\\s*'+key+'\\s*=.*$','mi'),key+' = '+value);
    await checked({technology:text,creator:c.creator});
    const request={action:'resonance',winding:0,ground:c.ground?0:-1,start_ghz:c.bounds_ghz[0],end_ghz:c.bounds_ghz[1],step_ghz:c.step_ghz,mode:c.mode,fast_quantization:c.fast,native_iterations:c.native_iterations,mesh:{length:c.cells[0],width:c.cells[1],thickness:c.cells[2]},contact_mesh:{length:c.contact_cells[0],width:c.contact_cells[1]},fft_x:c.grid[0],fft_y:c.grid[1],automatic_cells:c.automatic_cells,nativeInput:true,filament_limit:1024,contact_limit:512,eddy:{mode:0,layers:[0,1]}};
    if(reference.native_fault){assert.match((await send(request)).error,/FAST.*fault/);continue;}
    worker.postMessage({id:++id,...request});const frames=[];
    do{const r=await next();assert.equal(r.id,id);assert.ok(!r.error,r.error);assert.equal(r.resonance.analyses,frames.length+1);frames.push(r.resonance);}while(!frames.at(-1).done);
    assert.equal(frames.length,reference.samples.length,c.name+' original analysis count');
    for(const [i,r]of frames.entries()){
      assert.equal(bits(r.sample.frequency_hz),reference.samples[i].frequency_hex,c.name+' original saved query');assert.equal(r.source_model,'PI2','Original creator classification changed');
      const values=r.sample.matrix.flat(2),expected=Array.from({length:8},(_,j)=>Buffer.from(reference.samples[i].admittance_hex,'hex').readDoubleLE(j*8)),scale=Math.max(...expected.map(Math.abs));values.forEach((v,j)=>assert.ok(Math.abs(v-expected[j])<=scale*1e-10,c.name+' original model Y'));queries++;
      if(c.automatic_cells)assert.equal(r.sample.automatic_cells.frequency_hz,r.sample.frequency_hz,'Automatic cells used a different frequency');
    }
    const result=frames.at(-1);assert.equal(bits(result.frequency_hz??-1),reference.return_frequency_hex[0],c.name+' original native return');assert.equal(result.native_message,reference.printed_result[0]);
    assert.equal(resonanceHistoryCSV(frames.map(r=>r.sample),result).trimEnd().split('\n').length,frames.length+1);
    if(c.name==='wire-unbracketed'){assert.equal(result.observed_sign_change,false);assert.equal(result.convergence,'frequency_step');assert.notEqual(result.frequency_hz,result.last_analyzed_hz,'Returned frequency was relabeled as an actual analysis');}
    reports.push({name:c.name,queries:frames.length,frequency_hz:result.frequency_hz,last_analyzed_hz:result.last_analyzed_hz,observed_sign_change:result.observed_sign_change,convergence:result.convergence});lastRequest=request;lastResult=result;
  }
  for(const change of [{sample_budget:0},{sample_budget:1.5},{step_ghz:0},{mode:'bad'},{fast_quantization:true},{winding:999}])assert.ok((await send({...lastRequest,...change})).error);
  const limited=await checked({...lastRequest,sample_budget:1});assert.equal(limited.resonance.status,'sample_limit');assert.equal(limited.resonance.done,true);assert.equal(limited.resonance.analyses,1);
  const failed=await send({...lastRequest,eddy:{mode:1,layers:[0,1],evaluation_limit:1}});assert.match(failed.error,/SELFRES analysis 1.*budget/);
  worker.postMessage({id:++id,...lastRequest});const searchId=id;let r=await next();assert.equal(r.id,searchId);assert.equal(r.resonance.done,false);let completed=1;worker.postMessage({id:++id,action:'cancel'});
  while(!(r=await next()).cancelled){assert.equal(r.id,searchId);assert.ok(!r.resonance.done,'Cancellation arrived after the whole search');completed++;}assert.equal(r.id,id);
  worker.postMessage({id:++id,...lastRequest});do{r=await next();assert.equal(r.id,id);assert.ok(!r.error,r.error);}while(!r.resonance.done);assert.deepEqual(r.resonance,lastResult,'Rejected or cancelled search changed following results');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/self-resonance-worker.json',JSON.stringify({original_queries:queries,reports,cooperative_cancel:true,cancelled_after:completed,recovery:true},null,2)+'\n');
  console.log(`SELFRES worker: ${queries} original model queries and native returned frequencies, automatic cells, truthful unbracketed results, sample budgets, faults, cancellation and recovery pass`);
}finally{await worker.terminate();}
