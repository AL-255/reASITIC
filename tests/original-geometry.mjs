import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
const fixture=JSON.parse(readFileSync(new URL('./fixtures/original-creators.json',import.meta.url),'utf8'));
const kernel=await createKernel(),technology=parseTechnology(readFileSync(new URL('../web/technology/CMOS.tek',import.meta.url),'utf8'));configureKernel(kernel,technology);
const kinds={square:0,polygon:1,wire:2,symmetric_square:3,symmetric_polygon:4,ring:5,multimetal_square:8};
const keys={AX:[2,0],AY:[2,1],BX:[3,0],BY:[3,1],CX:[4,0],CY:[4,1],DX:[5,0],DY:[5,1],PORT1X:[0,0],PORT1Y:[0,1],PORT2X:[1,0],PORT2Y:[1,1]};
let checks=0,cases=0;
for(const item of fixture.fixtures){
  if(!(item.family in kinds))continue;
  for(const expected of item.shapes){const f=expected.fields,kind=kinds[item.family];
    const metal=technology.metals.findIndex(m=>m.name===f.METAL),exit=technology.metals.findIndex(m=>m.name===f.EXIT);
    let spacing=+f.S;if(kind===5){const gap=+item.script.match(/GAP=([^:\n]+)/)[1];spacing=Math.fround(gap*Math.PI/180)*180/Math.PI;}
    const p=[+f.LEN,+f.WID,+f.W,spacing,kind===5?1:+f.N,+f.RAD,Math.max(3,+f.SIDES),metal,exit,+f.ORIENT,kind===3 || kind===4?+f.WID:15,1];
    // Descriptor-only dimensions need valid unused values for wire/symmetric shapes.
    if(kind===3 || kind===4)p[1]=90;
    const actual=generateGeometry(kernel,kind,p).shapes[0];assert.equal(actual.segments.length,expected.segments.length,item.family);
    actual.segments.forEach((s,i)=>{const e=expected.segments[i];assert.equal([...technology.metals,...technology.vias][s.metal].name,e.METAL);for(const [key,[point,axis]] of Object.entries(keys)){assert.ok(Math.abs(s.points[point][axis]-e[key])<=5.1e-7,`${item.family}[${i}].${key}: ${s.points[point][axis]} vs ${e[key]}`);checks++;}});
    const dc=item.resistance_dc[f.NAME];if(dc!==undefined){assert.ok(Math.abs(actual.resistance_dc-dc)<=5.1e-7,`${item.family}: resistance`);checks++;}
    cases++;console.log(`${item.family}: matches original SAVE coordinates at printed precision`);
  }
}
assert.ok(cases>=7);console.log(`${cases} original-executable fixtures, ${checks} coordinate/resistance checks. Printed SAVE precision is not a bitwise boundary.`);
