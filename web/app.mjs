import {polygons,writeGDS} from './gds.mjs';
import {parseTechnology} from './technology.mjs';
const $=id=>document.getElementById(id),svgNS='http://www.w3.org/2000/svg';
const colors=['#d8ba69','#8ab9df','#b89bde','#b4e887','#e1b486','#cf83a6','#80c9c3','#ece7bb'];
let importedText='',technologyText='',layout,technology,ready=false,request=0,view,initialView,drag;
const worker=new Worker(new URL('./worker.mjs',import.meta.url),{type:'module'});
function failure(message){$('status').textContent=message;$('loading').textContent='';$('engine').textContent='Kernel error';$('generate').disabled=!ready;}
worker.onerror=event=>failure(event.message||'Unable to load the WebAssembly kernel.');
worker.onmessage=({data})=>{
  if(data.ready){ready=true;$('engine').textContent='● C++ / WASM ready';$('generate').disabled=false;if(technologyText)generate();return;}
  if(data.id!==request)return;
  $('generate').disabled=false;$('analyze').disabled=!layout;$('loading').textContent='';
  if(data.analysis){$('electrical').textContent=data.analysis.windings.map(w=>`${w.name} · L = ${w.inductance_nh.toPrecision(7)} nH · R(${(data.analysis.frequency_hz/1e9).toPrecision(4)} GHz) = ${w.resistance_hf.toPrecision(7)} Ω`).join('\n');$('status').textContent=data.warnings||'Original conductor estimates calculated. These do not include the distributed substrate network.';return;}
  if(data.error){failure(data.error);return;}
  layout=data.layout;technology=data.technology;$('engine').textContent='● C++ / WASM ready';render();$('download').disabled=false;$('analyze').disabled=false;$('electrical').textContent='Choose a frequency and calculate the conductor estimates.';
  $('status').textContent=layout.warnings||'Layout generated. GDS export uses a 1 nm grid.';
};
function parameters(){return ['length','height','width','spacing','turns','radius','sides','metal','exit','angle','inner','secondary'].map(id=>Number($(id).value)*(id==='angle'?Math.PI/180:1));}
$('analyze').onclick=()=>{const frequency=Number($('frequency').value)*1e9;if(!Number.isFinite(frequency) || frequency<=0){$('status').textContent='Enter a positive frequency.';return;}if(!layout)return;$('analyze').disabled=true;$('generate').disabled=true;$('status').textContent='Calculating original conductor estimates…';worker.postMessage({id:++request,action:'analyze',frequency});};
function generate(){if(!ready || !technologyText)return;$('download').disabled=true;$('analyze').disabled=true;$('generate').disabled=true;$('loading').textContent='Generating geometry…';worker.postMessage({id:++request,kind:Number($('kind').value),parameters:parameters(),technology:technologyText});}
function updateMetals(text){const t=parseTechnology(text);for(const id of ['metal','exit']){$(id).replaceChildren();t.metals.forEach((m,i)=>{const o=document.createElement('option');o.value=i;o.textContent=m.name;$(id).append(o);});}const none=document.createElement('option');none.value=-1;none.textContent='None';$('exit').append(none);$('metal').value=Math.min(3,t.metals.length-1);$('exit').value=Math.min(2,t.metals.length-2);}
async function loadProcess(){try{if($('process').value==='custom'){updateMetals(importedText);technologyText=importedText;generate();return;}const response=await fetch(`technology/${$('process').value}.tek`);if(!response.ok)throw Error('Unable to load technology');const text=await response.text();updateMetals(text);technologyText=text;generate();}catch(e){failure(e.message);}}
$('process').onchange=loadProcess;
$('technology-file').onchange=async event=>{try{const file=event.target.files[0];if(!file)return;const text=await file.text();updateMetals(text);technologyText=importedText=text;let option=$('process').querySelector('[value="custom"]');if(!option){option=document.createElement('option');option.value='custom';$('process').append(option);}option.textContent=file.name;$('process').value='custom';generate();}catch(e){failure(e.message);}};
$('kind').onchange=()=>{const ring=$('kind').value==='5';$('spacing-label').firstChild.textContent=ring?'Gap · degrees':'Spacing';$('spacing').value=ring?20:2;};
$('controls').onsubmit=e=>{e.preventDefault();generate();};
function setView(){if(view)$('preview').setAttribute('viewBox',view.join(' '));}
$('fit').onclick=()=>{if(initialView){view=[...initialView];setView();}};
function render(){
  const list=[...polygons(layout,technology)],points=list.flatMap(p=>p.points);
  const xs=points.map(p=>p[0]),ys=points.map(p=>p[1]),x0=Math.min(...xs),x1=Math.max(...xs),y0=Math.min(...ys),y1=Math.max(...ys);
  const w=Math.max(1,x1-x0),h=Math.max(1,y1-y0),padding=Math.max(w,h)*.15;
  initialView=[x0-padding,-y1-padding,w+padding*2,h+padding*2];view=[...initialView];
  $('preview').replaceChildren();
  for(const polygon of list){const p=document.createElementNS(svgNS,'polygon');p.setAttribute('points',polygon.points.map(([x,y])=>`${x},${-y}`).join(' '));p.setAttribute('fill',colors[(polygon.layer-1)%colors.length]);p.setAttribute('fill-opacity','.77');p.setAttribute('stroke',colors[(polygon.layer-1)%colors.length]);p.setAttribute('stroke-width','.15');$('preview').append(p);}setView();
  $('layout-title').textContent=$('kind').selectedOptions[0].textContent.toUpperCase();$('footprint').textContent=`${w.toFixed(1)} × ${h.toFixed(1)} µm`;
  $('segments').textContent=layout.shapes.reduce((n,s)=>n+s.segments.length,0);
  $('resistance').textContent=layout.shapes.map(s=>`${s.name}: ${s.resistance_dc.toFixed(3)} Ω`).join(' · ');
  $('legend').replaceChildren();for(const layer of [...new Set(list.map(p=>p.layer))].sort((a,b)=>a-b)){const el=document.createElement('span');el.style.setProperty('--color',colors[(layer-1)%colors.length]);el.textContent=layer>=100?technology.vias[layer-100].name:technology.metals[layer-1].name;$('legend').append(el);}
}
$('preview').addEventListener('wheel',e=>{if(!view)return;e.preventDefault();const rect=$('preview').getBoundingClientRect(),f=e.deltaY>0?1.15:1/1.15;const cx=view[0]+view[2]/2,cy=view[1]+view[3]/2;view=[cx-view[2]*f/2,cy-view[3]*f/2,view[2]*f,view[3]*f];setView();},{passive:false});
$('preview').onpointerdown=e=>{if(!view)return;drag={x:e.clientX,y:e.clientY,view:[...view]};$('preview').setPointerCapture(e.pointerId);};
$('preview').onpointermove=e=>{if(!drag)return;const rect=$('preview').getBoundingClientRect(),scale=Math.max(drag.view[2]/rect.width,drag.view[3]/rect.height);view=[drag.view[0]-(e.clientX-drag.x)*scale,drag.view[1]-(e.clientY-drag.y)*scale,drag.view[2],drag.view[3]];setView();};
$('preview').onpointerup=$('preview').onpointercancel=()=>drag=null;
$('download').onclick=()=>{try{const bytes=writeGDS(layout,technology),url=URL.createObjectURL(new Blob([bytes],{type:'application/octet-stream'})),a=document.createElement('a');a.href=url;a.download='reASITIC-coil.gds';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}catch(e){failure(e.message);}};
loadProcess();
