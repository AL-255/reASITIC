import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {solverLimits,filamentBudget,eddySettings,portSampleBudget} from '../web/solver-request.mjs';
const k=await make(),limits=solverLimits(k),native=JSON.parse(execFileSync('build/native-geometry',['--solver-limits'],{encoding:'utf8'}));assert.deepEqual(limits,native);assert.equal(limits.filaments**2*16,limits.matrix_bytes);assert.equal(limits.filaments,k._max_contact_count());assert.ok(Object.isFrozen(limits));
assert.equal(filamentBudget(k,1),1);assert.equal(filamentBudget(k,limits.filaments),limits.filaments);assert.equal(portSampleBudget(k,limits.port_samples),limits.port_samples);
const technology=readFileSync('web/technology/CMOS.tek','utf8'),bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`,worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Integer-control worker timeout')),60000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}async function checked(data){const result=await send(data);assert.ok(!result.error,result.error);return result;}
const clean=({id,warnings,...rest})=>rest,reports=[];let rejected=0;
try{
  assert.equal((await next()).ready,true);let layout;
  for(const [name,length,width,metal,x,y]of [['A',100,40,'M1',100,100],['B',100,40,'M3',100,180],['C',100,40,'M5',100,260],['P1',64,64,'M1',36,100],['P2',64,64,'M1',200,100],['G',100,40,'M2',100,100]])layout=(await checked({technology,creator:{id:2,keywords:`NAME=${name}:LEN=${length}:WID=${width}:METAL=${metal}:XORG=${x}:YORG=${y}`},options:{append:Boolean(layout)}})).layout;
  const index=name=>layout.shapes.findIndex(s=>s.name===name),base={winding:index('A'),ground:index('G'),primary:index('A'),secondary:index('B'),windings:['A','B','C'].map(index),pads:['P1','P2'].map(index),frequency_ghz:1,nativeInput:true,mesh:{length:1000,width:1000,thickness:1000,subdivision_trigger:1000},contact_mesh:{length:1000,width:1000},fft_x:16,fft_y:16,filament_limit:128,contact_limit:128,eddy:{mode:0,layers:[]}};
  const definitions=[
    ['LMAT',{...base,action:'simulate',eddy:undefined},['winding'],true,false],
    ['LMAT eddy',{...base,action:'simulate'},['winding'],true,true],
    ['CAP',{...base,action:'capacitance',mesh:base.contact_mesh},['winding','ground'],false,false],
    ...['PI2','PI3','PIX','PI4'].map(model=>[model,{...base,action:'network',model},['winding','ground'],true,true]),
    ['CALCTRANS',{...base,action:'transformer'},['primary','secondary','ground'],true,true],
    ['3PORT',{...base,action:'three-port'},['ground'],true,true],
    ['SELFRES',{...base,action:'resonance',start_ghz:1,end_ghz:2,step_ghz:.1,mode:'single',sample_budget:1},['winding','ground'],true,true],
    ...['network','transformer','three-port'].map(target=>['sweep '+target,{...base,action:'port-sweep',port_kind:target,model:'PI2',start_ghz:1,stop_ghz:1,step_ghz:1,sample_limit:1},target==='transformer'?['primary','secondary','ground']:target==='three-port'?['ground']:['winding','ground'],true,true]),
  ];
  for(const [name,request,roles,filaments,eddy]of definitions){
    const expected=clean(await checked(request));let count=0;
    const invalid=async(patch,pattern)=>{const result=await send({...request,...patch});assert.match(result.error,pattern,name+' did not reject an invalid integer');rejected++;count++;};
    for(const role of roles)for(const bad of [.75,2**32+1,NaN,Infinity,'1',0n])await invalid({[role]:bad},/integer.*winding/);
    if(filaments)for(const bad of [0,-1,1.5,limits.filaments+1,2**32+128,NaN,Infinity,'128',1n])await invalid({filament_limit:bad},/integer filament limit/);
    if(eddy){
      for(const bad of [-1,.5,limits.eddy_mode+1,2**32+1,NaN,Infinity,'1',1n])await invalid({eddy:{...base.eddy,mode:bad}},/integer eddy mode/);
      for(const bad of [-1,.5,2,2**32,NaN,Infinity,'0',0n])await invalid({eddy:{...base.eddy,layers:[bad]}},/integer substrate layer index/);
      for(const bad of [0,-1,1.5,limits.green_evaluations+1,2**32+250000,NaN,Infinity,'250000',1n])await invalid({eddy:{...base.eddy,evaluation_limit:bad}},/integer Green evaluation budget/);
      for(const evaluation_limit of [1,limits.green_evaluations])assert.deepEqual(clean(await checked({...request,eddy:{...base.eddy,evaluation_limit}})),expected,name+' valid budget boundary changed a disabled-eddy model');
    }
    if(request.action==='port-sweep')for(const bad of [0,1.5,limits.port_samples+1,2**32+1,NaN,Infinity,'1',1n])await invalid({sample_limit:bad},/integer port sample limit/);
    if(request.action==='three-port'||request.port_kind==='three-port')for(const bad of [.75,2**32+index('A'),NaN,Infinity,'1',1n])await invalid({windings:[bad,index('B'),index('C')]},/three distinct winding/);
    if(request.model==='PI4')for(const bad of [.75,2**32+index('P1'),NaN,Infinity,'1',1n])await invalid({pads:[bad,index('P2')]},/pad records/);
    assert.deepEqual(clean(await checked(request)),expected,name+' rejected request changed the following result');assert.deepEqual(clean((await checked({action:'edit',commands:'UNSELECT'})).layout),clean(layout),name+' changed source geometry');reports.push({name,rejected:count,recoveryExact:true,sourcePreserved:true});
  }
  // Both documented eddy defaults remain valid; the published C++ limits are
  // inputs to validation, rather than values re-created in JavaScript.
  assert.deepEqual(eddySettings(k,false,{eddy_layers:[0],dielectric:[{},{}]}),{mode:0,layers:[0],budget:250000});
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/worker-integer-controls.json',JSON.stringify({limits,nativeWasmLimitsExact:true,requestsRejected:rejected,cases:reports},null,2)+'\n');console.log(`Integer controls: ${rejected} malformed/wrapping controls rejected across ${reports.length} solver/search/sweep routes; exact recovery, geometry preservation and native/WASM resource limits pass`);
}finally{await worker.terminate();}
