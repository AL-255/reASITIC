import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {parseCreatorCommand} from '../web/creators.mjs';
const fixture=JSON.parse(readFileSync('tests/fixtures/original-creator-binary.json','utf8'));
const text=readFileSync('web/technology/CMOS.tek','utf8'),hash=value=>createHash('sha256').update(value).digest('hex');
assert.equal(hash(text),fixture.technology_sha256);assert.equal(hash(readFileSync('tests/fixtures/original-creators.json')),fixture.case_definition_sha256);
const kernel=await createKernel();configureKernel(kernel,parseTechnology(text));let fields=0,differences=0;const report=[];
function exact(a,b,path){fields++;if(!Object.is(a,b)){differences++;throw Error(`${path}: ${a} vs original ${b}`);}}
for(const item of fixture.cases) {
  const recipe=parseCreatorCommand(item.command,item.answers),layout=createGeometry(kernel,recipe.id,recipe.keywords,recipe.answers);
  assert.equal(layout.shapes.length,Object.keys(item.shapes).length);let count=0;
  for(const actual of layout.shapes) {
    const expected=item.shapes[actual.name];assert.ok(expected,actual.name);assert.equal(actual.segments.length,expected.segments.length);
    actual.origin.forEach((x,i)=>exact(x,expected.origin[i],item.family+'.origin'));
    actual.segments.forEach((segment,i)=>{const e=expected.segments[i];assert.equal(segment.metal,e.metal);exact(segment.width,e.width,item.family+'.width');if(e.thickness!==undefined)exact(segment.thickness,e.thickness,item.family+'.thickness');if(e.nx!==undefined){assert.equal(segment.nx,e.nx);assert.equal(segment.ny,e.ny);}segment.points.forEach((point,j)=>point.forEach((x,k)=>exact(x,e.points[j][k],`${item.family}.${actual.name}[${i}].point[${j}][${k}]`)));});
    count+=actual.segments.length;
  }
  report.push({family:item.family,shapes:layout.shapes.length,segments:count});console.log(item.family+': all original raw physical binary64 fields exact');
}
mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/original-creator-binary-parity.json',JSON.stringify({fields,differences,cases:report},null,2)+'\n');
assert.equal(fixture.cases.length,12);assert.equal(differences,0);console.log(`${fixture.cases.length} original creator families: ${fields} physical binary64 fields, ${differences} differences.`);
