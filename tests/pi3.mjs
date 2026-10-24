import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {twoPortRecord,networkMetricsCSV} from '../web/network.mjs';
const definitions=readFileSync('tests/fixtures/pi3-cases.json'),cases=JSON.parse(definitions).cases,original=JSON.parse(readFileSync('tests/fixtures/original-pi3.json')),sha=v=>createHash('sha256').update(v).digest('hex');assert.equal(sha(definitions),original.case_definition_sha256);
const technologies=Object.fromEntries(['CMOS','BiCMOS'].map(name=>{const text=readFileSync('web/technology/'+name+'.tek','utf8');assert.equal(sha(text),original.technology_sha256[name]);return [name,text];}));
const kernel=await make(),bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`,worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',r=>waiters.length?waiters.shift()(r):inbox.push(r));function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('PI3 worker timeout')),60000);waiters.push(r=>{clearTimeout(timer);resolve(r);});});}let id=0;async function send(data){worker.postMessage({id:++id,...data});const r=await next();assert.equal(r.id,id);return r;}async function checked(data){const r=await send(data);assert.ok(!r.error,r.error);return r;}
const clean=({id,warnings,...rest})=>rest,reports=[];let fields=0,nativeCases=0,independentCases=0,different=0,lastRequest;
function nearMatrix(actual,expected,tolerance,label){assert.equal(actual.length,expected.length,label+' size');const a=actual.flat(Infinity),e=expected.flat(Infinity),scale=Math.max(...e.map(Math.abs),1e-300);assert.equal(a.length,e.length);a.forEach((v,i)=>assert.ok(Number.isFinite(v)&&Math.abs(v-e[i])<=scale*tolerance,label+' field '+i+': '+v+' vs '+e[i]+'; relative '+(Math.abs(v-e[i])/scale)));}
try{
 assert.equal((await next()).ready,true);
 for(const c of cases){
  let text=technologies[c.technology];for(const [key,value]of [['fftx',c.grid[0]],['ffty',c.grid[1]]])text=text.replace(new RegExp('^\\s*'+key+'\\s*=.*$','mi'),key+' = '+value);
  const technology=parseTechnology(text);configureKernel(kernel,technology);let layout=createGeometry(kernel,c.creator.id,c.creator.keywords,'',{domain:technology.domain});let workerLayout=(await checked({technology:text,creator:c.creator})).layout;
  if(c.ground){layout=createGeometry(kernel,2,c.ground,'',{append:true,domain:technology.domain});workerLayout=(await checked({technology:text,creator:{id:2,keywords:c.ground},options:{append:true}})).layout;}
  const winding=layout.shapes.findIndex(s=>s.name==='A'),ground=c.ground?layout.shapes.findIndex(s=>s.name==='G'):winding,hz=kernel._normalize_value(c.frequency_ghz,5,.1,0),cells=[...c.cells,...c.contact_cells].map(v=>kernel._normalize_value(v,7,.1,1)),args=[winding,ground,hz,...cells,technology.domain.x,technology.domain.y,...c.grid,512,1024,0,0,0,0,250000],capture=original.cases.find(v=>v.name===c.name);
  const request={action:'network',model:'PI3',winding,ground,frequency_ghz:c.frequency_ghz,mesh:{length:c.cells[0],width:c.cells[1],thickness:c.cells[2]},contact_mesh:{length:c.contact_cells[0],width:c.contact_cells[1]},fft_x:c.grid[0],fft_y:c.grid[1],contact_limit:512,filament_limit:1024,eddy:{mode:0,layers:[]},nativeInput:true},solve=(model,historical=false)=>{kernel._set_reference_numerics(historical?1:0);return JSON.parse(kernel.UTF8ToString((model==='PI3'?kernel._simulate_pi3:kernel._simulate_network)(...args)));};
  const results={};
  for(const model of ['PI3','PI2']){
   const diagnostic=solve(model,true),expected=capture[model];
   if(Object.values(expected.stage_matrices).flat(Infinity).some(v=>typeof v!=='number'||!Number.isFinite(v))){assert.match(diagnostic.error,/collaps|finite|degenerate|dimension|group/);assert.ok((await send({...request,model})).error);results[model]={error:diagnostic.error};continue;}
   assert.ok(!diagnostic.error,c.name+'/'+model+': '+diagnostic.error);assert.equal(diagnostic.green_table_hash,expected.green_table_hash,c.name+' original Green cache');
   for(const [key,file]of [['capacitance_matrix','ZC.out'],['inductance_admittance_matrix','YM.out'],['node_capacitance_matrix','Ycap.out']]){
    assert.deepEqual(diagnostic[key].map(row=>row.map(z=>z.map(v=>Number(v.toPrecision(10))))),expected.stage_matrices[file],c.name+'/'+model+' original '+file);fields+=diagnostic[key].flat(Infinity).length;
   }
   const p=diagnostic.pi;for(const [i,key]of ['quality_first','quality_second','quality_differential'].entries())assert.equal(Number(p[key].toFixed(3)),Number(expected.quality_printed[i]),c.name+' original '+key);
   const metrics=[p.inductance_h/1e-9,p.resistance,p.capacitance_second_f/1e-15,p.resistance_second,p.capacitance_first_f/1e-15,p.resistance_first,diagnostic.resonance_hz/1e9];metrics.forEach((v,i)=>assert.equal(Number(i===6?v.toFixed(2):v.toPrecision(3)),Number(expected.pi_printed[i]),c.name+'/'+model+' original metric '+i));
   diagnostic.input_impedance.forEach((z,i)=>z.forEach((value,j)=>assert.equal(value===null?NaN:Object.is(value,-0)?-0:Number(value.toFixed(3)),Number(expected.input_impedance_printed[i][j]),c.name+'/'+model+' original ZIN '+i+'/'+j)));
   for(const [i,key]of ['shunt_single','shunt_differential'].entries())assert.equal(p[key]===null?NaN:Number(p[key].toFixed(6)),Number.parseFloat(expected.shunt_printed[i]),c.name+'/'+model+' original SHUNTR '+i);
   const actual=solve(model);assert.ok(!actual.error,actual.error);if(model==='PI3')assert.equal(actual.model,'PI3');results[model]=actual;
   if(c.technology==='CMOS'){
    const native=JSON.parse(execFileSync('build/native-geometry',[model==='PI3'?'--network3':'--network',c.creator.id,c.creator.keywords,2,c.ground??'@self',hz,...cells,...c.grid,512,1024,0,0,0,0,250000].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));assert.ok(!native.error,native.error);assert.equal(actual.filaments,native.filaments);assert.deepEqual(actual.mesh,native.mesh);nearMatrix(actual.matrix,native.matrix,1e-10,c.name+'/'+model+' native ports');nativeCases++;
   }
   const response=(await checked({...request,model})).network;assert.deepEqual(response,actual,c.name+'/'+model+' worker/direct');
   if(model==='PI3'){
    const oracle=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_two_port.py'],{input:JSON.stringify(actual),encoding:'utf8'}));nearMatrix(actual.matrix,oracle.matrix,1e-14,c.name+' independent nodal matrix');
    for(const [key,value]of Object.entries(oracle.circuit_metrics.pi)){if(value===null)assert.equal(actual.pi[key],null,c.name+' '+key+' classification');else assert.ok(Math.abs(actual.pi[key]-value)<=Math.max(Math.abs(value),1e-300)*1e-12,c.name+' independent '+key);}
    independentCases++;assert.ok(twoPortRecord(actual).startsWith('\n# GHZ S RI '));assert.equal(networkMetricsCSV(actual).trim().split('\n').length,2);
   }
  }
  if(results.PI3&&!results.PI3.error){
   if(capture.PI3.stage_matrices['YM.out'].flat(Infinity).join(',')!==capture.PI2.stage_matrices['YM.out'].flat(Infinity).join(',')){assert.notDeepEqual(results.PI3.inductance_admittance_matrix,results.PI2.inductance_admittance_matrix,c.name+' PI3 was replaced by PI2');different++;}
   else nearMatrix(results.PI3.matrix,results.PI2.matrix,1e-14,c.name+' shared finite reference');
   const r=(await checked(request)).network;assert.deepEqual(r,results.PI3);assert.equal((await checked({action:'loaded-port'})).loaded_port.status,'finite');lastRequest=request;
  }
  const after=(await checked({action:'edit',commands:'UNSELECT'})).layout;assert.deepEqual(clean(after),clean(workerLayout),c.name+' changed live geometry');reports.push({name:c.name,originalNonfiniteModels:Object.keys(results).filter(model=>results[model].error),originalOutcomeChecked:true,sourcePreserved:true});
 }
 const request=lastRequest,baseline=clean(await checked(request));
 for(const patch of [{ground:-1},{ground:.5},{ground:2**32},{winding:.5},{filament_limit:0},{eddy:{mode:3,layers:[]}},{eddy:{mode:0,layers:[2**32]}}]){assert.ok((await send({...request,...patch})).error);assert.deepEqual(clean(await checked(request)),baseline,'Rejected PI3 request changed its following solve');}
 assert.match((await send({...request,action:'port-sweep',port_kind:'network',start_ghz:1,stop_ghz:1,step_ghz:1})).error,/PI3 is a single-frequency/);
 const missing=[...Array(19)].fill(0);missing[1]=-1;assert.match(JSON.parse(kernel.UTF8ToString(kernel._simulate_pi3(...missing))).error,/explicit ground/);
 mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/pi3.json',JSON.stringify({reports,originalNumericFields:fields,nativeCmosComparisons:nativeCases,legacyHostRelativeTolerance:1e-10,independentMatrixRelativeTolerance:1e-14,independentPi3Cases:independentCases,distinctPi3MeshCases:different,workerExact:true,requestRecovery:true,sourceImmutable:true},null,2)+'\n');console.log(`PI3: ${cases.length} original cases / 40 fresh reference runs, ${fields} original stage fields, ${nativeCases} native C++ comparisons, ${independentCases} independent circuits, ${different} distinct PI3 meshes, worker/export/recovery and immutable geometry pass`);
}finally{await worker.terminate();}
