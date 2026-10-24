import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const kernel=await createKernel();configureKernel(kernel,parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')));
const keywords='NAME=A:LEN=512:WID=512:METAL=M1:XORG=0:YORG=0';createGeometry(kernel,2,keywords);const reports=[];
for(const grid of [[1,1],[8,16],[16,8]]) {
  const actual=JSON.parse(kernel.UTF8ToString(kernel._simulate_capacitance(0,-1,1e9,1000,1000,512,512,...grid,128,0)));
  const native=JSON.parse(execFileSync('build/native-geometry',['--cap',2,keywords,2,'',1e9,1000,1000,...grid,0].map(String),{encoding:'utf8'}));assert.ok(!actual.error,actual.error);assert.deepEqual(actual,native);
  reports.push({fft_x:grid[0],fft_y:grid[1],contacts:actual.contacts,green_table_hash:actual.green_table_hash});
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/capacitance-grid-parity.json',JSON.stringify({reference:'i386 recovered C++ with sufficient zeroed backing; original-executable nonsquare parity remains unproven',cases:reports},null,2)+'\n');
console.log('CAP FFT grids: valid one-cell boundary and both nonsquare indexing/scratch orientations match i386 exactly');
