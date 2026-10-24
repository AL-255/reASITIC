import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import make from '../dist/asitic.js';
import {configureKernel,parseTechnology,createGeometry} from '../web/technology.mjs';
import {solverCells,meshResult} from '../web/mesh-policy.mjs';
import {portSampleSummary} from '../web/port-sweep.mjs';
const technology=readFileSync('web/technology/CMOS.tek','utf8'),kernel=await make();configureKernel(kernel,parseTechnology(technology));
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('MAXNW worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;
async function send(data){worker.postMessage({id:++id,...data});const r=await next();assert.equal(r.id,id);return r;}
async function checked(data){const r=await send(data);assert.ok(!r.error,r.error);return r;}
function direct(data,hz=1e9){
  const kind=data.action==='simulate'?'inductance':data.action==='capacitance'?'capacitance':'network',selection=solverCells(kernel,data,hz,kind),cells=selection.cells;
  const suffix=[...cells,512,512,16,16,128,256,0,0,0,0],call=(fn,...args)=>meshResult(JSON.parse(kernel.UTF8ToString(fn(...args))),selection);
  if(data.action==='simulate')return call(kernel._simulate_eddy,data.winding,hz,...cells,data.filament_limit??256,0,0,0,0,250000);
  if(data.action==='capacitance')return call(kernel._simulate_capacitance,data.winding,data.ground,hz,...cells,512,512,16,16,128,0);
  if(data.action==='transformer')return call(kernel._simulate_transformer,data.primary,data.secondary,data.ground,hz,...suffix,0,250000);
  if(data.action==='three-port')return call(kernel._simulate_three_port,...data.windings,data.ground,hz,...suffix,250000);
  if(data.model==='PI4')return call(kernel._simulate_pad_network,data.winding,...data.pads,data.ground,hz,...suffix,250000);
  return call(data.model==='PIX'?kernel._simulate_extended_network:data.model==='PI3'?kernel._simulate_pi3:kernel._simulate_network,data.winding,data.ground,hz,...suffix,250000);
}
try{
  assert.equal((await next()).ready,true);let layout;
  const creators=[['A',100,40,'M1',100,100],['B',100,40,'M3',100,180],['C',100,40,'M5',100,260],['P1',64,64,'M1',36,100],['P2',64,64,'M1',200,100],['G',100,40,'M2',100,100]];
  for(const [name,length,width,metal,x,y]of creators){const creator={id:2,keywords:`NAME=${name}:LEN=${length}:WID=${width}:METAL=${metal}:XORG=${x}:YORG=${y}`};layout=(await checked({technology,creator,options:{append:Boolean(layout)}})).layout;createGeometry(kernel,2,creator.keywords,'',{append:layout.shapes.length>1});}
  const index=name=>layout.shapes.findIndex(s=>s.name===name),base={winding:index('A'),ground:index('G'),primary:index('A'),secondary:index('B'),windings:['A','B','C'].map(index),pads:['P1','P2'].map(index),frequency_ghz:1,nativeInput:true,mesh:{length:1000,width:1,thickness:1,subdivision_trigger:1000},contact_mesh:{length:1000,width:1000},fft_x:16,fft_y:16,filament_limit:256,contact_limit:128,eddy:{mode:0,layers:[]}};
  let comparisons=0;
  for(const [action,field,model]of [['simulate','simulation'],['network','network','PI2'],['network','network','PI3'],['network','network','PIX'],['network','network','PI4'],['transformer','transformer'],['three-port','three_port']]){
    const request={...base,action,model};
    for(const maximum of [3,8,undefined]){const data={...request,max_width_cells:maximum},actual=(await checked(data))[field];assert.deepEqual(actual,direct(data),action+' '+(model??'')+' MAXNW '+maximum);assert.equal(actual.max_width_cells,maximum);comparisons++;}
  }
  const cap={...base,action:'capacitance',mesh:base.contact_mesh},expectedCAP=(await checked(cap)).capacitance;
  assert.deepEqual((await checked({...cap,max_width_cells:3})).capacitance,expectedCAP,'MAXNW changed the electrostatic solve');
  for(const [target,field]of [['network','network'],['transformer','transformer'],['three-port','three_port']]){
    const request={...base,action:target,model:'PI2',max_width_cells:4};worker.postMessage({id:++id,...request,action:'port-sweep',port_kind:target,start_ghz:1,stop_ghz:1.5,step_ghz:.5});let count=0;
    while(true){const r=await next();assert.equal(r.id,id);assert.ok(!r.error,r.error);const {dispatch,...sample}=portSampleSummary(r[field]);assert.deepEqual(sample,portSampleSummary(direct(request,r[field].frequency_hz)));if(target==='network')assert.equal(dispatch.model,'PI2');count++;if(r.port_sweep.done)break;}
    assert.equal(count,2);
  }
  const auto={...base,action:'simulate',automatic_cells:{alpha:.03,beta:.6},max_width_cells:3},automatic=(await checked(auto)).simulation;assert.equal(automatic.filaments,3);assert.deepEqual(automatic,direct(auto));
  const request={...base,action:'simulate',max_width_cells:3,filament_limit:4},expected=(await checked(request)).simulation;assert.equal(expected.filaments,3);
  assert.match((await send({...request,max_width_cells:500})).error,/Cross-section mesh/);assert.deepEqual((await checked(request)).simulation,expected);
  for(const maximum of [0,-1,1.5,NaN,Infinity,2147483648]){assert.match((await send({...request,max_width_cells:maximum})).error,/positive integer MAXNW/);assert.deepEqual((await checked(request)).simulation,expected);}
  assert.match((await send({...request,max_width_cells:1})).error,/MAXNW.*nonfinite/);assert.deepEqual((await checked(request)).simulation,expected);
  const originalDefault=(await checked({...base,action:'simulate'})).simulation;assert.equal(originalDefault.filaments,40);assert.equal(originalDefault.max_width_cells,undefined,'A previous request changed the default width limit');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/worker-maxnw.json',JSON.stringify({solver_comparisons:comparisons,port_sweep_families:3,automatic_cells_combined:true,capacitance_unchanged:true,default_restored:true,budget_and_input_recovery:true},null,2)+'\n');
  console.log(`MAXNW worker: ${comparisons} direct C++ comparisons across all inductive solvers, three port sweep families, automatic cells, CAP independence, budgets, invalid-input recovery and default restoration pass`);
}finally{await worker.terminate();}
