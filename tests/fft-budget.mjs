import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
const text=readFileSync('web/technology/CMOS.tek','utf8'),technology=parseTechnology(text),definitions=readFileSync('tests/fixtures/fft-budget-cases.json'),cases=JSON.parse(definitions).cases,fixture=JSON.parse(readFileSync('tests/fixtures/original-fft-budget.json','utf8')),hash=x=>createHash('sha256').update(x).digest('hex'),k=await make();
assert.equal(hash(text),fixture.technology_sha256);assert.equal(hash(definitions),fixture.case_definition_sha256);assert.ok(fixture.collapsed_grid_runs.every(c=>!c.finite_capacitance));
let budgetComparisons=0,nativeBudgetComparisons=0;
const fits=(nx,ny,metals,historical)=>{if(nx<1||ny<1||!Number.isInteger(Math.log2(nx))||!Number.isInteger(Math.log2(ny))||metals<1||metals>32)return false;const x=BigInt(nx),y=BigInt(ny),n=x>y?x:y,cells=historical?(n+1n)**2n+4n*n:(x+1n)*(y+1n)+2n*x+2n*y;return cells*16n*BigInt(metals*(metals+1)/2)<=128n*1024n*1024n;};
for(const metals of [1,2,5,32]){
  configureKernel(k,{...technology,metals:Array.from({length:metals},(_,i)=>({...technology.metals[i%5],name:'M'+(i+1)}))});
  for(const historical of [false,true]){
    k._set_reference_numerics(historical?1:0);
    for(let a=0;a<=30;a++)for(let b=0;b<=30;b++){const nx=2**a,ny=2**b;assert.equal(Boolean(k._potential_grid_fits(nx,ny)),fits(nx,ny,metals,historical),`${metals} metals, ${nx}x${ny}, historical=${historical}`);budgetComparisons++;}
    for(const [nx,ny]of [[1,1],[512,512],[1024,512],[1024,1024],[131072,1],[1,131072],[262144,1],[2**30,2**30]]){assert.equal(Number(execFileSync('build/native-geometry',['--fft-budget',nx,ny,metals,historical?1:0].map(String),{encoding:'utf8'})),Number(fits(nx,ny,metals,historical)));nativeBudgetComparisons++;}
    for(const n of [-2147483648,-1,0,3,6,2147483647])assert.equal(k._potential_grid_fits(n,8),0);
  }
}
k._set_reference_numerics(0);configureKernel(k,technology);console.log(`FFT admission: ${budgetComparisons} BigInt resource comparisons and ${nativeBudgetComparisons} native/WASM boundary checks pass`);
const reports=[],modelIndex=process.argv.indexOf('--model'),selected=modelIndex<0?cases:cases.filter(c=>c.model===process.argv[modelIndex+1]);assert.ok(selected.length,'Use --model CAP or PI2');
const matrixNorm=(a,b)=>{const av=a.flat(Infinity),bv=b.flat(Infinity),scale=bv.reduce((m,v)=>Math.max(m,Math.abs(v)),1e-300);assert.equal(av.length,bv.length);return av.reduce((m,v,i)=>Math.max(m,Math.abs(v-bv[i])/scale),0);};
function originalMatrix(actual,expected,label){assert.equal(actual.length,expected.length);for(let i=0;i<actual.length;i++){assert.equal(actual[i].length,expected[i].length);for(let j=0;j<actual[i].length;j++)for(let p=0;p<2;p++)assert.equal(Number(actual[i][j][p].toPrecision(10)),expected[i][j][p],`${label}[${i}][${j}][${p}]`);}}
if(!process.argv.includes('--admission-only')){
  const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`,worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
  worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
  function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('512-grid worker timeout')),900000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
  let id=0;async function send(data){worker.postMessage({id:++id,...data});const response=await next();assert.equal(response.id,id);return response;}
  const clean=({warnings,...rest})=>rest;
  try{
    assert.equal((await next()).ready,true);
    for(const c of selected){
      configureKernel(k,technology);const before=createGeometry(k,c.id,c.keywords),original=fixture.cases.find(v=>v.name===c.name),invoke=(nx,ny)=>JSON.parse(k.UTF8ToString(c.model==='CAP'?k._simulate_capacitance(0,-1,original.frequency_hz,...c.contact_cells,512,512,nx,ny,128,0):k._simulate_network(0,-1,original.frequency_hz,...c.cells,...c.contact_cells,512,512,nx,ny,128,128,0,0,0,0,250000)));
      assert.match(invoke(256,256).error,/collapses/);for(const grid of [[1024,1024],[2**30,2**30],[262144,1]])assert.match(invoke(...grid).error,/128 MiB/);
      assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before));const start=performance.now(),actual=invoke(...c.grid);assert.ok(!actual.error,actual.error);const directMs=performance.now()-start;assert.equal(actual.contacts,original.contacts);assert.equal(actual.green_table_hash,original.green_table_hash,c.name+' complete original raw Green payload');
      if(c.model==='CAP'){
        const native=JSON.parse(execFileSync('build/native-geometry',['--cap',c.id,c.keywords,2,'',original.frequency_hz,...c.contact_cells,...c.grid,0,128].map(String),{encoding:'utf8'}));
        // Production raw tables are pinned to the original above. The host
        // x87 harness can have processor-dependent table bytes; compare its
        // complete matrices and observable CAP results, rather than using a
        // host-table fingerprint as an accuracy test.
        const {green_table_hash:nativeTableHash,...nativeResults}=native,{green_table_hash:originalTableHash,...actualResults}=actual;
        assert.match(nativeTableHash,/^[a-f0-9]{16}$/);assert.deepEqual(actualResults,nativeResults,c.name+' native/WASM results');
        originalMatrix(actual.potential_matrix,original.stages['Pmat.out'],c.name+' P');originalMatrix(actual.matrix,original.stages['YC.out'],c.name+' C');assert.equal(Number((actual.capacitance_f*1e15).toFixed(3)),original.capacitance_ff_printed);assert.equal(Number(actual.resistance.toFixed(3)),original.resistance_printed);
      }else{
        const native=JSON.parse(execFileSync('build/native-geometry',['--network',c.id,c.keywords,2,'',original.frequency_hz,...c.cells,...c.contact_cells,...c.grid,128,128,0,0,0,0,250000].map(String),{encoding:'utf8'}));assert.ok(!native.error,native.error);
        for(const [key,name]of [['capacitance_matrix','ZC.out'],['inductance_admittance_matrix','YM.out'],['node_capacitance_matrix','Ycap.out']]){originalMatrix(actual[key],original.stages[name],c.name+' '+name);assert.ok(matrixNorm(actual[key],native[key])<1e-12,c.name+' native stage '+key);}
        const circuit=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_two_port.py','--nodes-only'],{input:JSON.stringify(actual),encoding:'utf8'}));assert.ok(matrixNorm(actual.matrix,circuit.matrix)<1e-13,c.name+' independent circuit');
        for(const [i,key]of ['quality_first','quality_second','quality_differential'].entries())assert.equal(Number(actual.pi[key].toFixed(3)),Number(original.quality_printed[i]));
        const p=actual.pi,values=[p.inductance_h/1e-9,p.resistance,p.capacitance_second_f/1e-15,p.resistance_second,p.capacitance_first_f/1e-15,p.resistance_first,actual.resonance_hz/1e9];values.forEach((v,i)=>assert.equal(Number(i===6?v.toFixed(2):v.toPrecision(3)),Number(original.pi_printed[i]),c.name+' pi '+i));
        actual.input_impedance.forEach((z,i)=>z.forEach((v,j)=>assert.equal(Number(v.toFixed(3)),Number(original.input_impedance_printed[i][j]),c.name+' ZIN')));for(const [i,key]of ['shunt_single','shunt_differential'].entries())assert.equal(actual.pi[key]===null?NaN:Number(actual.pi[key].toFixed(6)),Number.parseFloat(original.shunt_printed[i]),c.name+' '+key);
      }
      assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before));console.log(`${c.name}: original raw tables, stages and reports pass (${(directMs/1000).toFixed(1)} s); checking worker`);
      let response=await send({technology:text,creator:{id:c.id,keywords:c.keywords}});assert.ok(!response.error,response.error);
      const request={action:c.model==='CAP'?'capacitance':'network',model:c.model,frequency_ghz:c.frequency_ghz,nativeInput:true,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:512,fft_y:512,contact_limit:128,filament_limit:128,eddy:{mode:0,layers:[]}};
      for(const bad of [0,-1,3,512.5,2**32+512,2**31,NaN,Infinity]){response=await send({...request,fft_x:bad});assert.match(response.error,/FFT/);}
      for(const grid of [[1024,1024],[2**30,2**30],[262144,1]]){response=await send({...request,fft_x:grid[0],fft_y:grid[1]});assert.match(response.error,/128 MiB/);}
      response=await send({...request,fft_x:256,fft_y:256});assert.match(response.error,/collapses/);const workerStart=performance.now();response=await send(request);assert.ok(!response.error,response.error);assert.deepEqual(c.model==='CAP'?response.capacitance:response.network,actual,c.name+' worker');reports.push({name:c.name,grid:c.grid,contacts:actual.contacts,greenTableHash:actual.green_table_hash,originalGreenBytes:original.raw_cache_bytes,directMilliseconds:directMs,workerMilliseconds:performance.now()-workerStart,sourcePreserved:true,workerExact:true});console.log(c.name+': worker/direct parity and rejection recovery pass');
    }
    configureKernel(k,technology);createGeometry(k,2,'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100');const normal=()=>JSON.parse(k.UTF8ToString(k._simulate_capacitance(0,-1,1e9,1000,1000,512,512,8,8,128,0))),base=normal(),heap=k.HEAPF64.buffer.byteLength;assert.ok(!base.error,base.error);for(let i=0;i<3;i++)assert.deepEqual(normal(),base);assert.equal(k.HEAPF64.buffer.byteLength,heap);
  }finally{await worker.terminate();}
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/fft-budget-'+(process.argv.includes('--admission-only')?'admission':selected.map(c=>c.model.toLowerCase()).join('-'))+'.json',JSON.stringify({budgetComparisons,nativeBudgetComparisons,greenTableBudgetBytes:128*1024*1024,overflowSafe:true,admissionOnly:process.argv.includes('--admission-only'),reports},null,2)+'\n');console.log(process.argv.includes('--admission-only')?'FFT allocation admission checks pass':`FFT budget ${selected.map(c=>c.model).join('/')}: original 512-grid references, exact worker results and recovery pass`);
