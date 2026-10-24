import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import createKernel from '../dist/asitic.js';
import {writeGDS} from '../web/gds.mjs';
import {creatorRecipe,parseCreatorCommand} from '../web/creators.mjs';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
const fixture=JSON.parse(readFileSync(new URL('./fixtures/original-creators.json',import.meta.url),'utf8'));
const kernel=await createKernel(),technology=parseTechnology(readFileSync(new URL('../web/technology/CMOS.tek',import.meta.url),'utf8'));
configureKernel(kernel,technology);
const ids={square:0,polygon:1,wire:2,transformer:3,capacitor:4,balun:5,mirror_3d_transformer:10,via:14,symmetric_square:16,symmetric_polygon:17,multimetal_square:18,ring:22};
const coordinates={AX:[2,0],AY:[2,1],BX:[3,0],BY:[3,1],CX:[4,0],CY:[4,1],DX:[5,0],DY:[5,1],PORT1X:[0,0],PORT1Y:[0,1],PORT2X:[1,0],PORT2Y:[1,1]};
let printed=0,exact=0;const report=[];
function compare(a,b,path='root') {
  if(typeof a==='number') {assert.equal(a,b,path);exact++;}
  else if(a && typeof a==='object') {assert.deepEqual(Object.keys(a).filter(k=>k!=='warnings'),Object.keys(b).filter(k=>k!=='warnings'),path);for(const key of Object.keys(a))if(key!=='warnings')compare(a[key],b[key],path+'.'+key);}
  else assert.equal(a,b,path);
}
for(const item of fixture.fixtures) {
  const id=ids[item.family],command=item.script.split('\n')[2],keywords=command.includes(' ')?command.slice(command.indexOf(' ')+1):'',answers=keywords?'':item.stdin.replace(/EXIT\n$/,'');
  const actual=createGeometry(kernel,id,keywords,answers,{snap:true});
  const native=JSON.parse(execFileSync(new URL('../build/native-geometry',import.meta.url).pathname,['--creator',String(id),keywords,answers,'.1','1'],{encoding:'utf8'}));
  compare(actual,native,item.family);
  assert.equal(actual.shapes.length,item.shapes.length,item.family);
  for(let j=0;j<item.shapes.length;j++) {
    const shape=actual.shapes[j],expected=item.shapes[j];assert.equal(shape.name,expected.fields.NAME);assert.equal(shape.segments.length,expected.segments.length,item.family);
    shape.segments.forEach((s,i)=>{const e=expected.segments[i];assert.equal([...technology.metals,...technology.vias][s.metal].name,e.METAL);for(const [key,[point,axis]] of Object.entries(coordinates)){assert.ok(Math.abs(s.points[point][axis]-e[key])<=5.1e-7,`${item.family}[${i}].${key}: ${s.points[point][axis]} vs ${e[key]}`);printed++;}});
    const dc=item.resistance_dc[shape.name];if(dc!==undefined){assert.ok(Math.abs(shape.resistance_dc-dc)<=5.1e-7,`${item.family}: resistance ${shape.resistance_dc} vs ${dc}`);printed++;}
  }
  mkdirSync(new URL('../build/evidence/',import.meta.url),{recursive:true});writeFileSync(new URL(`../build/evidence/creator-${item.family}.gds`,import.meta.url),writeGDS(actual,technology));
  report.push({family:item.family,shapes:actual.shapes.length,segments:actual.shapes.reduce((n,s)=>n+s.segments.length,0)});
  console.log(item.family+': native fields exact; original SAVE printed fields match');
}
// Parser rules that the original low-level web adapter could not express.
for(const [id,line]of [[2,'NAME=SHORT:LEN=1:WID=5:METAL=M1'],[0,'NAME=INNER:ILEN=50:IWID=40:W=3:S=2:N=2.13:METAL=M4:EXIT=M3'],[14,'NAME=V:NX=7:NY=9:VIA=VIA2:PHASE=-1']]) {
  const a=createGeometry(kernel,id,line),b=JSON.parse(execFileSync(new URL('../build/native-geometry',import.meta.url).pathname,['--creator',String(id),line,'','.1','1'],{encoding:'utf8'}));compare(a,b,line);
}
for(let kind=0;kind<12;kind++) {
  const recipe=creatorRecipe(kind,[100,90,3,kind===7?8:kind===5?20:2,2,50,8,3,2,0,15,1.5],{name:'P',secondaryName:'S',nx:5,ny:7,via:2});
  const layout=createGeometry(kernel,recipe.id,recipe.keywords,recipe.answers);assert.ok(layout.shapes.length,`form kind ${kind}`);
  if(kind===9)assert.deepEqual([layout.shapes[0].segments[0].nx,layout.shapes[0].segments[0].ny],[5,7]);
}
assert.deepEqual(parseCreatorCommand('wire NAME=A:LEN=1:WID=5'),{id:2,keywords:'NAME=A:LEN=1:WID=5',answers:''});
assert.throws(()=>parseCreatorCommand('LMAT A 1'),/Use SQUARE/);
assert.throws(()=>createGeometry(kernel,18,'NAME=A:LEN=100:WID=90:W=3:S=2:N=2:METAL=M4:EXIT=-1'),/lower exit metal/);
const first=createGeometry(kernel,2,'NAME=A:LEN=20:WID=3:METAL=M1');
assert.throws(()=>createGeometry(kernel,0,'NAME=B:LEN=20:W=3:S=2:N=64','',{append:true}),/./);
assert.throws(()=>createGeometry(kernel,2,'NAME=A:LEN=20:WID=3:METAL=M1','',{append:true}),/already/);
const both=createGeometry(kernel,2,'NAME=B:LEN=20:WID=3:METAL=M1','',{append:true});assert.equal(both.shapes.length,2);assert.deepEqual(both.shapes[1],first.shapes[0]);
createGeometry(kernel,2,'NAME=G:LEN=100:WID=40:METAL=M2:XORG=100:YORG=100');
const grounded=createGeometry(kernel,2,'NAME=A:LEN=100:WID=40:METAL=M1:XORG=100:YORG=100','',{append:true});
writeFileSync(new URL('../build/evidence/composed-ground.gds',import.meta.url),writeGDS(grounded,technology));
assert.throws(()=>createGeometry(kernel,10,'','P\nS\n80 80\n3\n8\n2\nM4\nM3\n100 100\n0\n'),/another line/);
assert.throws(()=>createGeometry(kernel,0,':'),/arguments/);
assert.throws(()=>createGeometry(kernel,1,'NAME=A:RAD=50:W=0:S=0:N=2:SIDES=1000000'),/arguments/);
// A late export-limit failure occurs after native allocation/construction.
// Appending it must roll back all new records and preserve the previous layout.
const preserved=createGeometry(kernel,2,'NAME=KEEP:LEN=20:WID=3:METAL=M1');
assert.throws(()=>createGeometry(kernel,1,'NAME=TOO_BIG:RAD=250:SIDES=256:W=.1:S=.1:N=64:METAL=M4','',{append:true}),/export size limit/);
const recovered=createGeometry(kernel,2,'NAME=NEXT:LEN=20:WID=3:METAL=M1','',{append:true});assert.equal(recovered.shapes.length,2);assert.deepEqual(recovered.shapes[1],preserved.shapes[0]);
const quoted=createGeometry(kernel,2,'NAME=A"B:LEN=20:WID=3:METAL=M1');assert.equal(quoted.shapes[0].name,'A"B');assert.equal(JSON.parse(kernel.UTF8ToString(kernel._analyze(1e9))).windings[0].name,'A"B');assert.equal(JSON.parse(kernel.UTF8ToString(kernel._simulate(0,1e9,1000,1000,1000,1000,8))).name,'A"B');
for(let i=0;i<30;i++){assert.throws(()=>createGeometry(kernel,0,'NAME=X:LEN=20:W=3:S=2:N=64'),/./);createGeometry(kernel,2,'NAME=A:LEN=20:WID=3:METAL=M1');}
mkdirSync(new URL('../build/evidence/',import.meta.url),{recursive:true});writeFileSync(new URL('../build/evidence/native-creators.json',import.meta.url),JSON.stringify({families:report,exact_fields:exact,original_printed_fields:printed},null,2)+'\n');
console.log(`${report.length} original creator families; ${exact} native numeric fields exact; ${printed} original printed fields.`);
