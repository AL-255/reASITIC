import assert from 'node:assert/strict';import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';import {execFileSync} from 'node:child_process';import createKernel from '../build/portable-math/gmd.mjs';
const k=await createKernel(),fixture=JSON.parse(readFileSync('tests/fixtures/portable-gmd.json','utf8')),reports=[],failures=[];
const bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');};let adaptive=0;
for(const c of fixture.cases){
 const start=performance.now(),actual=JSON.parse(k.UTF8ToString(k._probe_gmd(...c.input))),native=JSON.parse(execFileSync('build/portable-math/gmd-native',c.input.map(String),{encoding:'utf8'}));
 try{assert.ok(!actual.error,c.name+': '+actual.error);assert.ok(!native.error,c.name+': '+native.error);assert.equal(bits(actual.distance),bits(c.distance),c.name+' high-precision integral');assert.equal(bits(actual.distance),bits(native.distance),c.name+' portable native/WASM');if(actual.precision_bits>128)adaptive++;reports.push({name:c.name,precision:actual.precision_bits,cancellation_bits:actual.cancellation_bits,milliseconds:performance.now()-start});}
 catch(e){failures.push({name:c.name,error:e.message,actual,native,reference:c.distance});console.log(c.name+': '+e.message);}
}
for(const values of [[0,1,1,1,0,0],[1,-1,1,1,0,0],[1,1,NaN,1,0,0],[1,1,1,1,Infinity,0]])assert.ok(JSON.parse(k.UTF8ToString(k._probe_gmd(...values))).error);
const edge=fixture.cases.find(c=>c.name==='thin-self-1e-300');const expected=JSON.parse(k.UTF8ToString(k._probe_gmd(...edge.input)));const size=k.HEAPF64.buffer.byteLength;
for(let i=0;i<20;i++)assert.deepEqual(JSON.parse(k.UTF8ToString(k._probe_gmd(...edge.input))),expected);assert.equal(k.HEAPF64.buffer.byteLength,size,'Adaptive math leaked or grew memory after warmup');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-gmd.json',JSON.stringify({cases:fixture.cases.length,adaptive,failures,reports},null,2)+'\n');console.log(`Portable geometry integral: ${fixture.cases.length} high-precision references, ${adaptive} adaptive cases, ${failures.length} failures; native/WASM compare by binary64 bits`);assert.equal(failures.length,0,JSON.stringify(failures.slice(0,5)));
