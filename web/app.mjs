import {polygons,writeGDS} from './gds.mjs';
import {parseTechnology} from './technology.mjs';
import {frequencySweep,matrixCSV,sweepCSV,plotSeries} from './simulation.mjs';
const $=id=>document.getElementById(id),svgNS='http://www.w3.org/2000/svg';
const colors=['#d8ba69','#8ab9df','#b89bde','#b4e887','#e1b486','#cf83a6','#80c9c3','#ece7bb'];
let importedText='',technologyText='',layout,technology,ready=false,busy=false,request=0,view,initialView,drag;
let samples=[],selectedSimulation=null,sampleRequest=null,sampleKey='';
const worker=new Worker(new URL('./worker.mjs',import.meta.url),{type:'module'});
const simulationControls=['simulate','sweep','sim-winding','cell-length','cell-width','cell-thickness','cell-trigger','filament-limit','sweep-start','sweep-stop','sweep-count','frequency-scale','frequency'];
function setBusy(value,stoppable=false){
  busy=value;for(const element of $('controls').elements)element.disabled=!ready||value;
  for(const id of simulationControls)$(id).disabled=!ready||!layout||value;
  $('analyze').disabled=!ready||!layout||value;$('stop').disabled=!value||!stoppable;
  $('matrix-csv').disabled=value||!selectedSimulation?.matrix;$('results-csv').disabled=value||!samples.length;
}
function failure(message){$('status').textContent=message;$('loading').textContent='';setBusy(false);}
worker.onerror=event=>{ready=false;$('engine').textContent='Kernel unavailable';failure(event.message||'Unable to load the WebAssembly kernel.');};
worker.onmessage=({data})=>{
  if(data.ready){ready=true;$('engine').textContent='● C++ / WASM ready';setBusy(false);if(technologyText)generate();return;}
  if(data.id!==request)return;
  if(data.cancelled){setBusy(false);$('status').textContent=`Sweep stopped. ${samples.length} completed samples retained.`;return;}
  if(data.error){if(data.action==='generate'){layout=undefined;view=initialView=undefined;$('download').disabled=true;$('preview').replaceChildren();for(const id of ['footprint','segments','resistance'])$(id).textContent='—';clearSimulation();}failure(data.error);return;}
  if(data.simulation){
    const result=data.simulation,summary={...result};delete summary.matrix;
    const index=samples.findIndex(s=>s.frequency_hz===result.frequency_hz);if(index<0)samples.push(summary);else samples[index]=summary;
    samples.sort((a,b)=>a.frequency_hz-b.frequency_hz);selectedSimulation=result;renderSimulation();renderPlot();
    const finished=!data.sweep||data.sweep.done;setBusy(!finished,!finished);
    const diagnostics=(data.warnings||'').split('\n').map(line=>line.trim()).filter(line=>line && line!=='Inverting matrix...').join('\n');
    $('status').textContent=(finished?`Original LMAT solve complete. ${result.filaments} filaments; substrate eddy currents disabled.`:`Solved ${data.sweep.index+1}/${data.sweep.total} samples · ${result.filaments} filaments.`)+(diagnostics?'\n'+diagnostics:'');
    return;
  }
  setBusy(false);$('loading').textContent='';
  if(data.analysis){$('electrical').textContent=data.analysis.windings.map(w=>`${w.name} · L = ${w.inductance_nh.toPrecision(7)} nH · R(${(data.analysis.frequency_hz/1e9).toPrecision(4)} GHz) = ${w.resistance_hf.toPrecision(7)} Ω`).join('\n');$('status').textContent=data.warnings||'Original conductor estimates calculated. These do not include the distributed substrate network.';return;}
  layout=data.layout;technology=data.technology;$('engine').textContent='● C++ / WASM ready';render();clearSimulation();
  $('sim-winding').replaceChildren();layout.shapes.forEach((shape,i)=>{const option=document.createElement('option');option.value=i;option.textContent=shape.name;$('sim-winding').append(option);});
  const primary=layout.shapes.findIndex(s=>s.name==='P');if(primary>=0)$('sim-winding').value=primary;
  $('download').disabled=false;setBusy(false);$('electrical').textContent='Choose a frequency and calculate the conductor estimates.';
  $('status').textContent=layout.warnings||'Layout generated. GDS export uses a 1 nm grid.';
};
function parameters(){return ['length','height','width','spacing','turns','radius','sides','metal','exit','angle','inner','secondary'].map(id=>Number($(id).value));}
function options(){return {nativeInput:true,x:Number($('origin-x').value),y:Number($('origin-y').value),phase:Number($('phase').value),grid:Number($('snap-grid').value),snap:$('snap').checked};}
function frequency(){const value=Number($('frequency').value);if(!Number.isFinite(value)||value<=0)throw Error('Enter a positive frequency.');return value;}
function send(data,stoppable=false){setBusy(true,stoppable);worker.postMessage({id:++request,...data});}
$('analyze').onclick=()=>{try{if(layout){$('status').textContent='Calculating original conductor estimates…';send({action:'analyze',frequency_ghz:frequency()});}}catch(e){failure(e.message);}};
function generate(){if(!ready||!technologyText)return;$('download').disabled=true;$('loading').textContent='Generating geometry…';send({kind:Number($('kind').value),parameters:parameters(),options:options(),technology:technologyText});}
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
  initialView=[x0-padding,-y1-padding,w+padding*2,h+padding*2];view=[...initialView];$('preview').replaceChildren();
  for(const polygon of list){const p=document.createElementNS(svgNS,'polygon');p.setAttribute('points',polygon.points.map(([x,y])=>`${x},${-y}`).join(' '));p.setAttribute('fill',colors[(polygon.layer-1)%colors.length]);p.setAttribute('fill-opacity','.77');p.setAttribute('stroke',colors[(polygon.layer-1)%colors.length]);p.setAttribute('stroke-width','.15');$('preview').append(p);}setView();
  $('layout-title').textContent=$('kind').selectedOptions[0].textContent.toUpperCase();$('footprint').textContent=`${w.toFixed(1)} × ${h.toFixed(1)} µm`;$('segments').textContent=layout.shapes.reduce((n,s)=>n+s.segments.length,0);$('resistance').textContent=layout.shapes.map(s=>`${s.name}: ${s.resistance_dc.toFixed(3)} Ω`).join(' · ');
  $('legend').replaceChildren();for(const layer of [...new Set(list.map(p=>p.layer))].sort((a,b)=>a-b)){const el=document.createElement('span');el.style.setProperty('--color',colors[(layer-1)%colors.length]);el.textContent=layer>=100?technology.vias[layer-100].name:technology.metals[layer-1].name;$('legend').append(el);}
}
function clearSimulation(){samples=[];selectedSimulation=null;sampleRequest=null;sampleKey='';for(const id of ['sim-l','sim-r','sim-q'])$(id).textContent='—';$('sim-details').textContent='Choose a winding and mesh, then run the original filament solver.';renderPlot();}
function simulationRequest(){const o=options();return {winding:Number($('sim-winding').value),mesh:{length:Number($('cell-length').value),width:Number($('cell-width').value),thickness:Number($('cell-thickness').value),subdivision_trigger:Number($('cell-trigger').value)},filament_limit:Number($('filament-limit').value),nativeInput:true,grid:o.grid,snap:o.snap};}
function prepareSamples(config,clear=false){const key=JSON.stringify(config);if(clear||key!==sampleKey){samples=[];selectedSimulation=null;renderPlot();}sampleRequest=config;sampleKey=key;}
$('simulate').onclick=()=>{try{const config=simulationRequest();prepareSamples(config);const ghz=frequency();$('status').textContent='Assembling and solving the original filament matrix…';send({action:'simulate',...config,frequency_ghz:ghz});}catch(e){failure(e.message);}};
$('sweep').onclick=()=>{try{const frequencies=frequencySweep(Number($('sweep-start').value),Number($('sweep-stop').value),Number($('sweep-count').value),$('frequency-scale').value==='log'),config=simulationRequest();prepareSamples(config,true);$('status').textContent=`Starting ${frequencies.length} original filament solves…`;send({action:'sweep',...config,frequencies_ghz:frequencies},true);}catch(e){failure(e.message);}};
$('stop').onclick=()=>{worker.postMessage({id:++request,action:'cancel'});$('stop').disabled=true;$('status').textContent='Stopping after the current sample…';};
function renderSimulation(){const s=selectedSimulation;if(!s)return;$('sim-l').textContent=(s.inductance_h*1e9).toPrecision(7)+' nH';$('sim-r').textContent=s.resistance.toPrecision(7)+' Ω';$('sim-q').textContent=s.series_q===null?'Undefined':s.series_q.toPrecision(7);$('sim-details').textContent=`${s.name} · ${(s.frequency_hz/1e9).toPrecision(5)} GHz · ${s.conductors} conductor groups · ${s.filaments} filaments · ${s.mesh.length}/${s.mesh.width}/${s.mesh.thickness} µm cells`;$('matrix-csv').disabled=busy||!s.matrix;}
function svgElement(tag,attributes={},text){const node=document.createElementNS(svgNS,tag);for(const [key,value]of Object.entries(attributes))node.setAttribute(key,value);if(text!==undefined)node.textContent=text;return node;}
function selectSample(sample){if(busy||!sampleRequest)return;$('status').textContent='Recomputing the selected sample and its matrix…';send({action:'simulate',...sampleRequest,frequency:sample.frequency_hz});}
function renderPlot(){
  const svg=$('frequency-plot');svg.replaceChildren();const key=$('plot-metric').value,logarithmic=$('frequency-scale').value==='log',series=plotSeries(samples,key,logarithmic);
  if(!series){svg.append(svgElement('text',{x:310,y:120,'text-anchor':'middle',class:'plot-empty'},'Run a solve or sweep to inspect L, R and series Q.'));return;}
  const x=p=>70+p*525,y=p=>210-p*185,scale=key==='inductance_h'?1e9:1;
  for(let i=0;i<=4;i++){const fraction=i/4,value=series.yRange[0]+fraction*(series.yRange[1]-series.yRange[0]);svg.append(svgElement('line',{x1:70,y1:y(fraction),x2:595,y2:y(fraction),stroke:'#304045','stroke-width':1}));svg.append(svgElement('text',{x:60,y:y(fraction)+3,'text-anchor':'end'},(value*scale).toPrecision(3)));}
  for(let i=0;i<=4;i++){const fraction=i/4,[a,b]=series.xRange,value=logarithmic?a*(b/a)**fraction:a+(b-a)*fraction;svg.append(svgElement('text',{x:x(fraction),y:230,'text-anchor':'middle'},(value/1e9).toPrecision(3)));}
  svg.append(svgElement('text',{x:330,y:248,'text-anchor':'middle'},'Frequency · GHz'));
  svg.append(svgElement('path',{d:series.points.map((p,i)=>`${i?'L':'M'}${x(p.x)},${y(p.y)}`).join(' '),fill:'none',stroke:'#c8ee8b','stroke-width':2}));
  for(const point of series.points){const selected=point.sample.frequency_hz===selectedSimulation?.frequency_hz,node=svgElement('circle',{cx:x(point.x),cy:y(point.y),r:selected?5:4,fill:selected?'#fff':'#c8ee8b',stroke:'#10191c','stroke-width':1,tabindex:0,role:'button','aria-label':`Inspect ${(point.sample.frequency_hz/1e9).toPrecision(5)} GHz sample`});node.onclick=()=>selectSample(point.sample);node.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();selectSample(point.sample);}};svg.append(node);}
}
$('plot-metric').onchange=renderPlot;$('frequency-scale').onchange=renderPlot;
$('preview').addEventListener('wheel',e=>{if(!view)return;e.preventDefault();const f=e.deltaY>0?1.15:1/1.15,cx=view[0]+view[2]/2,cy=view[1]+view[3]/2;view=[cx-view[2]*f/2,cy-view[3]*f/2,view[2]*f,view[3]*f];setView();},{passive:false});
$('preview').onpointerdown=e=>{if(!view)return;drag={x:e.clientX,y:e.clientY,view:[...view]};$('preview').setPointerCapture(e.pointerId);};
$('preview').onpointermove=e=>{if(!drag)return;const rect=$('preview').getBoundingClientRect(),scale=Math.max(drag.view[2]/rect.width,drag.view[3]/rect.height);view=[drag.view[0]-(e.clientX-drag.x)*scale,drag.view[1]-(e.clientY-drag.y)*scale,drag.view[2],drag.view[3]];setView();};
$('preview').onpointerup=$('preview').onpointercancel=()=>drag=null;
function download(data,name,type){const url=URL.createObjectURL(new Blob([data],{type})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
$('download').onclick=()=>{try{download(writeGDS(layout,technology),'reASITIC-coil.gds','application/octet-stream');}catch(e){failure(e.message);}};
$('matrix-csv').onclick=()=>{try{download(matrixCSV(selectedSimulation),`reASITIC-${selectedSimulation.name}-matrix.csv`,'text/csv');}catch(e){failure(e.message);}};
$('results-csv').onclick=()=>{try{download(sweepCSV(samples),'reASITIC-frequency-results.csv','text/csv');}catch(e){failure(e.message);}};
renderPlot();setBusy(false);loadProcess();
