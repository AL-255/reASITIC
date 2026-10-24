import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseCreatorCommand} from '../web/creators.mjs';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
import {portSampleSummary,portSweepRecord,portSweepMetricsCSV,portSweepScatteringCSV} from '../web/port-sweep.mjs';
import {capacitorPortMatrix,capacitorPortResponse} from '../web/self-resonance.mjs';
import {networkMetricsCSV,twoPortRecord} from '../web/network.mjs';
const definitions=readFileSync('tests/fixtures/port-dispatch-cases.json','utf8'),cases=JSON.parse(definitions).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-port-dispatch.json','utf8')),hash=x=>createHash('sha256').update(x).digest('hex');assert.equal(hash(definitions),fixture.case_definition_sha256);
const technologies=Object.fromEntries(Object.keys(fixture.technology_sha256).map(name=>{const text=readFileSync(`web/technology/${name}.tek`,'utf8');assert.equal(hash(text),fixture.technology_sha256[name]);return [name,{text,parsed:parseTechnology(text)}];}));
const ordered=shapes=>[...shapes].sort((a,b)=>a.name.localeCompare(b.name));
function physical(layout,count){const segments=values=>values.map(s=>({metal:s.metal,width:s.width,points:s.points,...(s.metal<count?{thickness:s.thickness}:{nx:s.nx,ny:s.ny}),...(s.branches?{branches:segments(s.branches)}:{})}));return ordered(layout.shapes.map(s=>({name:s.name,origin:s.origin,segments:segments(s.segments),...(s.hidden?{hidden:true}:{})})));}
const bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');};
function compare(a,b,label,tolerance=1e-10){assert.equal(a.length,b.length,label+' shape');assert.ok(a.every(Number.isFinite)&&b.every(Number.isFinite),label+' finite');const norm=Math.max(...b.map(Math.abs),1e-300),error=Math.max(...a.map((v,i)=>Math.abs(v-b[i])))/norm;assert.ok(error<=tolerance,label+' norm error '+error);return error;}
function recordError(actual,expected){const header=s=>s.split('\n').filter(l=>/^[#!]/.test(l)),values=s=>s.split('\n').filter(l=>l.trim()&&!/^[#!]/.test(l)).flatMap(l=>l.trim().split(/\s+/).map(Number));assert.deepEqual(header(actual),header(expected));return compare(values(actual),values(expected),'Original complete port record');}
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Port dispatch worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}
async function checked(data){const result=await send(data);assert.ok(!result.error,result.error);return result;}
const k=await make(),reports=[];let rawFields=0,files=0,maxOriginalError=0,maxReferenceError=0,classified=0;
const plan=(index,extended)=>JSON.parse(k.UTF8ToString(k._port_analysis_plan(index,extended)));
function solve(model,winding,hz,c,ground=c.model==='PI2'||c.explicit_ground?winding:-1){const cells=[...c.cells,...c.contact_cells].map(v=>k._normalize_value(v,7,.1,1)),solver=model==='PIX'?k._simulate_extended_network:k._simulate_network;return JSON.parse(k.UTF8ToString(solver(winding,ground,hz,...cells,512,512,...c.grid,512,1024,0,0,0,0,250000)));}
try{
  assert.equal((await next()).ready,true);assert.ok(plan(0,1).error);
  let lastRequest,lastFirstResult;
  for(const c of cases){
    const original=fixture.cases.find(v=>v.name===c.name),technology=technologies[c.technology],creator=parseCreatorCommand(c.command,c.answers);
    configureKernel(k,technology.parsed);let layout=createGeometry(k,creator.id,creator.keywords,creator.answers);await checked({technology:technology.text,creator});
    if(c.edits){layout=editGeometry(k,c.edits);await checked({action:'edit',commands:c.edits});}
    assert.deepEqual(physical(layout,original.metal_count),ordered(original.shapes),c.name+' original physical input');
    const winding=layout.shapes.findIndex(s=>s.name===c.winding),dispatch=plan(winding,c.model==='PIX'?1:0),observations=original.rows.filter(r=>r.parameter===1&&!r.polar);
    assert.equal(dispatch.model,observations[0].model,c.name+' actual native driver');assert.equal(dispatch.shape_kind,observations[0].shape_kind);assert.equal(Boolean(dispatch.warning),original.warning_count>0);assert.equal(original.warning_count,dispatch.warning?12:0);
    const actualSamples=[];
    for(const row of observations){
      const hz=Buffer.from(row.frequency_hex,'hex').readDoubleLE(),actual=solve(dispatch.model,winding,hz,c);assert.ok(!actual.error,c.name+': '+actual.error);
      k._set_reference_numerics(1);const historical=solve(dispatch.model,winding,hz,c);k._set_reference_numerics(0);assert.ok(!historical.error,historical.error);
      assert.equal(historical.matrix.flat(2).map(bits).join(''),row.admittance_hex,c.name+' retained original Y');rawFields+=8;
      const native=Array.from({length:8},(_,i)=>Buffer.from(row.admittance_hex,'hex').readDoubleLE(8*i));maxOriginalError=Math.max(maxOriginalError,compare(actual.matrix.flat(2),native,c.name+' production original Y'));
      const reference=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',[dispatch.model==='PIX'?'tools/reference_extended.py':'tools/reference_two_port.py'],{input:JSON.stringify(actual),encoding:'utf8',maxBuffer:16*1024*1024}));assert.equal(reference.mpmath_version,'1.3.0');maxReferenceError=Math.max(maxReferenceError,compare(actual.matrix.flat(2),reference.matrix.flat(2),c.name+' independent circuit',1e-12));
      actualSamples.push(actual);
    }
    const request={action:'port-sweep',port_kind:'network',model:c.model,winding,ground:c.default_ground?-1:c.model==='PI2'||c.explicit_ground?winding:-1,start_ghz:c.range_ghz[0],stop_ghz:c.range_ghz[1],step_ghz:c.range_ghz[2],mesh:{length:c.cells[0],width:c.cells[1],thickness:c.cells[2]},contact_mesh:{length:c.contact_cells[0],width:c.contact_cells[1]},fft_x:c.grid[0],fft_y:c.grid[1],eddy:{mode:0,layers:[]},filament_limit:1024,contact_limit:512,nativeInput:true};
    worker.postMessage({id:++id,...request});const frames=[];
    do{const frame=await next();assert.equal(frame.id,id);assert.ok(!frame.error,c.name+': '+frame.error);assert.deepEqual(frame.network.dispatch,dispatch);assert.equal(frame.network.frequency_hz,actualSamples[frames.length].frequency_hz);assert.equal(Boolean(frame.warnings.includes(dispatch.warning)&&dispatch.warning),Boolean(dispatch.warning));frames.push(frame);}while(!frames.at(-1).port_sweep.done);
    assert.equal(frames.length,3);
    for(const [i,frame]of frames.entries()){
      const {dispatch,...actual}=frame.network;assert.deepEqual(actual,i===2?actualSamples[i]:portSampleSummary(actualSamples[i]),c.name+' worker dispatched solver');
      assert.equal('potential_matrix' in frame.network,false,'Inductive solve published unrelated capacitor workspace');
    }
    for(const [key,originalRecord] of Object.entries(original.records)){const parameter=key.startsWith('s')?'s':'y',polar=key.endsWith('polar');recordError(portSweepRecord(frames.map(f=>f.network),parameter,polar),originalRecord);files++;}
    assert.equal(portSweepMetricsCSV(frames.map(f=>f.network)).trim().split('\n').length,4);assert.equal(portSweepScatteringCSV(frames.map(f=>f.network)).trim().split('\n').length,13);
    const direct=await checked({...request,action:'network',frequency_ghz:1});
    const explicit=solve(c.model,winding,1e9,c,request.ground);assert.ok(!explicit.error,explicit.error);assert.deepEqual(direct.network,explicit,c.name+' direct PIX/PI2 changed');
    if(c.model==='PIX'&&dispatch.model==='PI2'){assert.equal(direct.network.model,'PIX');assert.ok(direct.network.currents);assert.ok(!frames.at(-1).network.currents,'Fallback still used PIX');}
    const after=await checked({action:'edit',commands:'UNSELECT'});assert.deepEqual(physical(after.layout,original.metal_count),ordered(original.shapes),c.name+' sweep mutated source');
    lastRequest=request;lastFirstResult={...actualSamples[0],dispatch};reports.push({name:c.name,shape_kind:dispatch.shape_kind,model:dispatch.model,samples:frames.length});console.log(c.name+': original driver, raw Y, four S/Y files, independent circuit and worker routing pass');
  }
  // Classify actual output metadata for every original creator/process case,
  // including constructors that turn a type-4/18 descriptor into joined type 21.
  for(const c of JSON.parse(readFileSync('tests/fixtures/creator-coverage-cases.json','utf8')).cases){
    configureKernel(k,technologies[c.technology].parsed);const creator=parseCreatorCommand(c.command,c.answers),layout=createGeometry(k,creator.id,creator.keywords,creator.answers);
    for(let index=0;index<layout.shapes.length;index++)for(const extended of [0,1]){const kind=k._shape_kind(index),actual=plan(index,extended);assert.equal(actual.model,kind===4?'CAP':extended&&[0,1,16,17,18].includes(kind)?'PIX':'PI2');classified++;}
  }
  for(const args of [[-1,1],[999,1],[0,-1],[0,2]])assert.ok(plan(...args).error);
  // Type-4 records are a distinct native dispatch branch. The current native
  // CAPACITOR creator joins its plates into type 21; do not relabel it to reach
  // this branch. Check the extracted 2x2 conversion and defined S/Y exports on
  // an analytic matrix independently of any manufactured native shape.
  const cap={frequency_hz:1e9,matrix:[[[1,2],[.125,.25]],[[.125,.25],[2,4]]]},capacitor=capacitorPortResponse(k,cap);
  assert.deepEqual(capacitor.matrix,capacitorPortMatrix(k,cap));assert.deepEqual(capacitor.matrix,[[[2.125,4.25],[-.125,-.25]],[[-.125,-.25],[1.125,2.25]]]);
  assert.deepEqual(Object.keys(capacitor.two_port.records),['s','y']);assert.equal(capacitor.pi,undefined);assert.throws(()=>twoPortRecord(capacitor,'pi'));assert.ok(twoPortRecord(capacitor,'s'));assert.equal(networkMetricsCSV(capacitor).trim().split('\n').length,2);assert.equal(portSweepMetricsCSV([capacitor]).trim().split('\n').length,2);
  const pointer=k._malloc(64);try{k.HEAPF64.set(cap.matrix.flat(2),pointer/8);for(const hz of [0,-1,NaN,Infinity])assert.ok(JSON.parse(k.UTF8ToString(k._capacitor_port_response(pointer,hz))).error);k.HEAPF64[pointer/8]=NaN;assert.ok(JSON.parse(k.UTF8ToString(k._capacitor_port_response(pointer,1e9))).error);}finally{k._free(pointer);}
  assert.deepEqual(capacitorPortResponse(k,cap),capacitor,'Failed capacitor conversion contaminated the next request');
  for(const winding of [-1,.5,4294967296])assert.ok((await send({...lastRequest,winding,start_ghz:1,stop_ghz:1,step_ghz:.1})).error,'Port winding wrapped at the C ABI');
  for(const ground of [-2,.5,4294967296])for(const action of ['network','port-sweep'])assert.ok((await send({...lastRequest,action,ground,frequency_ghz:1,start_ghz:1,stop_ghz:1,step_ghz:.1})).error,'Port ground wrapped at the C ABI');
  worker.postMessage({id:++id,...lastRequest,start_ghz:1,stop_ghz:1,step_ghz:.1});const recovered=await next();assert.equal(recovered.id,id);assert.ok(!recovered.error,recovered.error);assert.deepEqual(recovered.network,lastFirstResult,'Port request recovery');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/port-dispatch.json',JSON.stringify({cases:reports,originalRawFields:rawFields,originalCompleteFiles:files,classifiedCreatorOutputs:classified,maxOriginalError,maxReferenceError,directPIXPreserved:true,sourceUnchanged:true,capacitorConversion:true,invalidRecovery:true},null,2)+'\n');
  console.log(`Port dispatch: ${cases.length} original cases, ${rawFields} raw Y fields, ${files} complete S/Y files, ${classified} creator-output classifications; direct PIX, worker, exports and recovery pass`);
}finally{await worker.terminate();}
