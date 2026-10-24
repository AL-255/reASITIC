import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import {Worker} from 'node:worker_threads';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
const kernel=await createKernel(),text=readFileSync('web/technology/CMOS.tek','utf8'),technology=parseTechnology(text),fixture=JSON.parse(gunzipSync(readFileSync('tests/fixtures/original-potential-grid.json.gz'))),hash=x=>createHash('sha256').update(x).digest('hex');
assert.equal(hash(text),fixture.technology_sha256);assert.equal(fixture.original_cases[0].returncode,0);assert.ok(fixture.original_cases.slice(1).every(c=>c.returncode<0&&c.fresh_repeats===2));
const cases=[];
for(const grid of [[1,1],[1,8],[8,1],[2,16],[16,2],[4,8],[8,4],[8,16],[16,8],[8,8],[16,16]]){
  const [nx,ny]=grid;
  for(const [length,width,cell]of Math.min(nx,ny)===1?[[512,512,1000]]:Math.min(nx,ny)===2?[[128,128,1000]]:[[128,128,1000],[256,128,128],[256,256,128]]){
    const origin=length===512?0:128,contacts=[];
    for(let i=0;i<Math.max(1,width/cell);i++)for(let j=0;j<Math.max(1,length/cell);j++){
      const stepX=Math.min(length,cell),stepY=Math.min(width,cell),x=origin+j*stepX,y=origin+width-(i+1)*stepY;
      contacts.unshift([Math.floor(x*nx/512),Math.floor((x+stepX)*nx/512),Math.floor(y*ny/512),Math.floor((y+stepY)*ny/512)]);
    }
    cases.push({name:`${nx}x${ny}-${length}x${width}`,grid,cell,contacts,keywords:`NAME=A:LEN=${length}:WID=${width}:METAL=M1:XORG=${origin}:YORG=${origin}`});
  }
}
const reference=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_potential_grid.py'],{input:JSON.stringify({cases}),encoding:'utf8'}));assert.equal(reference.decimal_digits,80);
const norm=(a,b)=>{const av=a.flat(Infinity),bv=b.flat(Infinity),scale=bv.reduce((m,v)=>Math.max(m,Math.abs(v)),1e-300);assert.equal(av.length,bv.length);return av.reduce((m,v,i)=>Math.max(m,Math.abs(v-bv[i])/scale),0);};
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`,worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Grid worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const response=await next();assert.equal(response.id,id);assert.ok(!response.error,response.error);return response;}
const solve=(c,unfolded=0)=>JSON.parse(kernel.UTF8ToString(kernel._simulate_capacitance(0,-1,1e9,c.cell,c.cell,512,512,...c.grid,128,unfolded))),clean=({warnings,...rest})=>rest,reports=[];
try{
  assert.equal((await next()).ready,true);
  for(const c of cases){
    kernel._set_reference_numerics(0);kernel._set_potential_memo(0);configureKernel(kernel,technology);const before=createGeometry(kernel,2,c.keywords),actual=solve(c),expected=reference.cases.find(v=>v.name===c.name);assert.ok(!actual.error,c.name+': '+actual.error);assert.equal(actual.contacts,c.contacts.length);
    const potentialError=norm(actual.potential_matrix,expected.potential_matrix),matrixError=norm(actual.matrix,expected.matrix);
    // The oracle starts from original binary64 FFT data. Compare complex
    // norms: inverse-DCT roundoff can dominate a tiny dissipative component.
    assert.ok(potentialError<1e-13,c.name+' direct cosine potential '+potentialError);assert.ok(matrixError<1e-13,c.name+' independent inverse/folding '+matrixError);
    const angular=1e9*2*Math.PI,impedanceError=norm([actual.resistance,-1/(angular*actual.capacitance_f)],[expected.resistance,-1/(angular*expected.capacitance_f)]);assert.ok(impedanceError<1e-13,c.name+' original equivalent impedance '+impedanceError);
    const unfolded=solve(c,1);assert.deepEqual(unfolded.matrix,actual.matrix,c.name+' single-group unfolded solve');assert.deepEqual(clean(editGeometry(kernel,'UNSELECT')),clean(before),c.name+' changed source');
    kernel._set_potential_memo(1);assert.deepEqual(solve(c),actual,c.name+' memo write');assert.deepEqual(solve(c),actual,c.name+' memo read');const memo=JSON.parse(kernel.UTF8ToString(kernel._potential_memo_stats()));assert.equal(memo.writes,1);assert.equal(memo.hits,1);kernel._set_potential_memo(0);
    await send({technology:text,creator:{id:2,keywords:c.keywords}});const response=await send({action:'capacitance',frequency_ghz:1,mesh:{length:c.cell,width:c.cell},fft_x:c.grid[0],fft_y:c.grid[1],contact_limit:128,nativeInput:true});assert.deepEqual(response.capacitance,actual,c.name+' worker');
    {
      const native=JSON.parse(execFileSync('build/native-geometry',['--cap',2,c.keywords,2,'',1e9,c.cell,c.cell,...c.grid,0].map(String),{encoding:'utf8'}));assert.deepEqual(actual,native,c.name+' native production layout');
    }
    let networkVerified=false;
    if(c.contacts.length===2&&c.grid[0]!==c.grid[1]){
      const network=JSON.parse(kernel.UTF8ToString(kernel._simulate_network(0,-1,1e9,128,1000,1000,128,128,512,512,...c.grid,128,128,0,0,0,0,250000)));assert.ok(!network.error,network.error);assert.equal(network.groups,2);assert.equal(network.contacts,2);
      const inverse=expected.contact_admittance_matrix,folded=inverse.map((row,i)=>row.map((z,j)=>i===j?row.reduce((sum,value)=>sum.map((v,p)=>v+value[p]),[0,0]):z.map(v=>-v)));
      assert.ok(norm(network.capacitance_matrix,folded)<1e-13,c.name+' original cosine contacts through network folding');
      const circuit=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_two_port.py','--nodes-only'],{input:JSON.stringify(network),encoding:'utf8'}));assert.ok(norm(network.matrix,circuit.matrix)<1e-13,c.name+' independent nodal reduction');
      for(const key of ['first','second'])assert.ok(norm(network.pi_shunts[key],circuit.circuit_shunts[key])<1e-13,c.name+' independent common mode '+key);
      const native=JSON.parse(execFileSync('build/native-geometry',['--network',2,c.keywords,2,'',1e9,128,1000,1000,128,128,...c.grid,128,128,0,0,0,0,250000].map(String),{encoding:'utf8'}));assert.deepEqual(network.capacitance_matrix,native.capacitance_matrix,c.name+' native network capacitance');
      const response=await send({action:'network',model:'PI2',frequency_ghz:1,mesh:{length:128,width:1000,thickness:1000},contact_mesh:{length:128,width:128},fft_x:c.grid[0],fft_y:c.grid[1],contact_limit:128,filament_limit:128,eddy:{mode:0,layers:[]},nativeInput:true});assert.deepEqual(response.network,network,c.name+' network worker');assert.deepEqual(clean(editGeometry(kernel,'UNSELECT')),clean(before),c.name+' network changed source');networkVerified=true;
    }
    reports.push({name:c.name,contacts:actual.contacts,potentialNormError:potentialError,matrixNormError:matrixError,impedanceNormError:impedanceError,workerExact:true,memoExact:true,networkVerified});
  }
  // Historical nonsquare addressing is an explicit diagnostic, with bounded
  // zero backing. No original fault output is a production expected value.
  for(const grid of [[8,16],[16,8]]){
    const c={grid,cell:1000,keywords:'NAME=A:LEN=512:WID=512:METAL=M1:XORG=0:YORG=0'};configureKernel(kernel,technology);createGeometry(kernel,2,c.keywords);kernel._set_reference_numerics(1);const diagnostic=solve(c),native=JSON.parse(execFileSync('build/native-geometry',['--cap',2,c.keywords,2,'',1e9,1000,1000,...grid,0].map(String),{encoding:'utf8',env:{...process.env,REASITIC_REFERENCE_NUMERICS:'1'}}));assert.deepEqual(diagnostic,native);kernel._set_reference_numerics(0);assert.notEqual(solve(c).green_table_hash,diagnostic.green_table_hash,'Historical indexing leaked into production');
  }
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/capacitance-grid-parity.json',JSON.stringify({reference:'80-digit analytic cosine contact integrals from fresh original square-cache spectral coefficients; square native bit parity; rectangular original aborts retained only as diagnostics',originalSquareCache:reference.square_original_cache_sha256,decimalDigits:reference.decimal_digits,cases:reports},null,2)+'\n');
  console.log(`CAP FFT grids: ${reports.length} independent cosine-integral/inverse/folding checks, exact worker/cache round trips, native production parity, rectangular PI2 circuits and isolated historical rectangular diagnostics pass`);
}finally{kernel._set_reference_numerics(0);kernel._set_potential_memo(0);await worker.terminate();}
