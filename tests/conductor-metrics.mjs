import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {parseCreatorCommand} from '../web/creators.mjs';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
import {conductorEstimatesCSV} from '../web/estimates.mjs';
const definitions=readFileSync('tests/fixtures/creator-coverage-cases.json','utf8'),extra=readFileSync('tests/fixtures/conductor-metric-extra-cases.json','utf8'),cases=[...JSON.parse(definitions).cases,...JSON.parse(extra).cases];
const fixture=JSON.parse(readFileSync('tests/fixtures/original-conductor-metrics.json','utf8')),hash=x=>createHash('sha256').update(x).digest('hex');
assert.equal(hash(definitions),fixture.case_definition_sha256);assert.equal(hash(extra),fixture.extra_case_definition_sha256);
const technologies=Object.fromEntries(Object.keys(fixture.technology_sha256).map(name=>{const text=readFileSync(`web/technology/${name}.tek`,'utf8');assert.equal(hash(text),fixture.technology_sha256[name]);return [name,{text,parsed:parseTechnology(text)}];}));
const ordered=shapes=>[...shapes].sort((a,b)=>a.name.localeCompare(b.name));
const bits=x=>{const b=Buffer.alloc(8);b.writeDoubleLE(x);return b.toString('hex');};
function physical(layout,count){const segments=values=>values.map(s=>({metal:s.metal,width:s.width,points:s.points,...(s.metal<count?{thickness:s.thickness}:{nx:s.nx,ny:s.ny}),...(s.branches?{branches:segments(s.branches)}:{})}));return ordered(layout.shapes.map(s=>({name:s.name,origin:s.origin,segments:segments(s.segments),...(s.hidden?{hidden:true}:{})})));}
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Conductor metric worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}
const k=await make(),reports=[];let fields=0,scalarSpills=0,shuntCases=0,maxIndependentError=0,maxOriginalError=0;
const analyze=hz=>JSON.parse(k.UTF8ToString(k._analyze(hz)));
try{
  assert.equal((await next()).ready,true);
  for(const c of cases){
    const original=fixture.cases.find(v=>v.name===c.name),technology=technologies[c.technology],creator=parseCreatorCommand(c.command,c.answers);
    configureKernel(k,technology.parsed);let layout=createGeometry(k,creator.id,creator.keywords,creator.answers);
    if(c.edits)layout=editGeometry(k,c.edits);
    assert.deepEqual(physical(layout,original.metal_count),ordered(original.shapes),c.name+' original physical input');
    let response=await send({technology:technology.text,creator});assert.ok(!response.error,response.error);
    if(c.edits){response=await send({action:'edit',commands:c.edits});assert.ok(!response.error,response.error);}
    if(c.edits?.includes('JOINSHUNT')){assert.ok(layout.shapes[0].segments.some(s=>s.branches?.length));shuntCases++;}
    for(const frequency of fixture.frequencies_ghz){
      const hz=k._normalize_value(Number(frequency),5,.1,0),actual=analyze(hz);assert.ok(!actual.error,c.name+': '+actual.error);
      k._set_reference_numerics(1);const historical=analyze(hz);k._set_reference_numerics(0);assert.ok(!historical.error,historical.error);
      const workerResult=await send({action:'analyze',frequency_ghz:Number(frequency)});assert.ok(!workerResult.error,workerResult.error);assert.deepEqual(workerResult.analysis,actual,c.name+' worker metrics');
      for(const observed of original.observations.filter(v=>v.frequency_hz===null||v.frequency_hz===hz)){
        const w=actual.windings.find(v=>v.name===observed.name);assert.ok(w,c.name+' missing '+observed.name);
        assert.equal(w[observed.key]===null,observed.value===null,c.name+' '+observed.key+' classification');
        const retained=historical.windings.find(v=>v.name===observed.name);
        assert.equal(retained[observed.key]===null,observed.value===null);
        if(observed.value!==null){
          assert.equal(bits(retained[observed.key]),observed.hex,c.name+' retained original '+observed.name+' '+observed.key);
          const error=Math.abs(w[observed.key]-observed.value)/Math.max(Math.abs(observed.value),1e-300);maxOriginalError=Math.max(maxOriginalError,error);
          if(['inductance_nh','quality_low_frequency'].includes(observed.key))assert.ok(error<=1e-12,c.name+' original mathematical accuracy '+observed.key+': '+error);
          else assert.equal(bits(w[observed.key]),observed.hex,c.name+' production original '+observed.key);
          const printed=original.printed[original.observations.indexOf(observed)].match(/ =\s*([-+\d.eE]+)/);assert.ok(printed);
          const reported=Number(observed.key==='inductance_nh'?w[observed.key].toPrecision(6):w[observed.key].toFixed(6));assert.equal(reported,Number.parseFloat(printed[1]),c.name+' original printed '+observed.key);
        }fields++;
      }
      assert.equal(original.observations.filter(v=>v.key==='quality_low_frequency'&&v.frequency_hz===hz).length,actual.windings.length,c.name+' native frequency normalization');
      const independent=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_conductor_metrics.py'],{input:JSON.stringify({layout,technology:technology.parsed,analysis:actual}),encoding:'utf8'}));assert.equal(independent.mpmath_version,'1.3.0');
      for(const w of actual.windings){
        const expected=independent.windings.find(v=>v.name===w.name);
        for(const key of ['metal_area_um2','resistance_dc','quality_low_frequency']){
          assert.equal(w[key]===null,expected[key]===null);
          if(w[key]!==null){const error=Math.abs(w[key]-expected[key])/Math.max(Math.abs(expected[key]),1e-300);assert.ok(error<=1e-14,c.name+' independent '+key+': '+error);maxIndependentError=Math.max(maxIndependentError,error);}
        }
        if(w.quality_low_frequency!==expected.quality_from_rounded_estimates)scalarSpills++;
      }
      const csv=conductorEstimatesCSV(actual).trim().split('\n');assert.equal(csv.length,actual.windings.length+1);assert.equal(csv[0],'"frequency_hz","name","inductance_nh","resistance_dc_ohm","resistance_hf_ohm","quality_low_frequency","metal_area_um2"');
      for(const [i,w] of actual.windings.entries())assert.equal(csv[i+1],`${hz},"${w.name}",${w.inductance_nh},${w.resistance_dc},${w.resistance_hf},${w.quality_low_frequency??''},${w.metal_area_um2}`);
    }
    assert.deepEqual(physical(editGeometry(k,'UNSELECT'),original.metal_count),ordered(original.shapes),c.name+' direct analysis mutated input');
    const after=await send({action:'edit',commands:'UNSELECT'});assert.ok(!after.error,after.error);assert.deepEqual(physical(after.layout,original.metal_count),ordered(original.shapes),c.name+' worker analysis mutated input');
    reports.push({name:c.name,windings:layout.shapes.length,frequencies:fixture.frequencies_ghz.length});console.log(c.name+': retained original Q/area/L/R words, independent production metrics and worker reports pass');
  }
  assert.ok(scalarSpills>0,'No case distinguishes retained intermediates from rounded L/R');assert.equal(shuntCases,2);
  configureKernel(k,technologies.CMOS.parsed);assert.ok(analyze(1e9).error);
  const wire=createGeometry(k,2,'NAME=A:LEN=40:WID=5:METAL=M1:XORG=100:YORG=100');
  const simulate=()=>JSON.parse(k.UTF8ToString(k._simulate(0,1e9,1000,1000,1000,1000,1024))),before=simulate();assert.ok(!before.error,before.error);
  for(const frequency of [0,-1,NaN,Infinity])assert.ok(analyze(frequency).error);
  const expected=analyze(1e9);assert.equal(expected.windings[0].metal_area_um2,200);assert.deepEqual(simulate(),before,'Estimates changed the following LMAT solve');
  for(let i=0;i<30;++i)analyze(1e9);const bytes=k.HEAPF64.buffer.byteLength;for(let i=0;i<60;++i)assert.deepEqual(analyze(1e9),expected);assert.equal(k.HEAPF64.buffer.byteLength,bytes,'Estimate heap grows');assert.deepEqual(physical(editGeometry(k,'UNSELECT'),5),physical(wire,5));
  for(const data of [{frequency_ghz:0},{frequency_ghz:Infinity},{frequency:NaN}])assert.ok((await send({action:'analyze',...data})).error);
  assert.ok(!(await send({action:'analyze',frequency_ghz:1})).error,'Worker did not recover');
  const zeroViaText=technologies.CMOS.text.replace(/(<via> 0[^]*?\br\s*=\s*)4\b/,(_,prefix)=>prefix+'0');
  const zeroViaTechnology=parseTechnology(zeroViaText);assert.equal(zeroViaTechnology.vias[0].resistance,0);
  configureKernel(k,zeroViaTechnology);createGeometry(k,14,'NAME=ZERO:NX=2:NY=2:VIA=0:XORG=100:YORG=100');
  const zeroVia=analyze(1e9);assert.ok(!zeroVia.error,zeroVia.error);assert.equal(zeroVia.windings[0].quality_low_frequency,null);assert.equal(zeroVia.windings[0].resistance_dc,0);
  const createdZero=await send({technology:zeroViaText,creator:{id:14,keywords:'NAME=ZERO:NX=2:NY=2:VIA=0:XORG=100:YORG=100'}});assert.ok(!createdZero.error,createdZero.error);
  const analyzedZero=await send({action:'analyze',frequency_ghz:1});assert.ok(!analyzedZero.error,analyzedZero.error);assert.deepEqual(analyzedZero.analysis,zeroVia);
  assert.throws(()=>conductorEstimatesCSV(null));const escaped=conductorEstimatesCSV({...expected,windings:[{...expected.windings[0],name:'a,"b',quality_low_frequency:null}]});assert.ok(escaped.includes('"a,""b"'));assert.ok(escaped.includes(',,'));
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/conductor-metrics.json',JSON.stringify({cases:reports,rawOriginalFields:fields,retainedIntermediateCases:scalarSpills,shuntCases,maxIndependentError,maxOriginalError,sourceUnchanged:true,workerParity:true,rejectionRecovery:true,heapStable:true},null,2)+'\n');
  console.log(`Conductor metrics: ${cases.length} original geometry cases, ${fields} raw original fields; ${scalarSpills} retained-intermediate witnesses; ${shuntCases} primary-chain shunt controls; independent DC/area/Q, worker, CSV and lifecycle checks pass`);
}finally{await worker.terminate();}
