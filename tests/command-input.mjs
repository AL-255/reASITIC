import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
const kernel=await createKernel();let count=0;
for(const type of [4,5,7,10])for(const value of [-720,-360,-37,-.05,-0,.05,.15,.25,1.23456789,37,360,721])for(const snap of [0,1]){
    const native=spawnSync('build/native-geometry',['--input',value,type,.1,snap].map(v=>Object.is(v,-0)?'-0':String(v)),{encoding:'utf8'});assert.equal(native.status,0,native.stderr);
    const actual=kernel._normalize_value(value,type,.1,snap),expected=Number(native.stdout.trim());assert.ok(Object.is(actual,expected),`${type}/${value}/${snap}: ${actual} vs ${expected}`);count++;
}
assert.equal(kernel._normalize_value(-37,10,.1,0),Math.fround(-397*0.017453292519943295),'Native negative-angle behavior changed');
configureKernel(kernel,parseTechnology(readFileSync('web/technology/CMOS.tek','utf8')));
const raw=[80,80,10,2,1,50,8,4,-1,37,15,1],normalized=[...raw];normalized[9]=kernel._normalize_value(37,10,.1,1);
const direct=generateGeometry(kernel,2,normalized,{x:0,y:0,phase:-1});
const nativeInput=generateGeometry(kernel,2,raw,{nativeInput:true,x:0,y:0,phase:-1,snap:true,grid:.1});assert.deepEqual(nativeInput,direct);
console.log(`Recovered native parser: ${count} exact scalar/frequency/grid/angle comparisons, including negative-angle quirks and public phase/angle generation`);
