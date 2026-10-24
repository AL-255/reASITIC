import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdirSync,writeFileSync} from 'node:fs';
import createKernel from '../dist/asitic.js';
const k=await createKernel(),cases=[];
const add=(operation,values,b=0,rule=0)=>values.forEach(a=>cases.push([operation,a,b,rule]));
add(0,[-0,0,1e-300,1e-100,.1,.5,1,2,1e100,1e300]);
add(1,[0,-1,1e-300,1e-100,.001,.1,.5,1-2**-52,1,1+2**-52,2,10,1e100,1e300]);
add(2,[-.292,-.1,-1e-100,-1e-20,-0,0,1e-20,1e-100,.1,.292]);
for(const operation of [4,5])add(operation,[-(2**63),-4*Math.PI,-2*Math.PI,-Math.PI,-Math.PI/2,-.1,-0,0,.1,Math.PI/2-2**-52,Math.PI/2,Math.PI/2+2**-52,Math.PI,2*Math.PI,4*Math.PI,2**63]);
add(6,[-750,-100,-10,-1,-1e-20,-0,0,1e-20,1,10,100,710]);
add(7,[-1,-.5,-1e-100,-1e-20,-0,0,1e-100,1e-20,.5,1]);
for(const a of [-Infinity,-1,-0,0,1,Infinity])for(const b of [-Infinity,-1,-0,0,1,Infinity])cases.push([3,a,b,0]);
let state=0x6a09e667;
const random=()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/2**32;};
for(let i=0;i<128;i++){
    cases.push([1,2**((random()-.5)*1000),1.01+random(),1]);
    cases.push([4,(random()-.5)*30,1.01+random(),1]);
    cases.push([5,(random()-.5)*30,1.01+random(),1]);
    cases.push([6,(random()-.5)*100,1.01+random(),3]);
}
const gap=Math.fround(20*Math.PI/180)*180/Math.PI;for(let side=0;side<15;side++)for(const op of [20,21,24])cases.push([op,gap,side,16]);
let extendedDifferences=0;const differences=[];
for(const [i,args]of cases.entries()){
    const native=spawnSync('build/native-geometry',['--math',...args].map(v=>Object.is(v,-0)?'-0':String(v)),{encoding:'utf8'});assert.equal(native.status,0,native.stderr);
    const expected=JSON.parse(native.stdout),actual=JSON.parse(k.UTF8ToString(k._probe_math(...args)));
    assert.equal(Boolean(actual.error),Boolean(expected.error),`Nonfinite math classification ${i}`);
    if(expected.error)continue;
    if(!Object.is(actual.value,expected.value))differences.push({input:args,native:expected.value,wasm:actual.value});
    if(actual.low!==expected.low || actual.high!==expected.high || actual.exponent!==expected.exponent)extendedDifferences++;
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/retained-math.json',JSON.stringify({cases:cases.length,binary64Differences:differences.length,extendedDifferences,differences},null,2)+'\n');
console.log(`Hardware x87: ${cases.length} instruction/arithmetic cases; ${differences.length} consumed binary64 differences, ${extendedDifferences} raw binary80 differences`);
assert.equal(differences.length,0,JSON.stringify(differences.slice(0,4)));
