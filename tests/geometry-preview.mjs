import assert from 'node:assert/strict';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import make from '../dist/asitic.js';
import {parseTechnology,configureKernel,createGeometry} from '../web/technology.mjs';
import {geometryPrimitives,arrayRectangle,polygons,gdsSize} from '../web/gds.mjs';
import {renderGeometryPreview} from '../web/geometry-preview.mjs';
const colors=['#d8ba69','#8ab9df','#b89bde','#b4e887','#e1b486','#cf83a6','#80c9c3','#ece7bb'];
// Exercise the actual DOM-producing renderer through its small DOM interface.
// The same nodes are serialized below for independent SVG raster inspection.
const document={createElementNS(ns,tag){assert.equal(ns,'http://www.w3.org/2000/svg');return new Element(tag);}};
class Element{constructor(tag){this.tag=tag;this.attrs={};this.children=[];this.ownerDocument=document;}setAttribute(key,value){this.attrs[key]=String(value);}append(...nodes){this.children.push(...nodes);}replaceChildren(...nodes){this.children=[...nodes];}}
const escape=x=>String(x).replaceAll('&','&amp;').replaceAll('"','&quot;').replaceAll('<','&lt;');
const xml=n=>`<${n.tag}${Object.entries(n.attrs).map(([k,v])=>` ${k}="${escape(v)}"`).join('')}>${n.children.map(xml).join('')}</${n.tag}>`;
const nodes=n=>1+n.children.reduce((sum,c)=>sum+nodes(c),0),near=(a,b,label)=>assert.ok(Math.abs(a-b)<=1e-9,label+': '+a+' vs '+b);
const cases=JSON.parse(readFileSync('tests/fixtures/original-creator-counts.json')).cases,reports=[];
mkdirSync('build/evidence/geometry-preview',{recursive:true});
for(const c of cases){
  const technology=parseTechnology(readFileSync(`web/technology/${c.technology}.tek`,'utf8')),layout={shapes:c.shapes},svg=new Element('svg'),stats=renderGeometryPreview(svg,layout,technology,{colors});
  const expected=[Infinity,Infinity,-Infinity,-Infinity];for(const p of polygons(layout,technology,{maxBytes:512*1024*1024}))for(const[x,y]of p.points){expected[0]=Math.min(expected[0],x);expected[1]=Math.min(expected[1],y);expected[2]=Math.max(expected[2],x);expected[3]=Math.max(expected[3],y);}
  stats.bounds.forEach((v,i)=>near(v,expected[i],c.name+' native polygon bounds'));
  const arrays=[...geometryPrimitives(layout,technology)].filter(p=>p.kind==='via-array'&&p.nx*p.ny>64),groups=svg.children.filter(n=>n.tag==='g');assert.equal(groups.length,arrays.length);
  for(const [i,a]of arrays.entries()){
    const matrix=groups[i].attrs.transform.slice(7,-1).split(' ').map(Number),pattern=svg.children[0].children[i];
    const scale=Number(pattern.attrs.patternTransform.slice(6,-1));near(Number(pattern.attrs.width)*scale,a.pitch,c.name+' normalized pattern pitch');near(Number(pattern.children[0].attrs.width)*scale,a.width,c.name+' normalized cut width');
    for(const [x,y]of [[0,0],[a.nx-1,0],[0,a.ny-1],[a.nx-1,a.ny-1],[Math.floor(a.nx/2),Math.floor(a.ny/2)]]){
      const actual=arrayRectangle(a,x*a.pitch,y*a.pitch,x*a.pitch+a.width,y*a.pitch+a.width);
      for(const [j,[u,v]]of [[x*a.pitch,y*a.pitch],[x*a.pitch+a.width,y*a.pitch],[x*a.pitch+a.width,y*a.pitch+a.width],[x*a.pitch,y*a.pitch+a.width]].entries()){
        near(matrix[0]*u+matrix[2]*v+matrix[4],actual[j][0],c.name+' SVG cut x');near(-(matrix[1]*u+matrix[3]*v+matrix[5]),actual[j][1],c.name+' SVG cut y');
      }
    }
  }
  if(c.name==='cmos-via-matrix'||c.name==='cmos-via-rotated'){
    const [x0,y0,x1,y1]=stats.bounds,pad=Math.max(x1-x0,y1-y0)*.07;
    Object.assign(svg.attrs,{xmlns:'http://www.w3.org/2000/svg',width:700,height:700,viewBox:[x0-pad,-y1-pad,x1-x0+2*pad,y1-y0+2*pad].join(' ')});
    writeFileSync(`build/evidence/geometry-preview/${c.name}.svg`,xml(svg));
    const a=arrays[0],corners=arrayRectangle(a,0,0,Math.min(a.nx-1,4)*a.pitch+a.width,Math.min(a.ny-1,4)*a.pitch+a.width),xs=corners.map(p=>p[0]),ys=corners.map(p=>p[1]);
    svg.attrs.viewBox=[Math.min(...xs)-.5,-Math.max(...ys)-.5,Math.max(...xs)-Math.min(...xs)+1,Math.max(...ys)-Math.min(...ys)+1].join(' ');
    writeFileSync(`build/evidence/geometry-preview/${c.name}-zoom.svg`,xml(svg));
  }
  reports.push({name:c.name,domNodes:nodes(svg),patterns:stats.arrays,cuts:stats.cuts});
}
// Small grids keep the former polygon positions, order and styling exactly.
for(const prefix of ['creator-coverage','creator-ranges','geometry-edits'])for(const c of JSON.parse(readFileSync(`tests/fixtures/original-${prefix}.json`)).cases){
  const technology=parseTechnology(readFileSync(`web/technology/${c.technology??'CMOS'}.tek`,'utf8')),layout={shapes:c.shapes},svg=new Element('svg');renderGeometryPreview(svg,layout,technology,{colors});
  const expected=[...polygons(layout,technology,{visibleOnly:true})];assert.equal(svg.children.length,expected.length,c.name+' former DOM count');
  svg.children.forEach((node,i)=>{assert.equal(node.tag,'polygon');assert.equal(node.attrs.points,expected[i].points.map(([x,y])=>`${x},${-y}`).join(' '));assert.equal(node.attrs.fill,colors[(expected[i].layer-1)%colors.length]);assert.equal(node.attrs['stroke-width'],'.15');assert.equal(node.attrs['fill-opacity'],'.77');});
}
const k=await make(),technology=parseTechnology(readFileSync('web/technology/CMOS.tek','utf8'));configureKernel(k,technology);
const huge=createGeometry(k,14,'NAME=A:NX=1000000:NY=2:VIA=VIA2:XORG=256:YORG=256'),svg=new Element('svg'),stats=renderGeometryPreview(svg,huge,technology,{colors});
assert.equal(stats.cuts,2000000);assert.equal(stats.arrays,1);assert.equal(nodes(svg),8,'Via preview expanded the grid into DOM nodes');assert.throws(()=>gdsSize(huge,technology),/output budget/);assert.equal(gdsSize(huge,technology,{maxBytes:128*1024*1024}).bytes,128000238);
const hidden=structuredClone(huge);hidden.shapes[0].hidden=true;assert.equal(renderGeometryPreview(svg,hidden,technology,{colors}).bounds,null);assert.equal(svg.children.length,0);
writeFileSync('build/evidence/geometry-preview.json',JSON.stringify({originalCases:reports,formerSmallPreviews:62,largeNativeGrid:{cuts:stats.cuts,domNodes:8},hiddenGeometry:true},null,2)+'\n');
console.log(`Geometry preview: ${reports.length} original layouts, 62 unchanged small previews, native bounds and rotated/reflected cut transforms; two million native cuts use 8 DOM nodes`);
