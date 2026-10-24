import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const kernel=await createKernel();configureKernel(kernel,parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')));
const before=createGeometry(kernel,2,'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100');
const base=[0,-1,1e9,1000,1000,1000,1000,1000,512,512,8,8,512,1024,0,2,0,1,250000];
const solve=changes=>{const args=[...base];for(const [i,v]of Object.entries(changes??{}))args[i]=v;return JSON.parse(kernel.UTF8ToString(kernel._simulate_network(...args)));};
const expected=solve();assert.ok(!expected.error,expected.error);assert.equal(expected.pi.resistance,0.1625);assert.equal(expected.pi.shunt_differential,null);
for(const changes of [{0:99},{1:99},{2:0},{3:1e-9},{4:1e-300},{5:1e-300},{10:7},{12:0},{13:0},{13:1,4:1},{14:3},{16:99},{14:1,18:1}]){assert.ok(solve(changes).error,JSON.stringify(changes));assert.deepEqual(solve(),expected);}
const explicit=solve({1:0});assert.ok(!explicit.error,explicit.error);assert.equal(explicit.filaments,9,'Explicit native type-8 ground must retain -1/-1 mesh override');assert.deepEqual(solve(),expected,'Explicit self-ground mutated source mesh history');
const eddy=solve({14:1});assert.ok(!eddy.error,eddy.error);assert.ok(eddy.eddy.evaluations>0);
const heap=kernel.HEAPF64.buffer.byteLength;for(let i=0;i<25;++i){assert.ok(solve({14:1,18:1}).error);assert.deepEqual(solve(),expected);}assert.equal(kernel.HEAPF64.buffer.byteLength,heap,'Repeated network solves grew the heap');
const cap=JSON.parse(kernel.UTF8ToString(kernel._simulate_capacitance(0,-1,1e9,1000,1000,512,512,8,8,128,0)));assert.ok(!cap.error,cap.error);assert.deepEqual(solve(),expected,'Shared CAP/network result storage or state contaminated the next solve');
const after=createGeometry(kernel,2,'NAME=B:LEN=100:WID=40:METAL=M2:XORG=100:YORG=100','',{append:true});assert.deepEqual(after.shapes[1],before.shapes[0]);
console.log('Network lifecycle: admission budgets, explicit self-ground history, eddy recovery, shared CAP state, immutable geometry and 25 bounded-heap repeats pass');
