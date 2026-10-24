import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';import {mkdirSync,writeFileSync} from 'node:fs';import createKernel from '../dist/asitic.js';
const kernel=await createKernel(),input=[1,0x99f3f5ef,0x9061ddd1,0x3fee];
const actual=JSON.parse(kernel.UTF8ToString(kernel._probe_retained_math(...input))),native=JSON.parse(execFileSync('build/native-geometry',['--x87-raw',...input].map(String),{encoding:'utf8'}));
assert.equal(actual.value,native.value,'The direct binary64 log store agrees; cancellation later exposes retained precision');
const gap=actual.low!==native.low||actual.high!==native.high||actual.exponent!==native.exponent;
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/pad-rounding-instruction.json',JSON.stringify({reference:'Retained input from divergent PI4 mutual-inductance pair; actual x87 FLDLN2/FYL2X vs software binary80',input,actual,native,gap},null,2)+'\n');
console.log(`PI4 logarithm diagnostic: binary64 log store agrees; ${gap?1:0} UNRESOLVED retained binary80 difference`);if(process.argv.includes('--strict'))assert.equal(gap,false,'Full-goal retained x87 identity is unresolved');
