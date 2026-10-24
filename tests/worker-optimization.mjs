import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {Worker} from 'node:worker_threads';
import {parseCreatorCommand} from '../web/creators.mjs';
import {writeGDS} from '../web/gds.mjs';
const multimetal=process.argv.includes('--multimetal'),prefix=multimetal?'multimetal-':'';
const cases=JSON.parse(readFileSync(`tests/fixtures/${prefix}optimization-cases.json`,'utf8')).cases,fixture=JSON.parse(readFileSync(`tests/fixtures/original-${prefix}optimization.json`,'utf8'));
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Optimization worker timeout')),60000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;function start(data){worker.postMessage({id:++id,...data});return id;}async function send(data){const request=start(data),result=await next();assert.equal(result.id,request);return result;}
const clean=({warnings,...rest})=>rest,settings={mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:128,fft_y:128,contact_limit:128,filament_limit:256,eddy:{mode:0,layers:[]},options:{candidate_limit:1000,search:multimetal?'SWEEPMM':'SWEEP'}};
let samples=0,applied=0,rejected=0;const prior=new Map();
try{
  assert.equal((await next()).ready,true);
  for(const c of cases){
    const technology=readFileSync(`web/technology/${c.technology}.tek`,'utf8'),creator=parseCreatorCommand(c.prelude??'WIRE NAME=KEEP:LEN=40:WID=8:METAL=0:XORG=100:YORG=100');
    const initial=await send({technology,creator});assert.ok(!initial.error,initial.error);
    const original=fixture.cases.find(v=>v.name===c.name),request=start({action:'optimize',technology,keywords:c.keywords,...settings}),results=[];
    while(true){
      const result=await next();assert.equal(result.id,request);assert.ok(!result.error,result.error);const r=result.optimization;assert.equal(r.total,original.candidates.length);
      if(r.sample){const s=r.sample,e=original.candidates[s.index];assert.equal(s.index,results.length);assert.deepEqual(s.descriptor,e.descriptor);if(e.pi.some(v=>typeof v!=='number')){assert.ok(s.error);assert.equal(s.accepted,false);rejected++;}else{assert.ok(!s.error,s.error);for(const [key,i]of [['inductance_h',1],['resistance',2],['quality_first',7]])assert.ok(Math.abs(s.pi[key]-e.pi[i])<=Math.abs(e.pi[i])*1e-8,c.name+' worker model '+key);}results.push(s);samples++;}
      if(r.done)break;
    }
    prior.set(c.name,results);assert.equal(results.filter(s=>s.accepted).length,original.accepted_printed.length,c.name+' accepted count');
    const unchanged=await send({action:'edit',commands:'UNSELECT'});assert.ok(!unchanged.error,unchanged.error);assert.deepEqual(clean(unchanged.layout),clean(initial.layout),c.name+' search mutated live layout');
    const index=results.find(s=>s.geometry_valid).index,response=await send({action:'optimization-apply',index});assert.ok(!response.error,response.error);assert.equal(response.layout.shapes.length,1);
    assert.deepEqual(writeGDS(response.layout,response.technology),writeGDS({shapes:[{name:'OPT',...original.candidates[index].geometry}]},response.technology),c.name+' applied original GDS');applied++;
  }
  if(multimetal){
    for(const [caseName,automatic]of [['square-two-metals',{alpha:.03,beta:40}],['simtype-one-numerical',{alpha:.03,beta:.000001}]]){
      const c=cases.find(v=>v.name===caseName),technology=readFileSync('web/technology/CMOS.tek','utf8');await send({technology,creator:parseCreatorCommand('WIRE NAME=KEEP:LEN=40:WID=8:METAL=M1')});
      const request=start({action:'optimize',technology,keywords:c.keywords,...settings,nativeInput:true,automatic_cells:automatic});let index=0;
      while(true){const result=await next();assert.equal(result.id,request);assert.ok(!result.error,result.error);const r=result.optimization;if(r.sample){assert.ok(!r.sample.error,r.sample.error);assert.deepEqual(r.sample.pi,prior.get(c.name)[index].pi,c.name+' automatic/manual coarse-model equivalence');assert.equal(Boolean(r.sample.automatic_cells),c.parameters.SIMTYPE!==1,c.name+' original AUTOCELL branch');index++;}if(r.done)break;}
      assert.equal(index,prior.get(c.name).length);
    }
  }
  const technology=readFileSync('web/technology/CMOS.tek','utf8'),initial=await send({technology,creator:parseCreatorCommand('WIRE NAME=KEEP:LEN=40:WID=8:METAL=M1')});assert.ok(!initial.error,initial.error);
  const c=cases.find(v=>v.name===(multimetal?'fractional-ranges':'square-ranges')),request=start({action:'optimize',technology,keywords:c.keywords,...settings});let result=await next();assert.equal(result.id,request);assert.ok(result.optimization?.sample&&!result.optimization.done);const first=result.optimization.sample.index,total=result.optimization.total,cancel=start({action:'cancel'});let completed=1;
  while(!(result=await next()).cancelled){assert.equal(result.id,request);completed++;}assert.equal(result.id,cancel);assert.ok(completed<total,'Cancellation did not stop the grid');
  const uncompleted=await send({action:'optimization-apply',index:total-1});assert.match(uncompleted.error,/completed/);
  const before=await send({action:'edit',commands:'UNSELECT'});assert.deepEqual(clean(before.layout),clean(initial.layout),'Cancellation changed live layout');
  const invalid=await send({action:'optimize',technology:readFileSync('web/technology/BiCMOS.tek','utf8'),keywords:cases[0].keywords,...settings,options:{candidate_limit:0,search:multimetal?'SWEEPMM':'SWEEP'}});assert.match(invalid.error,/budget/);
  const recovered=await send({action:'optimization-apply',index:first});assert.ok(!recovered.error,recovered.error);assert.equal(recovered.technology.metals.length,5,'Failed request contaminated retained search process');
  const solved=await send({action:'simulate',winding:0,frequency_ghz:1,mesh:{length:1000,width:1000,thickness:1000,subdivision_trigger:1000}});assert.ok(!solved.error,solved.error);assert.ok(solved.simulation.filaments>0);
  const badIndex=await send({action:'optimization-apply',index:4294967296});assert.match(badIndex.error,/completed/);
  mkdirSync('build/evidence',{recursive:true});writeFileSync(`build/evidence/worker-${prefix}optimization.json`,JSON.stringify({cases:cases.length,samples,applied,nativeNonfinite:rejected,cancellationCompleted:completed,cancellationTotal:total,sourceImmutable:true,invalidProcessRecovery:true,...(multimetal?{automaticCoarseModelExact:true,simtypeOneAutoBypass:true}:{})},null,2)+'\n');
  console.log(`SWEEP${multimetal?'MM':''} worker: ${samples} original candidates, ${applied} original GDS applications, ${rejected} nonfinite rejections, streamed results, cancellation, immutable live layout, process recovery and following simulation pass`);
}finally{await worker.terminate();}
