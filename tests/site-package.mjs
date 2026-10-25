import {relativeClose} from './green-accuracy.mjs';
import assert from 'node:assert/strict';
import {cpSync,readFileSync,writeFileSync,mkdtempSync,rmSync,statSync,mkdirSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve,sep} from 'node:path';
import {pathToFileURL} from 'node:url';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {createServer} from 'node:http';
import vm from 'node:vm';
import {Worker} from 'node:worker_threads';
import {parseTechnology} from '../web/technology.mjs';
import {writeGDS} from '../web/gds.mjs';
import {revealReload} from '../web/startup.mjs';
const sha=x=>createHash('sha256').update(x).digest('hex'),work=mkdtempSync(join(tmpdir(),'reasitic-site-')),a=join(work,'a'),b=join(work,'b');
const pack=dir=>{execFileSync('python3',['tools/package_site.py','--directory',dir],{encoding:'utf8'});return JSON.parse(readFileSync(join(dir,'build.json'),'utf8'));};
const entry=html=>JSON.parse(html.match(/import\(("\.\/releases\/[a-f0-9]{64}\/app\.mjs")\)/)[1]);
function audit(dir,manifest){
  assert.match(manifest.release,/^[a-f0-9]{64}$/);const folder='releases/'+manifest.release+'/',html=readFileSync(join(dir,'index.html'),'utf8');assert.equal(sha(html),manifest.root_index_sha256);assert.equal(entry(html),'./'+manifest.entry);assert.ok(html.includes(`href="${folder}style.css"`));
  for(const [name,expected]of Object.entries(manifest.files)){const bytes=readFileSync(join(dir,folder,name));assert.equal(bytes.length,expected.bytes,name);assert.equal(sha(bytes),expected.sha256,name);}
  // Check every literal local module/worker/WASM reference in the snapshot.
  // Runtime technology URLs are module-relative and covered by both files below.
  let references=0;
  for(const name of Object.keys(manifest.files).filter(n=>/\.(?:mjs|js)$/.test(n))){
    const source=readFileSync(join(dir,folder,name),'utf8'),base=new URL(folder+name,'https://example.test/reASITIC/');
    const imports=[...source.matchAll(/^import\s+[^\n]*?from\s*['"]([^'"]+)['"]/gm)].map(m=>m[1]),urls=[...source.matchAll(/new URL\(\s*['"]([^'"]+)['"]\s*,\s*import\.meta\.url\)/g)].map(m=>m[1]);
    for(const specifier of [...imports,...urls]){if(/^[a-z]+:/i.test(specifier))continue;const url=new URL(specifier,base);assert.ok(url.pathname.startsWith('/reASITIC/'+folder),name+' escaped its release: '+specifier);assert.ok(statSync(join(dir,url.pathname.slice('/reASITIC/'.length))).isFile(),url.href);references++;}
  }
  assert.ok(references>15);assert.ok(readFileSync(join(dir,folder,'app.mjs'),'utf8').includes('fetch(new URL(`./technology/'));for(const name of ['CMOS','BiCMOS'])assert.ok(parseTechnology(readFileSync(join(dir,folder,'technology',name+'.tek'),'utf8')).metals.length>0);
  return references;
}
let active,server,worker;
try{
  cpSync('dist',a,{recursive:true});const first=pack(a),originalHTML=readFileSync(join(a,'index.html'),'utf8');assert.deepEqual(pack(a),first,'Packaging changed an unchanged release');assert.equal(readFileSync(join(a,'index.html'),'utf8'),originalHTML);const references=audit(a,first);
  cpSync(a,b,{recursive:true});writeFileSync(join(b,'worker.mjs'),readFileSync(join(b,'worker.mjs'),'utf8')+'\n// Simulated later deployment.\n');const second=pack(b);assert.notEqual(first.release,second.release);assert.equal(first.files['asitic.wasm'].sha256,second.files['asitic.wasm'].sha256,'Deployment test changed mathematical code');rmSync(join(b,'releases',first.release),{recursive:true});audit(b,second);
  active=a;server=createServer((req,res)=>{
    const url=new URL(req.url,'http://localhost');if(!url.pathname.startsWith('/reASITIC/')){res.writeHead(404);res.end();return;}
    const relative=decodeURIComponent(url.pathname.slice('/reASITIC/'.length))||'index.html',file=resolve(active,relative);
    if(!file.startsWith(active+sep)){res.writeHead(403);res.end();return;}
    try{const bytes=readFileSync(file);res.writeHead(200,{'content-type':file.endsWith('.wasm')?'application/wasm':file.endsWith('.html')?'text/html':'text/javascript','cache-control':'max-age=600'});res.end(bytes);}catch{res.writeHead(404);res.end('Missing release');}
  });await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const base=`http://127.0.0.1:${server.address().port}/reASITIC/`,cached=new Map();
  async function cachedFetch(url){const key=String(url);if(cached.has(key))return cached.get(key);const response=await fetch(url);assert.equal(response.headers.get('cache-control'),'max-age=600');const body=await response.text();cached.set(key,body);return body;}
  const oldPage=await cachedFetch(base);assert.equal((await fetch(new URL(entry(oldPage),base))).status,200);active=b;assert.equal(await cachedFetch(base),oldPage,'Test did not preserve the cached HTML');assert.equal((await fetch(new URL(entry(oldPage),base))).status,404,'A vanished release was replaced with different code');assert.equal((await fetch(new URL('app.mjs',base))).status,200,'Test must distinguish immutable entries from mutable root files');
  const nodes={engine:{textContent:''},status:{textContent:''},'reload-app':{href:'./',hidden:true}},errors=[],requested=[];
  const bootstrap=oldPage.match(/<script type="module">([\s\S]*?)<\/script>/)[1],context=vm.createContext({URL,location:{href:base},Date:{now:()=>1234567890123},document:{getElementById:id=>nodes[id]},console:{error:e=>errors.push(String(e))}});
  const script=new vm.Script(bootstrap,{importModuleDynamically:async specifier=>{requested.push(specifier);const response=await fetch(new URL(specifier,base));if(!response.ok)throw Error('HTTP '+response.status);return import('data:text/javascript,export const loaded=true;');}});await script.runInContext(context);
  assert.equal(requested[0],entry(oldPage));assert.equal(nodes['reload-app'].hidden,false);assert.match(nodes.status.textContent,/Reload to get the current version/);assert.equal(errors.length,1);const reload=new URL(nodes['reload-app'].href);assert.equal(reload.pathname,'/reASITIC/');assert.ok(reload.searchParams.has('reload'));const currentPage=await cachedFetch(reload);assert.equal(entry(currentPage),'./'+second.entry);assert.equal((await fetch(new URL(entry(currentPage),base))).status,200);
  const freshNodes={engine:{textContent:'initial'},status:{textContent:'initial'},'reload-app':{href:'./',hidden:true}},freshErrors=[];
  const freshScript=new vm.Script(currentPage.match(/<script type="module">([\s\S]*?)<\/script>/)[1],{importModuleDynamically:async specifier=>{const response=await fetch(new URL(specifier,base));assert.equal(response.status,200);return import('data:text/javascript,export const loaded=true;');}});
  await freshScript.runInContext(vm.createContext({URL,location:{href:base},Date:{now:()=>1234567890124},document:{getElementById:id=>freshNodes[id]},console:{error:e=>freshErrors.push(e)}}));assert.equal(freshNodes['reload-app'].hidden,true);assert.deepEqual(freshErrors,[]);assert.equal(freshNodes.status.textContent,'initial');
  // A page cached before the reload anchor existed must recover as well.
  const oldNodes=new Map(),added=[],oldDocument={getElementById:id=>oldNodes.get(id),createElement:()=>({}),querySelector:()=>({append:node=>{added.push(node);oldNodes.set(node.id,node);}})};
  const oldLink=revealReload(oldDocument,base+'index.html?old=1#plot',1);assert.equal(oldLink.hidden,false);assert.equal(new URL(oldLink.href).pathname,'/reASITIC/');assert.equal(revealReload(oldDocument,base,2),oldLink);assert.equal(added.length,1);assert.equal(new URL(oldLink.href).searchParams.get('reload'),'2');
  // A cached/unversioned binary must never be a fallback for the snapshot.
  writeFileSync(join(b,'asitic.wasm'),'This is a deliberately incompatible mutable root artifact.');
  // Instantiate the *packaged* worker and actual WASM from the release folder.
  const folder=join(b,'releases',second.release),bootstrapWorker=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(pathToFileURL(join(folder,'worker.mjs')).href)});parentPort.on('message',data=>self.onmessage({data}));`;
  worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrapWorker)));const inbox=[],waiters=[];worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
  function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Packaged worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
  let id=0;async function send(data){worker.postMessage({id:++id,...data});const response=await next();assert.equal(response.id,id);assert.ok(!response.error,response.error);return response;}
  assert.equal((await next()).ready,true);const technology=readFileSync(join(folder,'technology/CMOS.tek'),'utf8'),layout=(await send({technology,creator:{id:2,keywords:'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100'}})).layout;
  const cap=(await send({action:'capacitance',frequency_ghz:1,mesh:{length:1000,width:1000},fft_x:8,fft_y:8,contact_limit:128,nativeInput:true})).capacitance;relativeClose(cap.capacitance_f,3.168713072636854e-13,'Original wire capacitance');assert.match(cap.green_table_hash,/^[a-f0-9]{16}$/);
  await send({action:'network',frequency_ghz:1,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:8,fft_y:8,eddy:{mode:0,layers:[]},nativeInput:true});
  const loaded=(await send({action:'loaded-port',nativeInput:true})).loaded_port,observed=JSON.parse(readFileSync('tests/fixtures/original-loaded-port.json','utf8')).cases.find(c=>c.name==='wire-default').printed[1].match(/([-+\d.e]+)\s*\+ j\s*([-+\d.e]+)/i).slice(1).map(Number);
  assert.equal(loaded.status,'finite');assert.equal(Number(loaded.impedance_ohm[0].toFixed(3)),observed[0]);assert.equal(Number(loaded.impedance_ohm[1].toFixed(2)),observed[1]);
  const loadedExport=await import(pathToFileURL(join(folder,'loaded-port.mjs')).href);assert.ok(loadedExport.loadedPortCSV(loaded).includes('input_real_ohm'));
  const pi3=(await send({action:'network',model:'PI3',ground:0,frequency_ghz:1,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:8,fft_y:8,eddy:{mode:0,layers:[]},nativeInput:true})).network;assert.equal(pi3.model,'PI3');assert.equal(pi3.filaments,1);
  const exported=await import(pathToFileURL(join(folder,'gds.mjs')).href),gds=exported.writeGDS(layout,parseTechnology(technology));assert.deepEqual(gds,writeGDS(layout,parseTechnology(technology)));
  // Deploying a newer site may remove this release after the main kernel is
  // already running. Its first optimizer must still create a private instance.
  const wasm=readFileSync(join(folder,'asitic.wasm'));rmSync(join(folder,'asitic.wasm'));
  const definition=JSON.parse(readFileSync('tests/fixtures/optimization-cases.json','utf8')).cases[0],original=JSON.parse(readFileSync('tests/fixtures/original-optimization.json','utf8')).cases.find(c=>c.name===definition.name);
  worker.postMessage({id:++id,action:'optimize',technology,keywords:definition.keywords,mesh:{length:1000,width:1000,thickness:1000},contact_mesh:{length:1000,width:1000},fft_x:128,fft_y:128,contact_limit:128,filament_limit:256,eddy:{mode:0,layers:[]},options:{candidate_limit:3,search:'SWEEP'}});
  let optimized=0;
  while(true){const response=await next();assert.equal(response.id,id);assert.ok(!response.error,response.error);const r=response.optimization;if(r.sample){const expected=original.candidates[optimized++];assert.ok(!r.sample.error,r.sample.error);assert.deepEqual(r.sample.descriptor,expected.descriptor);for(const [key,index]of [['inductance_h',1],['resistance',2],['quality_first',7]])assert.ok(Math.abs(r.sample.pi[key]-expected.pi[index])<=Math.abs(expected.pi[index])*1e-8,'Original candidate '+key);}if(r.done)break;}
  assert.equal(optimized,3);const unchanged=(await send({action:'edit',commands:'UNSELECT'})).layout,clean=({warnings,...rest})=>rest;assert.deepEqual(clean(unchanged),clean(layout));const after=(await send({action:'capacitance',frequency_ghz:1,mesh:{length:1000,width:1000},fft_x:8,fft_y:8,contact_limit:128,nativeInput:true})).capacitance;assert.deepEqual(after,cap);
  const legacy=join(work,'legacy');cpSync(b,legacy,{recursive:true});writeFileSync(join(legacy,'asitic.js'),'export default async()=>({});\n');writeFileSync(join(legacy,'asitic.wasm'),wasm);
  const oldWorkerSource=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(pathToFileURL(join(legacy,'worker.mjs')).href)});`;
  const oldWorker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(oldWorkerSource)));
  try{const error=await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Old kernel startup guard timeout')),30000);oldWorker.once('error',e=>{clearTimeout(timer);resolve(e);});oldWorker.once('message',()=>{clearTimeout(timer);reject(Error('An incompatible cached kernel reported ready'));});});assert.match(error.message,/out of date.*Reload/);}finally{await oldWorker.terminate();}
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/site-package.json',JSON.stringify({release:JSON.parse(readFileSync('dist/build.json','utf8')).release,files:Object.keys(first.files).length,localModuleReferences:references,immutableGraph:true,mutableRootWasmIgnored:true,reproduciblePackaging:true,staleHtmlRecovery:true,legacyPageRecovery:true,incompatibleKernelRejectedBeforeReady:true,packagedWorker:true,deferredOptimizerAfterReleaseRemoval:true,originalOptimizerCandidates:optimized,liveLayoutPreserved:true,originalCapacitance:cap.capacitance_f,productionGreenHash:cap.green_table_hash,gdsBytes:gds.byteLength},null,2)+'\n');
  console.log('Site package: coherent content-addressed assets, idempotent snapshots, stale-page reload, real packaged WASM/worker CAP and identical GDS export pass');
}finally{if(worker)await worker.terminate();if(server){server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}rmSync(work,{recursive:true,force:true});}
