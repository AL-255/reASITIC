import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import make from '../build/portable-math/gmd.mjs';
const k=await make(),python=process.env.MPMATH_PYTHON??'python3';
mkdirSync('build/portable-two-port',{recursive:true});
const columnMajor=m=>m[0].flatMap((_,j)=>m.flatMap(row=>row[j]));
function probe(cap,z,n){const p=k._malloc((cap.length+z.length)*8);try{k.HEAPF64.set([...cap,...z],p/8);return JSON.parse(k.UTF8ToString(k._probe_extended_ports(p,p+cap.length*8,n)));}finally{k._free(p);}}
function host(cap,z,n){const values=[...cap,...z],bytes=Buffer.alloc(8+values.length*8);bytes.writeInt32LE(n);values.forEach((v,i)=>bytes.writeDoubleLE(v,8+i*8));writeFileSync('build/portable-two-port/extended-ports.bin',bytes);return JSON.parse(execFileSync('build/portable-math/gmd-native',['--extended-ports','build/portable-two-port/extended-ports.bin'],{encoding:'utf8'}));}
const single=probe([1/16,1/8],[1,0],1);
assert.deepEqual(single,{matrix:[[[65/64,1/32],[-63/64,1/32]],[[-63/64,1/32],[65/64,1/32]]],currents:[[[65/64,1/32]],[[63/64,-1/32]]],voltages:[[[1,0]],[[0,0]]]});
assert.deepEqual(single,host([1/16,1/8],[1,0],1));
const zeroCap=probe(Array(8).fill(0),[1,0,0,0,0,0,1,0],2);
assert.deepEqual(zeroCap,{matrix:[[[.5,0],[-.5,0]],[[-.5,0],[.5,0]]],currents:[[[.5,0]],[[.5,0]],[[.5,0]]],voltages:[[[1,0]],[[.5,0]],[[0,0]]]});
assert.deepEqual(zeroCap,host(Array(8).fill(0),[1,0,0,0,0,0,1,0],2));
const circuits=[
  {cap:[[[1/16,1/8],[0,0]],[[0,0],[1/16,1/8]]],z:[[[1,0],[0,0]],[[0,0],[1,0]]]},
  {cap:[[[1/16,1/8],[-1/128,-1/64]], [[-1/128,-1/64],[1/8,1/4]]],z:[[[1,1/4],[1/8,1/16]],[[1/8,1/16],[2,1/2]]]},
  {cap:[[[1/16,1/8],[-1/128,-1/64],[-1/256,-1/128]],[[-1/128,-1/64],[1/8,1/4],[-1/64,-1/128]],[[-1/256,-1/128],[-1/64,-1/128],[1/4,1/2]]],z:[[[1,1/4],[1/8,1/16],[1/32,1/64]],[[1/8,1/16],[2,1/2],[1/16,1/32]],[[1/32,1/64],[1/16,1/32],[3,1]]]}
];
let checks=2,maxError=0;
for(const [index,c] of circuits.entries()){
  const base=probe(columnMajor(c.cap),columnMajor(c.z),c.cap.length);assert.ok(!base.error,base.error);
  for(const exponent of [-800,-400,0,400,800]){
    // Change impedance units while preserving Z*C, the dimensionless circuit.
    // Voltages must be invariant, currents and Y scale by the inverse factor.
    const scale=2**exponent,cap=c.cap.map(row=>row.map(z=>z.map(v=>v/scale))),z=c.z.map(row=>row.map(z=>z.map(v=>v*scale)));
    const actual=probe(columnMajor(cap),columnMajor(z),cap.length);assert.deepEqual(actual,host(columnMajor(cap),columnMajor(z),cap.length));
    assert.deepEqual(JSON.parse(execFileSync('build/portable-math/gmd-native',['--extended-rounding','build/portable-two-port/extended-ports.bin'],{encoding:'utf8'})),{rounding_modes:4,identical:true});
    assert.deepEqual(actual.voltages,base.voltages,'Impedance scaling changed voltage states');
    for(const key of ['matrix','currents'])assert.deepEqual(actual[key].map(row=>row.map(z=>z.map(v=>v*scale))),base[key],'Impedance scaling changed normalized '+key);
    const input={capacitance_matrix:cap,inductance_impedance_matrix:z,matrix:actual.matrix,frequency_hz:1,reference_decimal_digits:400};
    const expected=JSON.parse(execFileSync(python,['tools/reference_extended.py','--nodes-only'],{input:JSON.stringify(input),encoding:'utf8'}));
    for(const key of ['matrix','currents','voltages']){
      const a=actual[key].flat(2),b=expected[key].flat(2),norm=Math.max(...b.map(Math.abs));
      const error=Math.max(...a.map((v,i)=>Math.abs(v-b[i])))/norm;maxError=Math.max(maxError,error);assert.ok(error<=2e-15,`${index} ${exponent} ${key}: ${error}`);
    }
    checks++;
  }
}
for(const [cap,z,n] of [[[1,0],[0,0],1],[[NaN,0],[1,0],1],[[1,0],[Infinity,0],1],[[1,0],[1,0],0],[[1,0],[1,0],1025],[Array(8).fill(0),Array(8).fill(0),2]])assert.ok(probe(cap,z,n).error);
assert.deepEqual(probe([1/16,1/8],[1,0],1),single,'Failed solve contaminated the next request');
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/portable-extended-ports.json',JSON.stringify({reference:'Exact one-midpoint/zero-capacitance circuits and independent 400-digit current elimination',checks,impedanceExponents:[-800,-400,0,400,800],maxError,nativeWasmBitsIdentical:true,hostRoundingModes:4,rejectionRecovery:true},null,2)+'\n');
console.log(`Reduced PIX: ${checks} circuits, 1600-bit impedance scaling, native/WASM bit identity, four host rounding modes, independent node states, rejection and recovery pass; maximum norm error ${maxError}`);
