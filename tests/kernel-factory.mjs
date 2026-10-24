import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createServer} from 'node:http';
import {createKernelFactory} from '../dist/kernel.mjs';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
const binary=readFileSync('dist/asitic.wasm'),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));
let requests=0,mode='ok';const server=createServer((req,res)=>{requests++;if(mode==='missing'){res.writeHead(404);res.end();return;}res.writeHead(200,{'content-type':'application/wasm'});res.end(mode==='corrupt'?Buffer.from('invalid WASM'):binary);});
try{
  await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});const url=new URL(`http://127.0.0.1:${server.address().port}/releases/test/asitic.wasm`),make=createKernelFactory(url);
  const kernels=await Promise.all([make(),make(),make()]);assert.equal(requests,1,'Concurrent instance creation downloaded the same code repeatedly');assert.equal(new Set(kernels.map(k=>k.HEAPF64.buffer)).size,3,'Instances share mutable memory');
  for(const k of kernels)configureKernel(k,technology);
  const before=createGeometry(kernels[0],2,'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100');assert.equal(kernels[1]._shape_kind(0),-1);createGeometry(kernels[1],2,'NAME=B:LEN=200:WID=40:METAL=M2:XORG=100:YORG=100');assert.equal(kernels[2]._shape_kind(0),-1);
  const secondHeap=kernels[1].HEAPF64.buffer,secondBytes=secondHeap.byteLength,pointer=kernels[0]._malloc(24*1024*1024);assert.ok(pointer);assert.equal(kernels[1].HEAPF64.buffer,secondHeap);assert.equal(kernels[1].HEAPF64.buffer.byteLength,secondBytes);kernels[0]._free(pointer);
  const clean=({warnings,...rest})=>rest;assert.deepEqual(clean(editGeometry(kernels[0],'UNSELECT')),clean(before),'Another instance changed the first layout');
  const solve=k=>JSON.parse(k.UTF8ToString(k._simulate_capacitance(0,-1,1e9,1000,1000,512,512,8,8,128,0))),cap=solve(kernels[0]);assert.equal(cap.capacitance_f,3.168713072636854e-13);assert.equal(cap.green_table_hash,'d9ec835ccc4a4f7e');
  kernels[0]._set_reference_numerics(1);const other=JSON.parse(kernels[1].UTF8ToString(kernels[1]._simulate_network(0,-1,1e9,1000,1000,1000,1000,1000,512,512,8,8,128,128,0,0,0,0,250000)));assert.ok(!other.error,other.error);assert.equal(other.pi_shunts.precision_bits,128,'Diagnostic arithmetic leaked between instances');kernels[0]._set_reference_numerics(0);
  mode='missing';const after=await make();assert.equal(requests,1,'A later instance fetched a removed release');assert.equal(after._shape_kind(0),-1);configureKernel(after,technology);createGeometry(after,2,'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100');assert.deepEqual(solve(after),cap);
  const retry=createKernelFactory(url);await assert.rejects(retry(),/Unable to load/);assert.equal(requests,2);mode='ok';const recovered=await retry();assert.equal(requests,3);assert.equal(recovered._shape_kind(0),-1);mode='missing';await retry();assert.equal(requests,3,'Recovered code was not retained');
  const corrupt=createKernelFactory(url);mode='corrupt';await assert.rejects(corrupt(),WebAssembly.CompileError);assert.equal(requests,4);mode='ok';await corrupt();assert.equal(requests,5,'A failed compile poisoned the factory permanently');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/kernel-factory.json',JSON.stringify({oneDownloadForConcurrentInstances:true,independentHeaps:true,independentGeometry:true,independentMemoryGrowth:true,independentArithmeticMode:true,removedReleaseStillUsable:true,retryAfterLoadAndCompileFailure:true,originalCapacitance:cap.capacitance_f,originalGreenHash:cap.green_table_hash},null,2)+'\n');console.log('Kernel factory: one code download, concurrent isolated instances, independent memory growth/layouts, original CAP, removed-release reuse and retry after load/compile failure pass');
}finally{server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
