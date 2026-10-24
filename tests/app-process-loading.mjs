import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {Worker as Thread} from 'node:worker_threads';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import vm from 'node:vm';
import {writeGDS} from '../web/gds.mjs';
import {parseTechnology} from '../web/technology.mjs';

// Execute the unmodified application event handlers with real WASM/worker
// responses. These minimal DOM nodes exercise control state, not rendering.
const html=readFileSync('dist/index.html','utf8'),nodes=new Map(),downloads=[];
const fieldHTML=html.match(/<div class="fields">([\s\S]*?)<\/div>/)[1];
class Node {
  constructor(tag='div'){this.tag=tag;this.children=[];this.attributes={};this.hidden=false;this.disabled=false;this.textContent='';this.firstChild={textContent:''};this.style={setProperty(){}};this.classList={add(){}};this._value=undefined;}
  get value(){return this._value??(this.tag==='select'?this.children[0]?.value??'':'');}
  set value(v){this._value=String(v);}
  get options(){return this.children;}
  get selectedOptions(){return this.children.filter(n=>n.value===this.value);}
  append(...children){this.children.push(...children);}
  replaceChildren(...children){this.children=children;if(this.tag==='select')this._value=undefined;}
  closest(selector){return selector==='label'?this.label:selector==='.fields'&&this.field?{}:null;}
  querySelector(selector){return this.children.find(n=>selector===`[value="${n.value}"]`)??null;}
  setAttribute(key,value){this.attributes[key]=String(value);}
  addEventListener(){}
  click(){if(this.tag==='a')downloads.push({href:this.href,name:this.download});}
}
const document={getElementById:id=>{assert.ok(nodes.has(id),'Missing HTML node '+id);return nodes.get(id);},createElement:tag=>new Node(tag),createElementNS:(_,tag)=>new Node(tag),querySelectorAll:()=>[...nodes.values()].filter(n=>n.field)};
for(const match of html.matchAll(/<(\w+)\b([^>]*\bid="([^"]+)"[^>]*)>/g)){
  const [full,tag,attributes,id]=match,node=new Node(tag);node.id=id;node.ownerDocument=document;node.label=new Node('label');node.field=fieldHTML.includes(`id="${id}"`);node.disabled=/\sdisabled\b/.test(attributes);node.checked=/\schecked\b/.test(attributes);node.hidden=/\shidden\b/.test(attributes);
  const value=attributes.match(/\bvalue="([^"]*)"/);if(value)node.value=value[1];
  if(tag==='select'){
    const contents=html.slice(match.index+full.length).split('</select>')[0];
    for(const option of contents.matchAll(/<option\b([^>]*)>([^<]*)<\/option>/g)){const o=new Node('option');o.value=option[1].match(/value="([^"]*)"/)?.[1]??option[2];o.textContent=option[2];node.append(o);if(/\sselected\b/.test(option[1]))node.value=o.value;}
  }else if(tag==='textarea')node.value=html.slice(match.index+full.length).split('</textarea>')[0];
  nodes.set(id,node);
}
nodes.get('controls').elements=[...nodes.values()].filter(n=>html.slice(html.indexOf('<form'),html.indexOf('</form>')).includes(`id="${n.id}"`));
const $=id=>document.getElementById(id),deferred=()=>{let resolve,reject;const promise=new Promise((a,b)=>{resolve=a;reject=b;});return {promise,resolve,reject};};
const fetches=[],sent=[],responses=[],ready=deferred();let thread,adapter;
class Worker {
  constructor(url,options){
    assert.equal(options.type,'module');adapter=this;
    const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(url.href)});parentPort.on('message',data=>self.onmessage({data}));`;
    thread=new Thread(new URL('data:text/javascript,'+encodeURIComponent(bootstrap)));
    thread.on('message',data=>{if(data.ready){ready.resolve(data);return;}try{this.onmessage({data});responses.push(data);}catch(error){responses.push({harnessError:error});}});
    thread.on('error',error=>ready.reject(error));
  }
  postMessage(data){sent.push(structuredClone(data));thread.postMessage(data);}
}
const context=vm.createContext({document,Worker,URL,Blob,location:{href:'https://example.test/reASITIC/'},console,setTimeout,fetch:url=>{const next=deferred();fetches.push({...next,url:String(url)});return next.promise;}});
const appURL=pathToFileURL(resolve('dist/app.mjs')),app=new vm.SourceTextModule(readFileSync(appURL,'utf8'),{context,identifier:appURL.href,initializeImportMeta:meta=>{meta.url=appURL.href;}});
await app.link(async specifier=>{const namespace=await import(new URL(specifier,appURL).href),keys=Object.keys(namespace);return new vm.SyntheticModule(keys,function(){for(const key of keys)this.setExport(key,namespace[key]);},{context});});
const settle=()=>new Promise(resolve=>setImmediate(resolve));
async function response(count){const deadline=Date.now()+30000;while(responses.length<count){assert.ok(Date.now()<deadline,'Application worker timeout');await new Promise(resolve=>setTimeout(resolve,10));}const data=responses[count-1];if(data.harnessError)throw data.harnessError;assert.ok(!data.error,data.error);return data;}
const text=Object.fromEntries(['CMOS','BiCMOS'].map(name=>[name,readFileSync(`web/technology/${name}.tek`,'utf8')]));
const fetchResult=(index,source,ok=true)=>fetches[index].resolve({ok,text:async()=>source});
let applied=0;
try{
  await app.evaluate();assert.equal(fetches.length,1);assert.match(fetches[0].url,/\/technology\/CMOS\.tek$/);
  adapter.onmessage({data:await ready.promise});assert.equal(sent.length,0);assert.equal($('generate').disabled,true,'Kernel readiness enabled generation while technology was pending');
  assert.equal($('process').disabled,false,'A pending download prevented choosing another process');assert.equal($('technology-file').disabled,false);
  assert.equal(await $('technology-file').onchange({target:{files:[{name:'bad-startup.tek',text:async()=>'<layer>\nt=0'}]}}),false);assert.equal(sent.length,0);assert.equal($('generate').disabled,true,'Generation enabled before a first valid technology');
  const file=deferred(),importing=$('technology-file').onchange({target:{files:[{name:'my-process.tek',text:()=>file.promise}]}});
  file.resolve(text.BiCMOS);assert.equal(await importing,true);let result=await response(++applied);assert.equal(sent.at(-1).technology,text.BiCMOS);assert.equal(result.technology.source,text.BiCMOS);assert.equal($('process').value,'custom');assert.equal($('metal').options.length,3);assert.equal($('generate').disabled,false);
  fetchResult(0,text.CMOS);await settle();assert.equal(sent.length,applied,'Stale startup process replaced the imported file');assert.equal($('process').value,'custom');

  // Overlap two actual selector handlers; the older fetch may fail after the
  // newest model is already generated. Neither its data nor error may publish.
  $('process').value='CMOS';const old=$('process').onchange();$('process').value='BiCMOS';const current=$('process').onchange();assert.equal($('generate').disabled,true);assert.equal($('simulate').disabled,true);
  fetchResult(2,text.BiCMOS);assert.equal(await current,true);result=await response(++applied);assert.equal(result.technology.source,text.BiCMOS);const status=$('status').textContent;
  fetches[1].reject(Error('obsolete request failed'));assert.equal(await old,false);assert.equal($('status').textContent,status);assert.equal($('process').value,'BiCMOS');assert.equal(sent.length,applied);

  const before=structuredClone(result.layout),count=sent.length;
  assert.equal(await $('technology-file').onchange({target:{files:[{name:'invalid.tek',text:async()=>'<layer>\nt=0'}]}}),false);
  assert.equal(sent.length,count,'Invalid process reached the C++ kernel');assert.equal($('process').value,'BiCMOS');assert.equal($('download').disabled,false);assert.equal($('generate').disabled,false);assert.match($('status').textContent,/positive thickness/);
  $('download').onclick();const retained=new Uint8Array(await (await fetch(downloads.at(-1).href)).arrayBuffer());assert.deepEqual(retained,writeGDS(before,parseTechnology(text.BiCMOS)),'Rejected process import changed the displayed/exported layout');
  $('process').value='CMOS';const failed=$('process').onchange();fetchResult(3,'',false);assert.equal(await failed,false);assert.equal($('process').value,'BiCMOS');assert.match($('status').textContent,/Unable to load technology/);assert.equal(sent.length,count);

  $('process').value='BiCMOS';const pending=$('process').onchange();const custom=$('technology-file').onchange({target:{files:[{name:'latest.tek',text:async()=>text.CMOS}]}});assert.equal(await custom,true);result=await response(++applied);fetchResult(4,text.BiCMOS);assert.equal(await pending,false);assert.equal($('process').value,'custom');assert.equal($('process').querySelector('[value="custom"]').textContent,'latest.tek');assert.equal(result.technology.source,text.CMOS);assert.equal($('metal').options.length,5);assert.equal(sent.length,applied);

  // The real form must expose orientation for every native creator that
  // accepts it. Generate the captured negative-angle wire through submit.
  for(let kind=0;kind<12;++kind){$('kind').value=kind;$('kind').onchange();assert.equal($('angle').label.hidden,kind===9);assert.equal($('angle').disabled,kind===9);}
  $('kind').value=2;$('kind').onchange();for(const [id,value]of Object.entries({'shape-name':'A',length:1,width:5,metal:0,'origin-x':128.26,'origin-y':130.24,angle:-37,phase:-1}))$(id).value=value;
  $('controls').onsubmit({preventDefault(){}});result=await response(++applied);assert.match(sent.at(-1).creator.keywords,/ORIENT=-37/);
  const original=JSON.parse(readFileSync('tests/fixtures/original-creator-coverage.json','utf8')).cases.find(c=>c.name==='cmos-wire-short-negative-angle');
  const physical=shape=>({name:shape.name,origin:shape.origin,segments:shape.segments.map(({metal,width,thickness,points})=>({metal,width,points,thickness}))});
  assert.deepEqual(result.layout.shapes.map(physical),original.shapes.map(physical));
  $('download').onclick();const downloaded=new Uint8Array(await (await fetch(downloads.at(-1).href)).arrayBuffer()),expected=writeGDS({...result.layout,shapes:original.shapes},parseTechnology(text.CMOS));assert.deepEqual(downloaded,expected,'Form GDS differs from captured original physical records');
  const qualityCase=JSON.parse(readFileSync('tests/fixtures/quality-sweep-cases.json','utf8')).cases[0],qualityReference=JSON.parse(readFileSync('tests/fixtures/original-quality-sweep.json','utf8')).cases[0];
  $('native-command').value=qualityCase.command;$('native-command').oninput();$('controls').onsubmit({preventDefault(){}});await response(++applied);
  for(const [id,value]of Object.entries({'cell-length':1000,'cell-width':1000,'cell-thickness':1000,'cap-length':1000,'cap-width':1000,'cap-fftx':64,'cap-ffty':64,'eddy-mode':0,'eddy-layer-count':0,'network-start':5,'network-end':10,'network-step':2.5}))$(id).value=value;
  $('network-quality-sweep').onclick();let qualitySamples=0;
  while(true){result=await response(++applied);qualitySamples++;assert.equal(result.port_sweep.command,'PI2');if(result.port_sweep.done)break;}
  assert.equal(qualitySamples,3);assert.equal(sent.at(-1).action,'quality-sweep');assert.equal($('network-q-label').textContent,'Port 1 Q');assert.match($('network-q-note').textContent,/7\.5[0-9]* GHz/);assert.equal($('network-q-plot').children.filter(n=>n.tag==='circle').length,3);
  const expectedQuality=qualityReference.samples.find(s=>s.frequency_ghz===10).quality_printed;assert.ok(Math.abs(Number($('network-q').textContent)-expectedQuality[0])<.00051);
  $('network-q-definition').value='quality_second';$('network-q-definition').onchange();assert.equal($('network-q-label').textContent,'Port 2 Q');assert.ok(Math.abs(Number($('network-q').textContent)-expectedQuality[1])<.00051);
  $('network-q-definition').value='quality_differential';$('network-q-definition').onchange();assert.ok(Math.abs(Number($('network-q').textContent)-expectedQuality[2])<.00051);assert.match($('network-q-note').textContent,/sweep boundary/);
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/app-process-loading.json',JSON.stringify({realApplicationHandlers:true,realWasmWorker:true,generatedLayouts:responses.filter(r=>r.layout).length,latestSelectionWins:true,staleFailureIgnored:true,invalidProcessPreservesLayout:true,failedDownloadRestoresSelection:true,kernelReadyWhileLoading:true,creatorOrientations:11,originalRotatedWire:true,gdsBytes:downloaded.byteLength,qualitySamples,originalPortQuality:true,renderedBrowserQA:false},null,2)+'\n');
  console.log(`App: ${responses.filter(r=>r.layout).length} real WASM layouts, process races/recovery, original rotated-wire GDS, original PI2 Q peak and three port definitions pass (DOM logic; no rendered-browser claim)`);
}finally{if(thread)await thread.terminate();}
