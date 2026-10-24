import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import createKernel from './reference-kernel.mjs';
import {twoPortRecord,twoPortScatteringCSV} from '../web/network.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const fixture=JSON.parse(readFileSync('tests/fixtures/original-two-port.json','utf8'));
const hash=path=>createHash('sha256').update(readFileSync(path)).digest('hex');
assert.equal(hash('web/technology/CMOS.tek'),fixture.technology_sha256);
for(const [family,digest] of Object.entries(fixture.case_definition_sha256))assert.equal(hash(`tests/fixtures/${family}-cases.json`),digest);
const kernel=await createKernel(),memory=kernel._malloc(17*8),failures=[];let records=0,rawFields=0,nonfinite=0,integrated=0;
const doubles=hex=>{const b=Buffer.from(hex,'hex');return Array.from({length:b.length/8},(_,i)=>b.readDoubleLE(i*8));};
const bits=value=>{const b=Buffer.alloc(8);b.writeDoubleLE(value);return b.toString('hex');};
function probe(y,bank,transformer,frequency){kernel.HEAPF64.set([...y,...bank],memory/8);return JSON.parse(kernel.UTF8ToString(kernel._probe_two_port_output(memory,memory+64,transformer?1:0,frequency)));}
try{
  for(const c of fixture.cases){
    for(const [i,input] of c.inputs.entries()){
      const y=doubles(input.y_hex),bank=doubles(input.model_hex),transformer=c.family==='transformer';
      const actual=probe(y,bank,transformer,input.frequency_hz);
      const native=JSON.parse(execFileSync('build/native-geometry',['--output',input.frequency_hz,transformer?1:0,...y,...bank].map(String),{encoding:'utf8'}));
      try{
        assert.deepEqual(actual,native,c.name+' native software translation');
        if(y.some(v=>!Number.isFinite(v))){assert.match(actual.error,/nonfinite admittance/);nonfinite++;continue;}
        const parameter=input.parameter===1?'s':input.parameter===2?'y':transformer?'trans':'pi';
        const key=(i%3===2?'model':parameter)+(input.polar?'-polar':'');
        const rows=input.polar?actual.polar_records:actual.records,headers=input.polar?actual.polar_headers:actual.headers;
        assert.equal(rows[parameter],c.records[key],c.name+' original '+key+' row');
        assert.equal(headers[parameter],c.headers[key],c.name+' original '+key+' header');
        assert.equal(twoPortRecord({two_port:actual},parameter,Boolean(input.polar)),c.headers[key]+c.records[key]+'\n');records++;
        if(i===0){const expected=doubles(c.scattering_hex);actual.scattering_matrix.flat(2).forEach((v,j)=>{assert.equal(bits(v),bits(expected[j]),c.name+' original S binary64 '+j);rawFields++;});}
      }catch(error){failures.push({name:c.name,mode:i,error:error.message});}
    }
  }
  const technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));
  for(const c of fixture.cases){
    const v=c.case;configureKernel(kernel,technology);
    const creators=c.family==='transformer'?[v.primary,v.secondary]:c.family==='pad'?[v.device,...v.pads]:[{id:v.id,keywords:v.keywords}];
    let layout;for(const creator of creators.filter(Boolean))layout=createGeometry(kernel,creator.id,creator.keywords,'',{append:Boolean(layout)});
    if(v.ground&&!v.ground.startsWith('@'))layout=createGeometry(kernel,2,v.ground,'',{append:true});
    const index=name=>layout.shapes.findIndex(s=>s.name===name),a=index('A'),b=index('B'),p1=index('P1'),p2=index('P2');
    const ground=v.ground?v.ground.startsWith('@')?v.ground==='@1'?(c.family==='transformer'?a:p1):v.ground==='@2'?(c.family==='transformer'?b:p2):a:index('G'):c.family==='network'?a:-1;
    const prefix=c.family==='transformer'?[a,b,ground]:c.family==='pad'?[a,p1,p2,ground]:[a,ground];
    const args=[...prefix,kernel._normalize_value(v.frequency_ghz,5,.1,0),...[...v.cells,...v.contact_cells].map(value=>kernel._normalize_value(value,7,.1,1)),512,512,...v.grid,512,1024,v.eddy_mode,v.layers.length,v.layers[0]??0,v.layers[1]??0,...(c.family==='transformer'?[kernel._normalize_value(v.termination,4,.1,0)]:[]),250000];
    const solver=c.family==='transformer'?kernel._simulate_transformer:c.family==='pad'?kernel._simulate_pad_network:c.family==='extended'?kernel._simulate_extended_network:kernel._simulate_network;
    kernel._set_reference_numerics(1);const result=JSON.parse(kernel.UTF8ToString(solver(...args)));kernel._set_reference_numerics(0);
    if(doubles(c.inputs[0].y_hex).some(v=>!Number.isFinite(v))){assert.match(result.error,/finite/);continue;}
    assert.ok(!result.error,c.name+': '+result.error);
    for(const [key,record] of Object.entries(c.records)){
      const parameter=key.startsWith('model')?(c.family==='transformer'?'trans':'pi'):key[0],polar=key.endsWith('-polar');
      assert.equal((polar?result.two_port.polar_records:result.two_port.records)[parameter],record,c.name+' complete solve to original '+key);
    }
    const stable=JSON.parse(kernel.UTF8ToString(solver(...args)));assert.ok(!stable.error,stable.error);
    stable.two_port.scattering_matrix.flat(2).forEach((value,i)=>assert.equal(value.toPrecision(8),result.two_port.scattering_matrix.flat(2)[i].toPrecision(8),c.name+' production S precision'));
    integrated++;
  }
  // Isolate conversion from solver/history: distinct off-diagonals, singular
  // systems, signed zero, extreme values, and randomized complex matrices.
  kernel._set_reference_numerics(1);
  let randomState=0x53504152;const random=()=>{randomState=(Math.imul(randomState,1664525)+1013904223)>>>0;return randomState/2**32;};
  for(let i=0;i<128;++i){const y=i===0?[0,0,0,0,0,0,0,0]:i===1?[-.02,-0,0,-0,0,0,-.02,0]:Array.from({length:8},()=>((random()-.5)*2)*2**Math.floor((random()-.5)*300)),bank=Array.from({length:9},(_,j)=>j%2?-j:j),actual=probe(y,bank,i%2,1e9),native=JSON.parse(execFileSync('build/native-geometry',['--output',1e9,i%2,...y,...bank].map(v=>Object.is(v,-0)?'-0':String(v)),{encoding:'utf8'}));assert.deepEqual(actual,native,'Isolated output case '+i);}
  const csv=twoPortScatteringCSV({frequency_hz:1e9,two_port:{reference_ohm:50,scattering_matrix:[[[1,-0],[2,3]],[[4,5],[null,6]]]}});
  assert.match(csv,/1000000000,0,0,50,1,-0\n/);assert.match(csv,/1000000000,1,1,50,,6\n/);assert.equal(csv.trim().split('\n').length,5);
  assert.throws(()=>twoPortRecord(null),/Solve/);assert.throws(()=>twoPortRecord({two_port:{records:{}}},'z'),/Solve/);assert.throws(()=>twoPortScatteringCSV(null),/Run/);
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/two-port-output.json',JSON.stringify({cases:fixture.cases.length,integrated,records,rawFields,nonfinite,isolatedNativeCases:128,failures},null,2)+'\n');
  assert.equal(failures.length,0,JSON.stringify(failures));
  console.log(`Two-port output: ${records} original numerical rows and headers, ${rawFields} original S fields by binary64 bits, ${integrated} complete solves, 128 native edge/random cases, and exact exports pass`);
}finally{kernel._free(memory);}
