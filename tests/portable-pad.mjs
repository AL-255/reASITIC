import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import createKernel from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';

// Production uses the mathematical formulation, so compare independent original
// reports at their published precision rather than a CPU's hidden low bits.
const kernel=await createKernel();
const technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));
const cases=JSON.parse(readFileSync('tests/fixtures/pad-cases.json','utf8')).cases;
const originals=JSON.parse(readFileSync('tests/fixtures/original-pad.json','utf8')).cases;
const reports=[];
for(const item of cases){
    configureKernel(kernel,technology);
    let layout;
    for(const [i,c] of [item.device,...item.pads].entries())layout=createGeometry(kernel,c.id,c.keywords,'',{append:i>0});
    let device=layout.shapes.findIndex(s=>s.name==='A'),pad1=layout.shapes.findIndex(s=>s.name==='P1'),pad2=layout.shapes.findIndex(s=>s.name==='P2'),ground=-1;
    if(item.ground?.startsWith('@'))ground=item.ground==='@1'?pad1:item.ground==='@2'?pad2:device;
    else if(item.ground){layout=createGeometry(kernel,2,item.ground,'',{append:true});++device;++pad1;++pad2;ground=0;}
    const frequency=kernel._normalize_value(item.frequency_ghz,5,.1,0);
    const cells=[...item.cells,...item.contact_cells].map(v=>kernel._normalize_value(v,7,.1,1));
    const actual=JSON.parse(kernel.UTF8ToString(kernel._simulate_pad_network(device,pad1,pad2,ground,frequency,...cells,512,512,...item.grid,512,1024,item.eddy_mode,item.layers.length,item.layers[0]??0,item.layers[1]??0,250000)));
    const reference=originals.find(c=>c.name===item.name),raw=Buffer.from(reference.raw_hex.matrix,'hex');
    if(Array.from({length:8},(_,i)=>raw.readDoubleLE(i*8)).some(v=>!Number.isFinite(v))){
        assert.match(actual.error,/nonfinite|finite stage/,item.name);reports.push({name:item.name,rejectedNonfinite:true});continue;
    }
    assert.ok(!actual.error,item.name+': '+actual.error);
    assert.equal(actual.groups,reference.groups,item.name+' groups');
    assert.equal(actual.green_table_hash,reference.green_table_hash,item.name+' independent Green tables');
    for(const key of ['capacitance_matrix','pad_capacitance_matrix']){
        assert.equal(actual[key].length,reference[key].length);
        actual[key].forEach((row,i)=>row.forEach((z,j)=>z.forEach((v,k)=>assert.equal(Number(v.toPrecision(10)),reference[key][i][j][k],item.name+' '+key))));
    }
    const p=actual.pi;
    [p.inductance_h/1e-9,p.resistance,p.capacitance_second_f/1e-15,p.resistance_second,p.capacitance_first_f/1e-15,p.resistance_first,actual.resonance_hz===null?null:actual.resonance_hz/1e9].forEach((v,i)=>assert.equal(v===null?NaN:Number(i===6?v.toFixed(2):v.toPrecision(3)),Number(reference.pi_printed[i]),item.name+' pi '+i));
    for(const [i,key] of ['quality_first','quality_second','quality_differential'].entries())assert.equal(Number(p[key].toFixed(3)),Number(reference.quality_printed[i]),item.name+' '+key);
    reports.push({name:item.name,pi:actual.pi});
}
mkdirSync('build/evidence',{recursive:true});
writeFileSync('build/evidence/portable-pad.json',JSON.stringify({cases:reports.length,reports},null,2)+'\n');
console.log(`Production PI4: ${reports.length} original cases retain published model precision, independent capacitance and Green tables, and nonfinite rejection`);
