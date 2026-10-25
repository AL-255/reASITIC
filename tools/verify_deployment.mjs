// Audit the bytes actually served by Pages, then execute those downloaded
// worker/kernel modules. This verifies delivery and computation, not rendering.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,mkdtempSync,mkdirSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname,join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {Worker} from 'node:worker_threads';
import {parseCreatorCommand} from '../web/creators.mjs';
import {parseTechnology} from '../web/technology.mjs';
import {writeGDS} from '../web/gds.mjs';

const root=new URL('../',import.meta.url),sha=bytes=>createHash('sha256').update(bytes).digest('hex');
const fixture=name=>readFileSync(new URL('tests/fixtures/'+name,root));
const canonical=value=>value&&typeof value==='object'?JSON.stringify(value,Object.keys(value).sort()):JSON.stringify(value);
function releaseHash(files){return sha('{'+Object.keys(files).sort().map(name=>JSON.stringify(name)+':'+canonical(files[name])).join(',')+'}');}
function physical(layout,metals){
  const segments=values=>values.map(s=>({metal:s.metal,width:s.width,points:s.points,...(s.metal<metals?{thickness:s.thickness}:{nx:s.nx,ny:s.ny}),...(s.branches?{branches:segments(s.branches)}:{})}));
  return layout.shapes.map(s=>({name:s.name,origin:s.origin,segments:segments(s.segments),...(s.hidden?{hidden:true}:{})}));
}
function printed(actual,expected,digits,significant,label){
  expected=Number(expected);assert.ok(Number.isFinite(actual)&&Number.isFinite(expected),label+' must be finite');
  // A native printed decimal denotes a rounding interval. A stable evaluation
  // may land on its boundary: e.g. the exact wire resistance is 0.1625 ohm,
  // while the legacy inversion lands one binary64 step below that boundary.
  const quantum=significant?(expected===0?0:10**(Math.floor(Math.log10(Math.abs(expected)))-digits+1)):10**-digits;
  assert.ok(Math.abs(actual-expected)<=quantum*.5+2*Number.EPSILON*Math.max(Math.abs(actual),Math.abs(expected)),label+': '+actual+' vs printed '+expected);
}
export async function verifyDeployment({url='https://al-255.github.io/reASITIC/',expectedRelease,evidence,waitForReleaseMs=0}={}){
  const base=new URL(url);assert.ok(base.pathname.endsWith('/'),'Deployment URL must end in /');
  assert.ok(base.protocol==='https:'||(base.protocol==='http:'&&['localhost','127.0.0.1','[::1]'].includes(base.hostname)),'Use HTTPS or a local test server');
  if(expectedRelease!==undefined)assert.match(expectedRelease,/^[a-f0-9]{64}$/,'Expected release SHA');
  assert.ok(Number.isInteger(waitForReleaseMs)&&waitForReleaseMs>=0&&waitForReleaseMs<=300000,'Invalid publication wait budget');
  async function get(name,maximum=64*1024*1024){
    const target=new URL(name,base);target.searchParams.set('verify',Date.now().toString(36));
    const response=await fetch(target,{redirect:'error',cache:'no-store',signal:AbortSignal.timeout(30000)});
    assert.equal(response.status,200,'HTTP status for '+name);
    const type=(response.headers.get('content-type')??'').split(';')[0].trim().toLowerCase();
    if(/\.wasm$/.test(name))assert.equal(type,'application/wasm','WASM MIME type');
    if(/\.(?:mjs|js)$/.test(name))assert.ok(['application/javascript','text/javascript'].includes(type),'JavaScript MIME type for '+name+': '+type);
    if(/\.css$/.test(name))assert.equal(type,'text/css','CSS MIME type');
    if(/\.html$/.test(name))assert.equal(type,'text/html','HTML MIME type');
    const declared=Number(response.headers.get('content-length'));assert.ok(!declared||declared<=maximum,'Oversized response for '+name);
    const reader=response.body.getReader(),parts=[];let length=0;
    try{while(true){const {value,done}=await reader.read();if(done)break;length+=value.length;assert.ok(length<=maximum,'Oversized response for '+name);parts.push(value);}}finally{await reader.cancel();}
    return Buffer.concat(parts,length);
  }
  const deadline=Date.now()+waitForReleaseMs;let manifest;
  do{
    manifest=JSON.parse((await get('build.json',1024*1024)).toString());assert.match(manifest.release??'',/^[a-f0-9]{64}$/,'Release identifier');
    if(expectedRelease===undefined||manifest.release===expectedRelease||Date.now()>=deadline)break;
    await new Promise(resolve=>setTimeout(resolve,Math.min(2000,deadline-Date.now())));
  }while(true);
  if(expectedRelease!==undefined)assert.equal(manifest.release,expectedRelease,'The expected build has not reached this deployment');
  const prefix='releases/'+manifest.release+'/';assert.equal(manifest.entry,prefix+'app.mjs');
  assert.ok(manifest.files&&Object.keys(manifest.files).length>10&&Object.keys(manifest.files).length<1000,'Release file manifest');
  let total=0;
  for(const [name,entry]of Object.entries(manifest.files)){
    assert.ok(/^[a-zA-Z0-9_.\/-]+$/.test(name)&&!name.startsWith('/')&&!name.split('/').some(p=>!p||p==='.'||p==='..'),'Unsafe release path');
    assert.ok(Number.isSafeInteger(entry.bytes)&&entry.bytes>=0&&entry.bytes<=64*1024*1024,'Invalid file size');assert.match(entry.sha256??'',/^[a-f0-9]{64}$/);total+=entry.bytes;
  }
  assert.ok(total<=128*1024*1024,'Release exceeds audit download budget');assert.equal(releaseHash(manifest.files),manifest.release,'Manifest content does not match its release identifier');
  for(const name of ['index.html','style.css','app.mjs','worker.mjs','asitic.js','asitic.wasm','technology/CMOS.tek','technology/BiCMOS.tek'])assert.ok(manifest.files[name],'Missing release asset '+name);
  const html=await get('index.html',1024*1024);assert.equal(sha(html),manifest.root_index_sha256,'Root HTML differs from build manifest');assert.ok(html.toString().includes(`./${manifest.entry}`),'HTML selects another release');assert.ok(html.toString().includes(`href="${prefix}style.css"`),'HTML selects another stylesheet');
  const folder=mkdtempSync(join(tmpdir(),'reasitic-deployment-'));let worker;
  try{
    const names=Object.keys(manifest.files);let nextName=0,downloadedBytes=0;
    const downloads=await Promise.allSettled(Array.from({length:4},async()=>{while(nextName<names.length){const name=names[nextName++],expected=manifest.files[name],bytes=await get(prefix+name,Math.max(1,expected.bytes));assert.equal(bytes.length,expected.bytes,'Release byte count for '+name);assert.equal(sha(bytes),expected.sha256,'Release hash for '+name);const file=join(folder,name);mkdirSync(dirname(file),{recursive:true});writeFileSync(file,bytes);downloadedBytes+=bytes.length;}}));
    for(const result of downloads)if(result.status==='rejected')throw result.reason;
    const original=JSON.parse(fixture('original-creator-coverage.json')),definition=fixture('creator-coverage-cases.json'),cases=JSON.parse(definition).cases;
    assert.equal(sha(definition),original.case_definition_sha256,'Original creator case definitions changed');
    const technologies={};for(const name of Object.keys(original.technology_sha256)){const text=readFileSync(join(folder,'technology',name+'.tek'),'utf8');assert.equal(sha(text),original.technology_sha256[name],name+' original technology');technologies[name]={text,parsed:parseTechnology(text)};}
    const publicGDS=await import(pathToFileURL(join(folder,'gds.mjs')).href);
    const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(pathToFileURL(join(folder,'worker.mjs')).href)});parentPort.on('message',data=>self.onmessage({data}));`;
    worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap)));
    const inbox=[],waiters=[];let workerError;
    worker.on('message',data=>waiters.length?waiters.shift().resolve(data):inbox.push(data));worker.on('error',error=>{workerError=error;while(waiters.length)waiters.shift().reject(error);});
    function next(){if(workerError)return Promise.reject(workerError);if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const waiting={resolve:data=>{clearTimeout(timer);resolve(data);},reject:error=>{clearTimeout(timer);reject(error);}},timer=setTimeout(()=>{const i=waiters.indexOf(waiting);if(i>=0)waiters.splice(i,1);reject(Error('Deployed worker timeout'));},30000);waiters.push(waiting);});}
    let id=0;async function send(data){worker.postMessage({id:++id,...data});const actual=await next();assert.equal(actual.id,id);assert.ok(!actual.error,actual.error);return actual;}
    assert.equal((await next()).ready,true);
    let fields=0,gdsBytes=0;const families=new Set();
    const count=value=>typeof value==='number'?1:value&&typeof value==='object'?Object.values(value).reduce((sum,v)=>sum+count(v),0):0;
    for(const item of cases){
      const reference=original.cases.find(c=>c.name===item.name);assert.ok(reference,'Missing original case '+item.name);
      const technology=technologies[item.technology],creator=parseCreatorCommand(item.command,item.answers);families.add(creator.id);
      let actual=await send({technology:technology.text,creator});if(item.edits)actual=await send({action:'edit',commands:item.edits});
      assert.deepEqual(physical(actual.layout,reference.metal_count),reference.shapes,item.name+' deployed original physical geometry');fields+=count(reference.shapes);
      const gds=publicGDS.writeGDS(actual.layout,actual.technology),expected=writeGDS({...actual.layout,shapes:reference.shapes},technology.parsed);assert.deepEqual(gds,expected,item.name+' deployed GDS');gdsBytes+=gds.length;
      for(const shape of actual.layout.shapes)assert.ok(Math.abs(shape.resistance_dc-Number(reference.resistance_dc_printed[shape.name]))<=5.1e-7,item.name+' deployed original DC resistance');
    }
    assert.equal(families.size,12,'Original creator family coverage');
    const cmos=technologies.CMOS.text,capFixture=JSON.parse(fixture('original-capacitance.json')),networkFixture=JSON.parse(fixture('original-network.json'));
    assert.equal(sha(cmos),capFixture.technology_sha256);assert.equal(sha(cmos),networkFixture.technology_sha256);
    const capCase=JSON.parse(fixture('capacitance-cases.json')).cases.find(c=>c.name==='wire-coarse'),capReference=capFixture.cases.find(c=>c.name===capCase.name);
    const wire=(await send({technology:cmos,creator:{id:capCase.id,keywords:capCase.keywords}})).layout;
    const cap=(await send({action:'capacitance',frequency_ghz:capCase.frequency_ghz,mesh:{length:capCase.cells[0],width:capCase.cells[1]},fft_x:capCase.grid[0],fft_y:capCase.grid[1],contact_limit:128,nativeInput:true})).capacitance;
    assert.match(cap.green_table_hash,/^[a-f0-9]{16}$/);assert.equal(Number((cap.capacitance_f*1e15).toFixed(3)),capReference.capacitance_ff_printed);assert.equal(Number(cap.resistance.toFixed(3)),capReference.resistance_printed);
    for(const key of ['matrix','potential_matrix'])assert.deepEqual(cap[key].map(row=>row.map(z=>z.map(v=>Number(v.toPrecision(10))))),capReference[key],'Deployed original '+key);
    const network=(await send({action:'network',frequency_ghz:1,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:8,fft_y:8,eddy:{mode:0,layers:[]},nativeInput:true})).network;
    const reference=networkFixture.cases.find(c=>c.name==='wire-coarse'),p=network.pi;
    for(const [i,key]of ['quality_first','quality_second','quality_differential'].entries())printed(p[key],reference.quality_printed[i],3,false,'Deployed original '+key);
    const metrics=[p.inductance_h/1e-9,p.resistance,p.capacitance_second_f/1e-15,p.resistance_second,p.capacitance_first_f/1e-15,p.resistance_first,network.resonance_hz/1e9];metrics.forEach((v,i)=>printed(v,reference.pi_printed[i],i===6?2:3,i!==6,'Deployed original pi metric '+i));
    network.input_impedance.forEach((row,i)=>row.forEach((v,j)=>printed(v,reference.input_impedance_printed[i][j],3,false,'Deployed original input impedance')));
    for(const key of ['capacitance_matrix','inductance_admittance_matrix','node_capacitance_matrix']){
      assert.equal(network[key].length,reference[key].length);network[key].forEach((row,i)=>row.forEach((z,j)=>z.forEach((v,k)=>printed(v,reference[key][i][j][k],10,true,'Deployed original '+key))));
    }
    assert.equal(p.resistance,wire.shapes[0].resistance_dc,'Single-branch series resistance must equal its physical DC resistance');
    const report={url:base.href,release:manifest.release,expectedRelease:expectedRelease??null,verifiedAt:new Date().toISOString(),files:names.length,downloadedBytes,originalCreatorCases:cases.length,creatorFamilies:families.size,originalPhysicalFields:fields,gdsBytes,originalCapacitance:true,originalPiMetrics:true,metricComparison:'Original printed decimal rounding intervals; exact single-branch DC resistance; ten-digit original stage matrices',wire:{capacitance_f:cap.capacitance_f,series_resistance_ohm:p.resistance,original_printed_resistance_ohm:reference.pi_printed[1],green_table_hash:cap.green_table_hash},downloadedWasmWorker:true,renderedBrowserQA:false};
    if(evidence){mkdirSync(dirname(evidence),{recursive:true});writeFileSync(evidence,JSON.stringify(report,null,2)+'\n');}return report;
  }finally{if(worker)await worker.terminate();rmSync(folder,{recursive:true,force:true});}
}
if(process.argv[1]&&pathToFileURL(resolve(process.argv[1])).href===import.meta.url){
  const option=name=>{const i=process.argv.indexOf(name);if(i<0)return undefined;assert.ok(i+1<process.argv.length&&!process.argv[i+1].startsWith('--'),'Missing value for '+name);return process.argv[i+1];};
  const evidence=option('--evidence')??'build/evidence/deployment.json',url=option('--url'),expectedRelease=option('--expected-release'),waitForReleaseMs=Number(option('--wait-for-release-ms')??0);
  try{
    const result=await verifyDeployment({url,expectedRelease,evidence,waitForReleaseMs});
    console.log(`Deployed release ${result.release}: ${result.files} verified files, ${result.originalCreatorCases} original cases / ${result.creatorFamilies} families, exact physical geometry/GDS and original CAP/PI2 reports pass`);
  }catch(error){mkdirSync(dirname(evidence),{recursive:true});writeFileSync(evidence,JSON.stringify({url,expectedRelease,verifiedAt:new Date().toISOString(),status:'failed',error:error.message},null,2)+'\n');throw error;}
}
