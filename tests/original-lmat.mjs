import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from '../web/technology.mjs';
const fixture=JSON.parse(readFileSync('tests/fixtures/original-lmat.json','utf8'));
const text=readFileSync('web/technology/CMOS.tek','utf8');assert.equal(createHash('sha256').update(text).digest('hex'),fixture.technology_sha256);
const technology=parseTechnology(text),kernel=await createKernel();configureKernel(kernel,technology);mkdirSync('build/evidence',{recursive:true});
const printed=value=>(Object.is(value,-0)?'-0.00000000000000000000':value.toFixed(20));
const metal=name=>technology.metals.findIndex(m=>m.name===name);
let geometryFields=0,geometryDifferences=0,matrixFields=0,matrixDifferences=0,maxAbsolute=0;const report=[];
for(const item of fixture.cases) {
    const p=item.parameters;
    const parameters=[p.length??100,p.height??p.length??100,p.width,item.kind===5?Math.fround(p.gap*Math.PI/180)*180/Math.PI:(p.spacing??2),p.turns??1,p.radius??50,p.sides??8,metal(p.metal),metal(p.exit_metal??p.bottom_metal??p.secondary_metal),Math.fround((p.angle||0)*Math.PI/180),p.inner_gap??15,1];
    const layout=generateGeometry(kernel,item.kind,parameters,item.options).shapes[0];assert.equal(layout.segments.length,item.geometry.segments.length,item.name);
    let gd=0;
    function geometry(a,b,path) { geometryFields++;if(!Object.is(a,b)){geometryDifferences++;gd++;assert.ok(Math.abs(a-b)<=1e-12,`${item.name} ${path}: ${a} vs ${b}`);} }
    layout.origin.forEach((x,i)=>geometry(item.geometry.origin[i],x,'origin'));
    layout.segments.forEach((s,i)=>{
        const expected=item.geometry.segments[i];assert.equal(s.metal,expected.metal,item.name+' metal');
        geometry(expected.width,s.width,'width');
        if(expected.thickness!==undefined)geometry(expected.thickness,s.thickness,'thickness');
        if(expected.nx!==undefined){assert.equal(s.nx,expected.nx);assert.equal(s.ny,expected.ny);}
        s.points.forEach((point,j)=>point.forEach((value,k)=>geometry(expected.points[j][k],value,`segment[${i}].point[${j}][${k}]`)));
    });
    for(const mesh of item.matrices) {
        const actual=JSON.parse(kernel.UTF8ToString(kernel._simulate(0,mesh.frequency_hz,...mesh.cells,1024)));
        assert.ok(!actual.error,`${item.name}: ${actual.error}`);assert.equal(actual.filaments,mesh.filaments);assert.equal(actual.matrix.length,mesh.matrix.length);
        let differences=0;
        actual.matrix.forEach((row,i)=>row.forEach((z,j)=>z.forEach((value,k)=>{
            const expected=mesh.matrix[i][j][k];matrixFields++;
            // Compare at the original output boundary: native LMAT uses 20
            // digits after the decimal, while raw BSAVE geometry is binary64.
            if(printed(value)!==printed(expected)) {
                differences++;matrixDifferences++;const delta=Math.abs(value-expected);maxAbsolute=Math.max(maxAbsolute,delta);
                assert.ok(delta<=1e-12+1e-10*Math.abs(expected),`${item.name}-${mesh.name} [${i}][${j}][${k}]: ${expected} vs ${value}`);
            }
        })));
        report.push({name:item.name+'-'+mesh.name,geometryDifferences:gd,filaments:actual.filaments,matrixDifferences:differences});
        console.log(`${item.name}-${mesh.name}: ${gd} raw geometry differences, ${differences} printed matrix differences`);
    }
}
writeFileSync('build/evidence/original-lmat-parity.json',JSON.stringify({geometryFields,geometryDifferences,matrixFields,matrixDifferences,maxAbsolute,cases:report},null,2)+'\n');
console.log(`Original executable: ${geometryFields} binary64 geometry fields (${geometryDifferences} differences), ${matrixFields} printed matrix components (${matrixDifferences} differences), max absolute ${maxAbsolute}`);
assert.equal(geometryDifferences,0,'Original physical geometry differs');assert.equal(matrixDifferences,0,'Original printed LMAT output differs');
