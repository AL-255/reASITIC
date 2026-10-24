import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {parseTechnology} from '../web/technology.mjs';
import {writeGDS,gdsSize,polygons,GDS_BYTE_BUDGET} from '../web/gds.mjs';
const hash=x=>createHash('sha256').update(x).digest('hex'),proof=JSON.parse(readFileSync('tests/fixtures/gds-compatibility.json')),reports=[];
for(const f of proof.fixtures){
  const raw=readFileSync(f.path);assert.equal(hash(raw),f.sha256,'Reference geometry changed');const source=JSON.parse(raw);
  for(const c of f.cases){
    const text=readFileSync(`web/technology/${c.technology}.tek`,'utf8');assert.equal(hash(text),c.technology_sha256);
    const technology=parseTechnology(text),layout={shapes:source.cases.find(v=>v.name===c.name).shapes},size=gdsSize(layout,technology),bytes=writeGDS(layout,technology);
    assert.equal(bytes.length,c.bytes,c.name+' byte count');assert.equal(hash(bytes),c.sha256,c.name+' former-encoder bytes');assert.equal(size.bytes,bytes.length);
    assert.equal(size.polygons,(bytes.length-110)/64,c.name+' preflight boundary count');
    reports.push({name:c.name,bytes:bytes.length,polygons:size.polygons});
  }
}
const fixture=JSON.parse(readFileSync('tests/fixtures/original-creator-counts.json')).cases.find(c=>c.name==='cmos-via-matrix'),layout={shapes:structuredClone(fixture.shapes)},technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));
layout.shapes[0].segments[0].nx=1000000;layout.shapes[0].segments[0].ny=1000000;
assert.throws(()=>gdsSize(layout,technology),/output budget/);assert.throws(()=>writeGDS(layout,technology),/output budget/);assert.throws(()=>polygons(layout,technology).next(),/output budget/);
const original={shapes:fixture.shapes},size=gdsSize(original,technology),small=writeGDS(original,technology,{maxBytes:size.bytes});
assert.equal(small.length,size.bytes);assert.throws(()=>writeGDS(original,technology,{maxBytes:size.bytes-1}),/output budget/);
assert.throws(()=>gdsSize(original,technology,{maxBytes:Infinity}),/budget/);assert.throws(()=>gdsSize(original,technology,{maxBytes:109}),/budget/);
const shifted=structuredClone(original);shifted.shapes[0].origin[0]=3e6;assert.throws(()=>writeGDS(shifted,technology),/signed 32-bit/);
assert.equal(hash(writeGDS(original,technology)),hash(small),'Failed export contaminated next output');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/gds-stream.json',JSON.stringify({referenceEncoderCommit:proof.encoderCommit,referenceEncoderSourceSha256:proof.encoderSourceSha256,defaultByteBudget:GDS_BYTE_BUDGET,oversizePreflight:true,explicitBudget:true,recovery:true,reports},null,2)+'\n');
console.log(`GDS stream: ${reports.length} byte-identical former-encoder libraries, exact output preflight, oversized-grid rejection, explicit budgets, coordinate guards and recovery pass`);
