import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {Worker} from 'node:worker_threads';
import {execFileSync} from 'node:child_process';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry,editGeometry} from '../web/technology.mjs';
import {parseCreatorCommand} from '../web/creators.mjs';
import {networkQualitySamples} from '../web/network.mjs';
import {plotSeries} from '../web/simulation.mjs';
import {portSweepMetricsCSV} from '../web/port-sweep.mjs';
const bytes=readFileSync('tests/fixtures/quality-sweep-cases.json'),definition=JSON.parse(bytes),original=JSON.parse(readFileSync('tests/fixtures/original-quality-sweep.json','utf8')),sha=x=>createHash('sha256').update(x).digest('hex');
assert.equal(sha(bytes),original.case_definition_sha256);
const k=await make(),python=process.env.MPMATH_PYTHON??'python3',reports=[];
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Q sweep worker timeout')),60000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0;async function send(data,allowError=false){worker.postMessage({id:++id,...data});const r=await next();assert.equal(r.id,id);if(!allowError)assert.ok(!r.error,r.error);return r;}
function close(a,b,tolerance,label){assert.ok(Math.abs(a-b)<=Math.max(Math.abs(b),1e-300)*tolerance,label+': '+a+' vs '+b);}
const clean=({warnings,...rest})=>rest;
const config={model:'PI2',nativeInput:true,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:64,fft_y:64,eddy:{mode:0,layers:[]},filament_limit:256};
let nativeQualityFields=0,stageFields=0,independent=0,maximumStageNormError=0;
try{
  assert.equal((await next()).ready,true);
  for(const c of definition.cases){
    const text=readFileSync('web/technology/'+c.technology+'.tek','utf8');assert.equal(sha(text),original.technology_sha256[c.technology]);const technology=parseTechnology(text),creator=parseCreatorCommand(c.command),reference=original.cases.find(v=>v.name===c.name);
    configureKernel(k,technology);const before=createGeometry(k,creator.id,creator.keywords);const source=(await send({technology:text,creator})).layout;
    const samples=[];
    for(const [start,stop,step]of [[1,2,1],[5,10,2.5],[15,15,1],[20,20,1]]){
      worker.postMessage({id:++id,action:'quality-sweep',...config,start_ghz:start,stop_ghz:stop,step_ghz:step});
      while(true){
        const r=await next();assert.equal(r.id,id);assert.ok(!r.error,r.error);assert.equal(r.port_sweep.command,'PI2');const actual=r.network,expected=reference.samples.find(s=>s.frequency_hz===actual.frequency_hz);assert.ok(expected,'Unexpected native frequency '+actual.frequency_hz+' Hz');const f=expected.frequency_ghz;
        assert.equal(actual.model??'PI2','PI2');assert.equal(actual.filaments,expected.stages.capacitance_matrix.length,'Direct PI2 sweep accidentally acquired a type-8 self reference');
        const fields=['quality_first','quality_second','quality_differential'];fields.forEach((key,i)=>{assert.ok(Math.abs(actual.pi[key]-expected.quality_printed[i])<=.0005000001,c.name+' original '+f+'GHz '+key);nativeQualityFields++;});samples.push(actual);
        const hz=k._normalize_value(f,5,.1,0),lf=JSON.parse(k.UTF8ToString(k._analyze(hz))).windings[0].quality_low_frequency,series=JSON.parse(k.UTF8ToString(k._simulate(0,hz,1000,1000,1000,1000,256)));
        assert.ok(Math.abs(lf-expected.quality_low_frequency_printed)<=.0000005001);close(series.series_q,expected.series_q,1e-10,'Original LMAT Q');close(series.resistance,expected.series_impedance[0],1e-10,'Original LMAT resistance');close(series.reactance,expected.series_impedance[1],1e-10,'Original LMAT reactance');
        if(r.port_sweep.done){
          // A ten-significant-digit dump has up to 5e-10 relative rounding
          // uncertainty at a leading digit of one. Compare the matrix norm;
          // tiny cells must not select historical cancellation in production.
          for(const [key,values]of Object.entries(expected.stages)){const got=actual[key].flat(2),want=values.flat(2),norm=Math.max(...want.map(Math.abs));assert.equal(got.length,want.length);got.forEach((value,i)=>{const error=Math.abs(value-want[i])/norm;maximumStageNormError=Math.max(maximumStageNormError,error);assert.ok(error<=5e-10,c.name+' '+f+'GHz '+key+' original matrix norm '+error);stageFields++;});}
          const precise=JSON.parse(execFileSync(python,['tools/reference_two_port.py'],{input:JSON.stringify(actual),encoding:'utf8'}));for(const key of fields)close(actual.pi[key],precise.circuit_metrics.pi[key],1e-12,'Independent circuit Q '+key);independent++;
          break;
        }
        assert.equal(actual.capacitance_matrix,undefined,'Intermediate samples retained dense stages');
      }
    }
    assert.deepEqual(clean((await send({action:'edit',commands:'UNSELECT'})).layout),clean(source),'Q sweep mutated source geometry');assert.deepEqual(clean(editGeometry(k,'UNSELECT')),clean(before));
    const values=networkQualitySamples(samples),peak=values.reduce((a,b)=>a.quality>b.quality?a:b);assert.ok(values.at(-1).quality<peak.quality,'Full-network Q never falls');assert.ok(values.some(v=>v.quality<0),'Signed post-resonance Q was clamped');
    if(c.name==='cmos-medium'){assert.equal(peak.frequency_hz,reference.samples.find(s=>s.frequency_ghz===7.5).frequency_hz);assert.ok(peak.quality>values.find(v=>v.frequency_hz===10e9).quality);}
    assert.ok(reference.samples.every((s,i)=>i===0||s.series_q>reference.samples[i-1].series_q),'Original conductor Q does not reproduce the increasing curve');
    assert.deepEqual(networkQualitySamples(samples,'quality_second').map(s=>s.quality),samples.map(s=>s.pi.quality_second));assert.deepEqual(networkQualitySamples(samples,'quality_differential').map(s=>s.quality),samples.map(s=>s.pi.quality_differential));assert.ok(plotSeries(values,'quality',false).yRange[0]<0);assert.ok(portSweepMetricsCSV(samples).includes('quality_first'));
    reports.push({name:c.name,peakPort1GHz:peak.frequency_hz/1e9,samples:samples.map((s,i)=>({frequency_ghz:s.frequency_hz/1e9,port_q:[s.pi.quality_first,s.pi.quality_second,s.pi.quality_differential],original_series_q:reference.samples[i].series_q}))});
  }
  for(const extra of [{model:'PIX'},{model:'PI3'},{winding:4294967296},{winding:.5},{ground:4294967296},{sample_limit:0},{sample_limit:1.5}])assert.ok((await send({action:'quality-sweep',...config,start_ghz:5,stop_ghz:10,step_ghz:2.5,...extra},true)).error,'Invalid Q sweep accepted');
  assert.match((await send({action:'quality-sweep',...config,start_ghz:7.5,stop_ghz:7.500001,step_ghz:1e-8},true)).error,/collapse/);
  assert.throws(()=>networkQualitySamples([],'series_q'),/Select port/);assert.equal(networkQualitySamples([{frequency_hz:1,pi:{quality_first:null}}])[0].quality,null);
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/quality-sweep.json',JSON.stringify({originalProcesses:28,nativeQualityFields,stageFields,stageMatrixRelativeTolerance:5e-10,maximumStageNormError,independentCircuits:independent,model:'Repeated direct PI2, without a type-8 self-reference acquisition',reports},null,2)+'\n');
  console.log(`Q sweep: 28 original processes, ${nativeQualityFields} original port-Q reports, ${stageFields} stage fields, ${independent} independent circuits; LF/LMAT rise, PI2 peak/decline, signed Q, worker streaming, exports and source preservation pass`);
}finally{await worker.terminate();}
