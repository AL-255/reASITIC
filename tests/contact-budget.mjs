import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {gunzipSync} from 'node:zlib';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
const hash=x=>createHash('sha256').update(x).digest('hex'),definitions=readFileSync('tests/fixtures/contact-budget-cases.json'),cases=JSON.parse(definitions).cases,fixture=JSON.parse(gunzipSync(readFileSync('tests/fixtures/original-contact-budget.json.gz'))),text=readFileSync('web/technology/CMOS.tek','utf8'),technology=parseTechnology(text),k=await make();
assert.equal(hash(definitions),fixture.case_definition_sha256);assert.equal(hash(text),fixture.technology_sha256);assert.equal(k._max_contact_count(),1024);assert.equal(k._max_contact_count()**2*16,16*1024*1024);
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`,worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Dense-contact worker timeout')),300000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const response=await next();assert.equal(response.id,id);return response;}
const modelIndex=process.argv.indexOf('--model'),selectedCases=modelIndex<0?cases:cases.filter(c=>c.model===process.argv[modelIndex+1]);assert.ok(selectedCases.length,'Use --model CAP or --model PI2');
const clean=({warnings,...rest})=>rest,reports=[];
function compareMatrix(actual,expected,label){assert.equal(actual.length,expected.length,label+' rows');let count=0;for(let i=0;i<actual.length;i++){assert.equal(actual[i].length,expected[i].length);for(let j=0;j<actual[i].length;j++)for(let p=0;p<2;p++){assert.equal(Number(actual[i][j][p].toPrecision(10)),expected[i][j][p],`${label}[${i}][${j}][${p}]`);count++;}}return count;}
function exactNumbers(actual,expected,label){if(typeof actual==='number')assert.equal(actual,expected,label);else if(actual&&typeof actual==='object'){assert.deepEqual(Object.keys(actual),Object.keys(expected),label);for(const key of Object.keys(actual))exactNumbers(actual[key],expected[key],label+'.'+key);}else assert.equal(actual,expected,label);}
function matrixNorm(actual,expected){const values=expected.flat(2),scale=values.reduce((m,v)=>Math.max(m,Math.abs(v)),1e-300);return actual.flat(2).reduce((m,v,i)=>Math.max(m,Math.abs(v-values[i])/scale),0);}
try{
  assert.equal((await next()).ready,true);
  for(const c of selectedCases){
    const original=fixture.cases.find(v=>v.name===c.name),hz=k._normalize_value(c.frequency_ghz,5,.1,0);configureKernel(k,technology);let before=createGeometry(k,c.id,c.keywords),winding=0,ground=-1;
    if(c.ground){before=createGeometry(k,2,c.ground,'',{append:true});winding=1;ground=0;}
    const invoke=limit=>JSON.parse(k.UTF8ToString(c.model==='CAP'?k._simulate_capacitance(winding,ground,hz,...c.contact_cells,512,512,...c.grid,limit,0):k._simulate_network(winding,ground,hz,...c.cells,...c.contact_cells,512,512,...c.grid,limit,1024,0,0,0,0,250000)));
    for(const limit of [512,original.contacts-1,1025,2147483647]){const bad=invoke(limit);assert.ok(bad.error,c.name+' rejected '+limit);}
    assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before),c.name+' rejection changed source');
    const start=performance.now(),actual=invoke(original.contacts);assert.ok(!actual.error,c.name+': '+actual.error);const directMs=performance.now()-start;
    assert.equal(actual.contacts,original.contacts);assert.equal(actual.green_table_hash,original.green_table_hash,c.name+' raw original Green hash');assert.ok(original.contacts>512);
    if(c.model==='CAP'){
      const native=JSON.parse(execFileSync('build/native-geometry',['--cap',c.id,c.keywords,2,c.ground??'',hz,...c.contact_cells,...c.grid,0,1024].map(String),{encoding:'utf8',maxBuffer:64*1024*1024}));assert.ok(!native.error,native.error);exactNumbers(actual,native,c.name+' native/WASM');
      const compared=compareMatrix(actual.potential_matrix,original.stages['Pmat.out'],c.name+' original potential');compareMatrix(actual.matrix,original.stages['YC.out'],c.name+' original folded CAP');
      assert.equal(Number((actual.capacitance_f*1e15).toFixed(3)),original.capacitance_ff_printed);assert.equal(Number(actual.resistance.toFixed(3)),original.resistance_printed);assert.equal(compared,2*original.contacts**2);
    }else{
      const native=JSON.parse(execFileSync('build/native-geometry',['--network',c.id,c.keywords,2,c.ground??'',hz,...c.cells,...c.contact_cells,...c.grid,1024,1024,0,0,0,0,250000].map(String),{encoding:'utf8',maxBuffer:16*1024*1024}));assert.ok(!native.error,native.error);
      for(const [key,stage]of [['capacitance_matrix','ZC.out'],['inductance_admittance_matrix','YM.out'],['node_capacitance_matrix','Ycap.out']]){
        compareMatrix(actual[key],original.stages[stage],c.name+' original '+stage);
        // Production inductance uses the guarded GMD formula. Historical x87
        // intermediates are a numerical comparison, not a production bit gate.
        if(key==='inductance_admittance_matrix')assert.ok(matrixNorm(actual[key],native[key])<=1e-12,c.name+' native inductance stage');else assert.deepEqual(actual[key],native[key],c.name+' native/WASM '+key);
      }
      assert.ok(matrixNorm(actual.matrix,native.matrix)<=1e-10,c.name+' original port solve');
      for(const [i,key]of ['quality_first','quality_second','quality_differential'].entries())assert.equal(Number(actual.pi[key].toFixed(3)),Number(original.quality_printed[i]),c.name+' original '+key);
      const p=actual.pi,values=[p.inductance_h/1e-9,p.resistance,p.capacitance_second_f/1e-15,p.resistance_second,p.capacitance_first_f/1e-15,p.resistance_first,actual.resonance_hz===null?null:actual.resonance_hz/1e9];
      values.forEach((v,i)=>assert.equal(v===null?NaN:Number(i===6?v.toFixed(2):v.toPrecision(3)),Number(original.pi_printed[i]),c.name+' original pi '+i));
      actual.input_impedance.forEach((z,i)=>z.forEach((v,j)=>assert.equal(v===null?NaN:Number(v.toFixed(3)),Number(original.input_impedance_printed[i][j]),c.name+' original ZIN')));
      for(const [i,key]of ['shunt_single','shunt_differential'].entries())assert.equal(actual.pi[key]===null?NaN:Number(actual.pi[key].toFixed(6)),Number.parseFloat(original.shunt_printed[i]),c.name+' original '+key);
    }
    assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before),c.name+' solve changed source');
    console.log(`${c.name}: ${original.contacts} contacts; original matrices/reports pass; checking worker`);
    let response=await send({technology:text,creator:{id:c.id,keywords:c.keywords}});assert.ok(!response.error,response.error);
    if(c.ground){response=await send({technology:text,creator:{id:2,keywords:c.ground},options:{append:true}});assert.ok(!response.error,response.error);}
    const request={action:c.model==='CAP'?'capacitance':'network',model:c.model,winding,ground,frequency_ghz:c.frequency_ghz,mesh:c.model==='CAP'?{length:c.contact_cells[0],width:c.contact_cells[1]}:{length:c.cells[0],width:c.cells[1],thickness:c.cells[2]},contact_mesh:{length:c.contact_cells[0],width:c.contact_cells[1]},fft_x:c.grid[0],fft_y:c.grid[1],filament_limit:1024,eddy:{mode:0,layers:[]},nativeInput:true};
    for(const limit of [0,1025,660.5,2**32+1024]){response=await send({...request,contact_limit:limit});assert.match(response.error,/contacts/);}
    response=await send({...request,contact_limit:original.contacts-1});assert.match(response.error,/contact budget/);
    const workerStart=performance.now();response=await send({...request,contact_limit:1024});assert.ok(!response.error,response.error);const result=c.model==='CAP'?response.capacitance:response.network;assert.deepEqual(result,actual,c.name+' worker/direct');
    reports.push({name:c.name,contacts:actual.contacts,mesh:actual.mesh,greenTableHash:actual.green_table_hash,directMilliseconds:directMs,workerMilliseconds:performance.now()-workerStart,nativeOriginalStagePrecision:true,sourcePreserved:true});
    console.log(`${c.name}: worker/direct parity and budget recovery pass`);
  }
  // Reject out-of-domain and wrapped budgets before solving, then recover on
  // a normal request without changing source geometry or mesh resolution.
  configureKernel(k,technology);createGeometry(k,2,'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100');
  const normal=()=>JSON.parse(k.UTF8ToString(k._simulate_capacitance(0,-1,1e9,1000,1000,512,512,8,8,128,0))),base=normal();assert.ok(!base.error,base.error);
  const heap=k.HEAPF64.buffer.byteLength;for(let i=0;i<3;i++)assert.deepEqual(normal(),base);assert.equal(k.HEAPF64.buffer.byteLength,heap,'Dense contact workspace leaked into following solves');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/contact-budget.json',JSON.stringify({models:selectedCases.map(c=>c.model),contactMatrixBytes:16*1024*1024,maximumContacts:k._max_contact_count(),originalExecutable:fixture.executable_sha256,integerBudgetValidation:true,recovery:true,reports},null,2)+'\n');
  console.log(`Dense-contact ${selectedCases.map(c=>c.model).join('/')}: original ${reports.map(r=>r.contacts).join('/')}-contact solves, native/WASM stage checks, worker parity, exact mesh preservation, integer guards and recovery pass`);
}finally{await worker.terminate();}
