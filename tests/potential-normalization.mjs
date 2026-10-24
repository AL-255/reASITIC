import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdirSync,writeFileSync} from 'node:fs';
import make from '../dist/asitic.js';
const k=await make(),reports=[],expected=[(700+18*2)/512**2,-18/512**2],half=[(736+8*3/Math.PI**2)/512**2,(-18-8*2/Math.PI**2)/512**2];
// A full-domain contact has zero average for every nonzero cosine mode.
// Two planar layers give Z0=700*(1+0i)+18*(2-i); divide by chip area.
for(const [nx,ny]of [[1,1],[8,16],[16,8],[512,1],[1,512],[32768,1],[65536,1],[1,65536],[131072,1],[1,131072],[2**20,1],[1,2**20]]){
  const actual=JSON.parse(k.UTF8ToString(k._probe_potential_contact(nx,ny))),native=JSON.parse(execFileSync('build/native-geometry',['--potential-contact',nx,ny].map(String),{encoding:'utf8'}));assert.ok(!actual.error,`${nx}x${ny}: ${actual.error}`);assert.deepEqual(actual.potential,expected,`${nx}x${ny}: analytic full-domain potential`);assert.deepEqual(actual,native,`${nx}x${ny}: native/WASM`);if(nx>1||ny>1)actual.half_domain_potential.forEach((v,i)=>assert.ok(Math.abs(v-half[i])<=Math.max(...half.map(Math.abs))*1e-15,`${nx}x${ny}: analytic first cosine mode`));else assert.equal(actual.half_domain_potential,null);reports.push({grid:[nx,ny],potential:actual.potential,halfDomainPotential:actual.half_domain_potential});
}
for(const [nx,ny]of [[0,1],[3,8],[2147483647,1],[2**30,2**30]])assert.ok(JSON.parse(k.UTF8ToString(k._probe_potential_contact(nx,ny))).error);
k._set_reference_numerics(1);assert.ok(JSON.parse(k.UTF8ToString(k._probe_potential_contact(65536,1))).error);k._set_reference_numerics(0);
const heap=k.HEAPF64.buffer.byteLength;for(let i=0;i<3;i++)assert.deepEqual(JSON.parse(k.UTF8ToString(k._probe_potential_contact(2**20,1))).potential,expected);assert.equal(k.HEAPF64.buffer.byteLength,heap,'Synthetic table workspace leaked');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/potential-normalization.json',JSON.stringify({analytic:'Full contact: Z0 / area; half-domain contact: (Z0+8*(3-2i)/pi^2)/area, with Z0=700*(1+0i)+18*(2-i)',exactNativeWasm:true,integerWrapRemoved:true,resourceRecovery:true,cases:reports},null,2)+'\n');console.log(`Potential normalization: ${reports.length} exact full-domain contacts, analytic first cosine modes within 1e-15, native/WASM bit parity through 2^20-by-1 and workspace recovery pass`);
