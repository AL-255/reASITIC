import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import createKernel from './reference-kernel.mjs';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
const kernel=await createKernel(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));configureKernel(kernel,technology);
mkdirSync('build/evidence',{recursive:true});
const fixture=JSON.parse(readFileSync('tests/fixtures/connected-analysis.json','utf8'));
const kinds={wire:2,square:0,polygon:1,ring:5,multimetal:8,symmetric_square:3,symmetric_polygon:4};
const metal=name=>technology.metals.findIndex(m=>m.name===name);
let fields=0,differences=0,maxRelative=0,maxAbsolute=0;const report=[];
for(const [index,item] of fixture.cases.entries()) {
  if(!(item.kind in kinds))continue;const p=item.parameters;
  const parameters=[p.length??100,p.height??p.length??100,p.width,item.kind==='ring'?Math.fround(p.gap*Math.PI/180)*180/Math.PI:(p.spacing??2),p.turns??1,p.radius??50,p.sides??8,metal(p.metal),metal(p.exit_metal??p.bottom_metal??p.secondary_metal),Math.fround((p.angle||0)*Math.PI/180),p.inner_gap??15,1];
  const options={x:p.x??0,y:p.y??0,phase:p.phase??1};
  generateGeometry(kernel,kinds[item.kind],parameters,options);
  for(const mesh of [
    {name:'coarse',frequency:1e9,cells:[1000,1000,1000,1000],expected:item.matrix_1ghz},
    {name:'fine',frequency:2e9,cells:[50,3,.5,50],expected:item.matrix_fine_2ghz},
    {name:'unequal',frequency:1e9,cells:[20,1000,1000,50],expected:item.matrix_unequal_1ghz},
  ]) {
    const name=`${index}-${item.kind}-${mesh.name}`;
    const native=spawnSync('build/native-geometry',[kinds[item.kind],...parameters,mesh.frequency,...mesh.cells,0,options.phase,options.x,options.y].map(String),{encoding:'utf8',timeout:30000});assert.equal(native.status,0,`${name}: ${native.stderr}`);
    const expected=JSON.parse(native.stdout),actual=JSON.parse(kernel.UTF8ToString(kernel._simulate(0,mesh.frequency,...mesh.cells,1024)));
    assert.ok(!expected.error,`${name} native: ${expected.error}`);assert.ok(!actual.error,`${name} WASM: ${actual.error}`);
    assert.equal(actual.filaments,expected.filaments,name+' filament count');assert.equal(actual.conductors,expected.conductors,name+' conductor count');
    let errors=0,fixtureRelative=0;
    function compare(a,b,path) {
      assert.ok(Number.isFinite(a) && Number.isFinite(b),`${name} ${path}`);fields++;
      if(!Object.is(a,b)) { const absolute=Math.abs(a-b),relative=absolute/Math.max(1e-30,Math.abs(a));differences++;errors++;maxRelative=Math.max(maxRelative,relative);maxAbsolute=Math.max(maxAbsolute,absolute);assert.ok(absolute<=1e-12+1e-10*Math.abs(a),`${name} ${path}: ${a} vs ${b}`); }
    }
    for(const key of ['resistance','reactance','inductance_h','series_q'])compare(expected[key],actual[key],key);
    assert.equal(actual.matrix.length,expected.matrix.length,name);
    actual.matrix.forEach((row,i)=>row.forEach((z,j)=>z.forEach((value,k)=>compare(expected.matrix[i][j][k],value,`matrix[${i}][${j}][${k}]`))));
    if(mesh.expected) {
      assert.equal(actual.matrix.length,mesh.expected.length,name+' connected command dimension');
      actual.matrix.forEach((row,i)=>row.forEach((z,j)=>z.forEach((value,k)=>{const a=mesh.expected[i][j][k],delta=Math.abs(a-value);fixtureRelative=Math.max(fixtureRelative,delta/Math.max(1e-30,Math.abs(a)));assert.ok(delta<=1e-11+1e-9*Math.abs(a),`${name} connected command [${i}][${j}][${k}]: ${a} vs ${value}`);}))); 
    }
    report.push({name,filaments:actual.filaments,conductors:actual.conductors,differences:errors,fixtureRelative});
    writeFileSync(`build/evidence/lmat-${name}.json`,JSON.stringify(actual));console.log(`${name}: ${actual.conductors} conductors, ${actual.filaments} filaments, ${errors} differing fields`);
  }
}
assert.equal(report.length,33);writeFileSync('build/evidence/lmat-parity.json',JSON.stringify({fields,differences,maxRelative,maxAbsolute,cases:report},null,2)+'\n');
console.log(`LMAT: ${report.length} cases, ${fields} fields; ${differences} differences; max relative ${maxRelative}, absolute ${maxAbsolute}`);
assert.equal(differences,0,'Original LMAT parity regressed');
