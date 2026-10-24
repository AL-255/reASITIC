// Reports from the pinned reconstructed command process, whose original-binary
// comparisons are documented in the source reference. Six printed digits only.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import createKernel from './reference-kernel.mjs';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
const kernel=await createKernel(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));configureKernel(kernel,technology);
const fixture=JSON.parse(readFileSync('tests/fixtures/connected-analysis.json','utf8'));
const kinds={wire:2,square:0,polygon:1,ring:5,multimetal:8,symmetric_square:3,symmetric_polygon:4};let checks=0,cases=0;
const metal=name=>technology.metals.findIndex(m=>m.name===name);
for(const item of fixture.cases){
  if(!(item.kind in kinds))continue;const p=item.parameters;
  const radians=Math.fround((p.angle||0)*Math.PI/180),spacing=item.kind==='ring'?Math.fround(p.gap*Math.PI/180)*180/Math.PI:(p.spacing??2);
  const parameters=[p.length??100,p.height??p.length??100,p.width,spacing,p.turns??1,p.radius??50,p.sides??8,metal(p.metal),metal(p.exit_metal??p.bottom_metal??p.secondary_metal),radians,p.inner_gap??15,1];
  const layout=generateGeometry(kernel,kinds[item.kind],parameters);
  for(const [frequency,resistance] of item.hf){
    const result=JSON.parse(kernel.UTF8ToString(kernel._analyze(frequency)));assert.ok(!result.error,result.error);const w=result.windings[0];
    const tolerance=.51*10**(Math.floor(Math.log10(Math.abs(item.inductance_nh)))-5);
    assert.ok(Math.abs(w.inductance_nh-item.inductance_nh)<=tolerance,`${item.kind}: L ${w.inductance_nh} vs ${item.inductance_nh}`);
    assert.ok(Math.abs(w.resistance_hf-resistance)<=5.1e-7,`${item.kind}: R ${w.resistance_hf} vs ${resistance}`);checks+=2;
  }
  assert.ok(Math.abs(layout.shapes[0].resistance_dc-item.dc)<=5.1e-7);checks++;cases++;
}
assert.equal(cases,11);console.log(`Connected command reports: ${cases} fixtures, ${checks} L / DC / R(f) comparisons at recorded print precision`);
