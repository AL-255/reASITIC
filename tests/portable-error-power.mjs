import assert from 'node:assert/strict';import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';import {execFileSync} from 'node:child_process';import make from '../build/portable-math/gmd.mjs';
const k=await make(),cases=JSON.parse(readFileSync('tests/fixtures/portable-error-power.json','utf8')).cases,bits=v=>{const b=Buffer.alloc(8);b.writeDoubleLE(v);return b.toString('hex');},failures=[];
for(const [i,c]of cases.entries()){
 const actual=JSON.parse(k.UTF8ToString(k._probe_error_power(c.input))),native=JSON.parse(execFileSync('build/portable-math/gmd-native',['--power',String(c.input)],{encoding:'utf8'}));
 try{if(c.overflow){assert.ok(actual.error);assert.ok(native.error);}else{assert.ok(!actual.error);assert.ok(!native.error);assert.equal(bits(actual.value),bits(c.value),'WASM/reference '+i);assert.equal(bits(native.value),bits(c.value),'native/reference '+i);}}
 catch(e){failures.push({i,input:c.input,error:e.message,actual,native,reference:c.value});}
}
for(const x of [-1,NaN,-Infinity])assert.ok(JSON.parse(k.UTF8ToString(k._probe_error_power(x))).error);
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-error-power.json',JSON.stringify({cases:cases.length,failures},null,2)+'\n');console.log(`Portable error estimator: ${cases.length} high-precision references, ${failures.length} failures; native and WASM bit-identical`);assert.equal(failures.length,0,JSON.stringify(failures.slice(0,4)));
