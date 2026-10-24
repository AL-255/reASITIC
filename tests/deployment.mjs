import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {resolve,sep} from 'node:path';
import {verifyDeployment} from '../tools/verify_deployment.mjs';

const directory=resolve('dist'),manifest=JSON.parse(readFileSync('dist/build.json','utf8')),requests=[];let mode='valid',manifestReads=0;
const server=createServer((req,res)=>{
  const url=new URL(req.url,'http://localhost'),relative=decodeURIComponent(url.pathname.slice('/reASITIC/'.length)),file=resolve(directory,relative||'index.html');requests.push(url.pathname);
  if(!url.pathname.startsWith('/reASITIC/')||!file.startsWith(directory+sep)){res.writeHead(404);res.end();return;}
  if(mode==='missing-worker'&&relative.endsWith('/worker.mjs')){res.writeHead(404);res.end('Not deployed');return;}
  if(mode==='redirect-worker'&&relative.endsWith('/worker.mjs')){res.writeHead(302,{location:'/reASITIC/worker.mjs'});res.end();return;}
  let bytes;try{bytes=readFileSync(file);}catch{res.writeHead(404);res.end();return;}
  if(mode==='unsafe-path'&&relative==='build.json'){const bad=structuredClone(manifest);bad.files['../outside.mjs']={bytes:0,sha256:'0'.repeat(64)};bytes=Buffer.from(JSON.stringify(bad));}
  if(mode==='publication-delay'&&relative==='build.json'&&manifestReads++===0){const old=structuredClone(manifest);old.release='0'.repeat(64);bytes=Buffer.from(JSON.stringify(old));}
  if(mode==='stale-html'&&relative==='index.html')bytes=Buffer.concat([bytes,Buffer.from('\n<!-- another deployment -->')]);
  if(mode==='corrupt-wasm'&&relative.endsWith('.wasm')){bytes=Buffer.from(bytes);bytes[bytes.length-1]^=1;}
  let type=relative.endsWith('.wasm')?'application/wasm':/\.(mjs|js)$/.test(relative)?'text/javascript':relative.endsWith('.css')?'text/css':relative.endsWith('.html')?'text/html':'application/octet-stream';
  if((mode==='wrong-wasm-mime'&&relative.endsWith('.wasm'))||(mode==='wrong-js-mime'&&/\.(mjs|js)$/.test(relative)))type='text/plain';
  res.writeHead(200,{'content-type':type,'content-length':bytes.length});res.end(bytes);
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const url=`http://127.0.0.1:${server.address().port}/reASITIC/`,expectedRelease=manifest.release;
try{
  const good=await verifyDeployment({url,expectedRelease});assert.equal(good.creatorFamilies,12);assert.equal(good.originalCreatorCases,37);assert.equal(good.downloadedWasmWorker,true);assert.equal(good.renderedBrowserQA,false);
  await assert.rejects(verifyDeployment({url,expectedRelease:'0'.repeat(64)}),/expected build has not reached/);
  await assert.rejects(verifyDeployment({url,expectedRelease:''}),/Expected release SHA/);
  mode='publication-delay';const propagated=await verifyDeployment({url,expectedRelease,waitForReleaseMs:50});assert.equal(propagated.release,expectedRelease);assert.equal(manifestReads,2);mode='valid';
  await assert.rejects(verifyDeployment({url,expectedRelease:'0'.repeat(64),waitForReleaseMs:10}),/expected build has not reached/);
  const failures=[['stale-html',/Root HTML differs/],['unsafe-path',/Unsafe release path/],['wrong-wasm-mime',/WASM MIME type/],['wrong-js-mime',/JavaScript MIME type/],['corrupt-wasm',/Release hash/],['missing-worker',/HTTP status/],['redirect-worker',/fetch failed/]];
  for(const [name,error]of failures){mode=name;await assert.rejects(verifyDeployment({url,expectedRelease}),error,name+' was accepted');}
  assert.ok(!requests.includes('/reASITIC/worker.mjs'),'Audit fell back to the mutable root worker');assert.ok(!requests.includes('/reASITIC/asitic.wasm'),'Audit fell back to the mutable root WASM');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/deployment-verifier.json',JSON.stringify({release:good.release,localDeployedWorker:true,originalCreatorCases:good.originalCreatorCases,creatorFamilies:good.creatorFamilies,originalPhysicalFields:good.originalPhysicalFields,originalCapacitance:true,originalPiMetrics:true,boundedPublicationWait:true,rejected:['wrong release','empty release pin','publication deadline',...failures.map(([name])=>name)],rootArtifactFallback:false,renderedBrowserQA:false},null,2)+'\n');
  console.log('Deployment audit: real downloaded worker, all twelve original creator families and CAP/PI2; wrong releases, stale HTML, unsafe paths, MIME errors, corrupt/missing/redirected assets reject without root fallback');
}finally{server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
