import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {Worker} from 'node:worker_threads';
import make from '../dist/asitic.js';
import {creatorAliases,creatorIds,parseCreatorCommand} from '../web/creators.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {writeGDS} from '../web/gds.mjs';
const read=name=>readFileSync(name,'utf8'),sha=x=>createHash('sha256').update(x).digest('hex');
const fixture=JSON.parse(read('tests/fixtures/original-creator-aliases.json')),canonicalText=read('tests/fixtures/original-creator-binary.json'),canonical=JSON.parse(canonicalText),techText=read('web/technology/CMOS.tek'),technology=parseTechnology(techText),catalog=read('vendor/asitic/src/command_catalog.inc');
assert.equal(sha(techText),fixture.technology_sha256);assert.equal(sha(read('tests/fixtures/original-creators.json')),fixture.case_definition_sha256);assert.equal(sha(canonicalText),fixture.canonical_geometry_sha256);assert.equal(sha(catalog),fixture.catalog_sha256);
const rows=catalog.split('\n').map(line=>line.match(/^\{(\d+),\{([^}]+)\}/)).filter(Boolean).map(m=>({id:Number(m[1]),aliases:JSON.parse('['+m[2]+']')})).filter(row=>Object.values(creatorIds).includes(row.id));
const expectedAliases=Object.fromEntries(rows.flatMap(row=>row.aliases.filter(alias=>alias&&alias===alias.toUpperCase()).map(alias=>[alias,row.id])));
assert.deepEqual(creatorAliases,expectedAliases,'Browser aliases differ from the native normalized catalog');assert.equal(fixture.cases.length,rows.reduce((n,r)=>n+r.aliases.filter(Boolean).length,0));
function physicalHash(layout){
  const parts=[],integer=value=>{const b=Buffer.alloc(4);b.writeInt32LE(value);parts.push(b);},number=value=>{const b=Buffer.alloc(8);b.writeDoubleLE(value);parts.push(b);};
  integer(layout.shapes.length);
  for(const s of [...layout.shapes].sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0)){
    parts.push(Buffer.from(s.name+'\0','ascii'));s.origin.forEach(number);integer(s.segments.length);
    for(const segment of s.segments){integer(segment.metal);number(segment.width);if(segment.metal>=technology.metals.length){integer(segment.nx);integer(segment.ny);}else number(segment.thickness);segment.points.flat().forEach(number);}
  }
  return sha(Buffer.concat(parts));
}
const k=await make();configureKernel(k,technology);const canonicalGDS=new Map();
for(const c of canonical.cases){const r=parseCreatorCommand(c.command,c.answers);canonicalGDS.set(c.family,writeGDS(createGeometry(k,r.id,r.keywords,r.answers),technology));}
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Creator-alias worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0,accepted=0,rejected=0,variants=0;const reports=[];
try{
  assert.equal((await next()).ready,true);
  for(const c of fixture.cases){
    const original=canonical.cases.find(v=>v.family===c.family),tail=original.command.slice(original.command.indexOf(' ')+1),hasKeywords=original.command.includes(' '),commands=[...c.variants.map(token=>token+(c.accepted&&hasKeywords?' '+tail:'')),c.delimiter_command];
    if(!c.accepted){commands.forEach(command=>assert.throws(()=>parseCreatorCommand(command,original.answers),/Use SQUARE/));rejected++;continue;}
    accepted++;
    for(const command of commands){
      const creator=parseCreatorCommand(command,original.answers);assert.equal(creator.id,c.id);assert.equal(creator.keywords,hasKeywords?tail:'');
      const direct=createGeometry(k,creator.id,creator.keywords,creator.answers);assert.equal(physicalHash(direct),c.geometry_sha256,command+' original physical fields');assert.deepEqual(writeGDS(direct,technology),canonicalGDS.get(c.family),command+' GDS changed');
      worker.postMessage({id:++id,creator,technology:techText});const result=await next();assert.equal(result.id,id);assert.ok(!result.error,result.error);assert.equal(physicalHash(result.layout),c.geometry_sha256,command+' worker original physical fields');assert.deepEqual(writeGDS(result.layout,technology),canonicalGDS.get(c.family));variants++;
    }
    reports.push({alias:c.alias,family:c.family,variants:commands.length,geometry_sha256:c.geometry_sha256});
  }
  for(const command of ['',', ,','LMAT A 1','constructor','__proto__','ſQUARE NAME=A','wıre NAME=A','\u00a0SQ NAME=A'])assert.throws(()=>parseCreatorCommand(command),/Use SQUARE/);
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/creator-aliases.json',JSON.stringify({acceptedAliases:accepted,rejectedCatalogAliases:rejected,originalProcesses:fixture.cases.length*4,acceptedVariants:variants,directAndWorkerPhysicalBitsExact:true,gdsIdentical:true,reports},null,2)+'\n');
  console.log(`Creator aliases: ${accepted} accepted aliases, ${rejected} native catalog rejections, ${variants} case/delimiter variants; original physical bits, actual worker layouts and GDS bytes match`);
}finally{await worker.terminate();}
