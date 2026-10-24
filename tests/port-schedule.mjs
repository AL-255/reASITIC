import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdirSync,writeFileSync} from 'node:fs';
import make from '../dist/asitic.js';
const kernel=await make(),schedule=(...args)=>JSON.parse(kernel.UTF8ToString(kernel._port_frequency_schedule(...args)));
const epsilon=Number.EPSILON;
assert.deepEqual(schedule(1,1+epsilon,epsilon*1.25,101).frequencies_hz,[1],'Compare the unrounded extended sum, even when its double rounds back to the stop value');
assert.deepEqual(schedule(1,1+epsilon,epsilon*.75,101).frequencies_hz,[1,1+epsilon]);
assert.deepEqual(schedule(1,1,.5,101).frequencies_hz,[1]);
assert.deepEqual(schedule(1,2,.25,101).frequencies_hz,[1,1.25,1.5,1.75,2]);
assert.match(schedule(1,2,.01,10).error,/limit/);
assert.match(schedule(1e300,1e300,1,101).error,/advance/);
for(const args of [[0,1,.1,101],[2,1,.1,101],[1,2,0,101],[1,2,-1,101],[1,Infinity,1,101],[NaN,2,1,101],[1,2,1,0],[1,2,1,102]])assert.ok(schedule(...args).error);
let state=0x46524551;const random=()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/2**32;};
const cases=[[1,1+epsilon,epsilon*1.25,101],[1e300,1e300,1,101],[1,2,.01,10]];
for(let i=0;i<256;i++){const first=2**Math.floor(random()*1800-900),step=first*2**Math.floor(random()*70-60),last=first+step*Math.floor(random()*120);if(Number.isFinite(last)&&Number.isFinite(step)&&step>0)cases.push([first,last,step,101]);}
for(const args of cases){const native=JSON.parse(execFileSync('build/native-geometry',['--schedule',...args].map(String),{encoding:'utf8'}));assert.deepEqual(schedule(...args),native,'Native/WASM saved frequency and comparison '+args);}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/port-schedule.json',JSON.stringify({nativeCases:cases.length,unroundedStopComparison:true,stalledStepRejection:true,budgetBeforeSolve:true},null,2)+'\n');
console.log(`Native port schedule: ${cases.length} native/WASM cases, unrounded stop boundary, saved-double recurrence, stalled steps and sample budgets pass`);
