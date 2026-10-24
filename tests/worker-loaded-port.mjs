import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {loadedPortImpedance,loadedPortCSV} from '../web/loaded-port.mjs';
const definitions=readFileSync('tests/fixtures/loaded-port-cases.json'),items=JSON.parse(definitions).cases,original=JSON.parse(readFileSync('tests/fixtures/original-loaded-port.json')),technology=readFileSync('web/technology/CMOS.tek','utf8'),sha=v=>createHash('sha256').update(v).digest('hex');assert.equal(sha(definitions),original.case_definition_sha256);assert.equal(sha(technology),original.technology_sha256);
const doubles=hex=>{const b=Buffer.from(hex,'hex');return Array.from({length:b.length/8},(_,i)=>b.readDoubleLE(8*i));},matrix=hex=>{const v=doubles(hex);return [[v.slice(0,2),v.slice(4,6)],[v.slice(2,4),v.slice(6,8)]];};
const kernel=await make(),bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`,worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',r=>waiters.length?waiters.shift()(r):inbox.push(r));function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Loaded-port worker timeout')),60000);waiters.push(r=>{clearTimeout(timer);resolve(r);});});}let id=0;async function send(data){worker.postMessage({id:++id,...data});const r=await next();assert.equal(r.id,id);return r;}async function checked(data){const r=await send(data);assert.ok(!r.error,r.error);return r;}
const clean=({id,warnings,...rest})=>rest,reports=[];let lastRequest,lastTechnology;
try{
 assert.equal((await next()).ready,true);assert.match((await send({action:'loaded-port'})).error,/Solve a two-port/);
 for(const item of items){
  const capture=original.cases.find(c=>c.name===item.name),c=capture.case,raw={matrix:matrix(capture.base.admittance_hex),frequency_hz:doubles(capture.loaded.frequency_hex)[0]},options={port:item.port,load_real_ohm:item.load[0],load_imag_ohm:item.load[1],first_capacitance_f:item.caps[0],second_capacitance_f:item.caps[1],nativeInput:true};
  const fromRaw=loadedPortImpedance(kernel,raw,options);assert.deepEqual(fromRaw.load_ohm,doubles(capture.loaded.load_hex),item.name+' original float inputs');
  if(!item.original_nonfinite){
   const native=doubles(capture.loaded.impedance_hex),scale=Math.max(...native.map(Math.abs));native.forEach((v,i)=>assert.ok(Math.abs(v-fromRaw.impedance_ohm[i])<=scale*1e-11,item.name+' captured original output'));
   const reported=capture.printed[1].match(/([-+\d.e]+)\s*\+ j\s*([-+\d.e]+)/i).slice(1).map(Number);assert.equal(Number(fromRaw.impedance_ohm[0].toFixed(3)),reported[0]);assert.equal(Number(fromRaw.impedance_ohm[1].toFixed(2)),reported[1]);
  }else{assert.ok(doubles(capture.loaded.impedance_hex).every(Number.isNaN));assert.equal(fromRaw.status,'finite');}
  let tech=technology;for(const [key,value]of [['fftx',c.grid[0]],['ffty',c.grid[1]]])tech=tech.replace(new RegExp('^\\s*'+key+'\\s*=.*$','mi'),key+' = '+value);
  const creators=item.family==='transformer'?[c.primary,c.secondary]:item.family==='pad'?[c.device,...c.pads]:[{id:c.id,keywords:c.keywords}];if(c.ground)creators.push({id:2,keywords:c.ground});let layout;
  for(const creator of creators)layout=(await checked({technology:tech,creator,options:{append:Boolean(layout)}})).layout;
  const index=name=>layout.shapes.findIndex(s=>s.name===name),source=item.family==='transformer'?'transformer':'network',request={action:source,model:item.model,winding:index('A'),primary:index('A'),secondary:index('B'),pads:[index('P1'),index('P2')],ground:c.ground?index('G'):c.self_ground?index('A'):-1,frequency_ghz:c.frequency_ghz,mesh:{length:c.cells[0],width:c.cells[1],thickness:c.cells[2]},contact_mesh:{length:c.contact_cells[0],width:c.contact_cells[1]},fft_x:c.grid[0],fft_y:c.grid[1],filament_limit:1024,contact_limit:512,eddy:{mode:c.eddy_mode,layers:c.layers},nativeInput:true,termination_ohm:c.termination??0};
  const result=(await checked(request))[source],expected={...loadedPortImpedance(kernel,result,options),source},loaded=(await checked({action:'loaded-port',source,...options})).loaded_port;assert.deepEqual(loaded,expected,item.name+' worker/direct exact');
  if(!item.original_nonfinite){const reported=capture.printed[1].match(/([-+\d.e]+)\s*\+ j\s*([-+\d.e]+)/i).slice(1).map(Number);assert.equal(Number(loaded.impedance_ohm[0].toFixed(3)),reported[0],item.name+' full production original real report');assert.equal(Number(loaded.impedance_ohm[1].toFixed(2)),reported[1],item.name+' full production original imaginary report');}
  assert.equal(loaded.frequency_hz,raw.frequency_hz);assert.equal(loadedPortCSV(loaded).trim().split('\n').length,2);assert.equal(loadedPortCSV(loaded).trim().split('\n')[1].split(',').length,9);
  assert.deepEqual((await checked(request))[source],result,item.name+' loaded solve changed network or following model');
  lastRequest=request;lastTechnology=tech;reports.push({name:item.name,source,originalFinite:!item.original_nonfinite,originalReport:!item.original_nonfinite,workerDirectExact:true,followingSolveExact:true});
 }
 const options={action:'loaded-port',source:'network',port:1,nativeInput:true},expected=clean(await checked(options));
 for(const patch of [{port:.5},{port:2**32+1},{port:NaN},{port:1n},{source:'bogus'},{load_real_ohm:Infinity},{load_imag_ohm:'50'},{first_capacitance_f:NaN},{second_capacitance_f:1n},{load_real_ohm:Number.MAX_VALUE}]){assert.ok((await send({...options,...patch})).error);assert.deepEqual(clean(await checked(options)),expected);}
 assert.ok((await send({...lastRequest,filament_limit:0})).error);assert.deepEqual(clean(await checked(options)),expected,'Failed solver discarded last valid result');
 assert.ok((await send({technology:lastTechnology,creator:{id:2,keywords:'NAME=A:LEN=1:WID=1:METAL=M1'},options:{append:true}})).error);assert.deepEqual(clean(await checked(options)),expected,'Failed append discarded last valid result');
 await checked({action:'edit',commands:'UNSELECT'});assert.match((await send(options)).error,/Solve a two-port/);
 await checked(lastRequest);const appended=await checked({technology:lastTechnology,creator:{id:2,keywords:'NAME=NEW:LEN=20:WID=10:METAL=M1'},options:{append:true}});assert.match((await send(options)).error,/Solve a two-port/);
 const first=await checked({...lastRequest,winding:appended.layout.shapes.findIndex(s=>s.name==='A'),action:'port-sweep',port_kind:'network',start_ghz:1,stop_ghz:2,step_ghz:1});assert.equal(first.port_sweep.done,false);const final=await next();assert.equal(final.id,id);assert.equal(final.port_sweep.done,true);assert.equal((await checked(options)).loaded_port.frequency_hz,final.network.frequency_hz);
 await checked({technology,creator:{id:2,keywords:'NAME=OTHER:LEN=20:WID=10:METAL=M1'}});assert.match((await send(options)).error,/Solve a two-port/);
 const r={matrix:[[[1,0],[0,0]],[[0,0],[2,0]]],frequency_hz:1e9},before=structuredClone(r);loadedPortImpedance(kernel,r,{load_real_ohm:0});assert.deepEqual(r,before);
 assert.throws(()=>loadedPortImpedance(kernel,{...r,matrix:[[[null,0],[0,0]],[[0,0],[2,0]]]}),/finite admittance/);
 const heap=kernel.HEAPF64.buffer.byteLength,repeat=loadedPortImpedance(kernel,r);for(let i=0;i<256;++i)assert.deepEqual(loadedPortImpedance(kernel,r),repeat);assert.equal(kernel.HEAPF64.buffer.byteLength,heap,'Repeated loaded calculations grew the heap');
 mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/worker-loaded-port.json',JSON.stringify({reports,invalidRequestRecovery:true,layoutInvalidation:true,finiteShortCircuit:true,immutableInput:true},null,2)+'\n');console.log(`Loaded-port worker: ${reports.length} repeated original PI2/PIX/PI4/CALCTRANS cases, original reports, exact direct/WASM integration, input recovery, cache invalidation and finite short-circuit limit pass`);
}finally{await worker.terminate();}
