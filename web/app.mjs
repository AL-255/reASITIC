import {loadedPortCSV} from './loaded-port.mjs';
import {revealReload} from './startup.mjs';
import {optimizationCSV} from './optimization.mjs';
import {conductorEstimatesCSV} from './estimates.mjs';
import {resonanceHistoryCSV} from './self-resonance.mjs';
import {writeGDS,segmentRecords} from './gds.mjs';
import {renderGeometryPreview} from './geometry-preview.mjs';
import {parseTechnology} from './technology.mjs';
import {creatorRecipe,parseCreatorCommand} from './creators.mjs';
import {capacitanceMatrixCSV,capacitanceDiagnostics} from './capacitance.mjs';
import {validateThreePortBacking,threePortMetricsCSV} from './three-port.mjs';
import {transformerImpedanceCSV,transformerMetricsCSV} from './transformer.mjs';
import {networkMatrixCSV,networkMetricsCSV,networkVectorsCSV} from './network.mjs';
import {portSampleSummary,portSweepRecord,portSweepScatteringCSV,portSweepMetricsCSV,threePortSweepMatrixCSV} from './port-sweep.mjs';
import {frequencySweep,matrixCSV,sweepCSV,plotSeries} from './simulation.mjs';
const $=id=>document.getElementById(id),svgNS='http://www.w3.org/2000/svg';
const colors=['#d8ba69','#8ab9df','#b89bde','#b4e887','#e1b486','#cf83a6','#80c9c3','#ece7bb'];
let importedText='',technologyText='',layout,technology,ready=false,busy=false,request=0,view,initialView,drag;
let conductorAnalysis=null,loadedPortResult=null;
let samples=[],selectedSimulation=null,sampleRequest=null,sampleKey='',capacitanceResult=null,networkResult=null,transformerResult=null,threeResult=null,threeBacking=null;
const portSamples={network:[],transformer:[],'three-port':[]};let activePortSweep=null,activeSelfResonance=false,selfResonanceResult=null,selfResonanceSamples=[];
let activeOptimization=false,optimizationSamples=[];
const worker=new Worker(new URL('./worker.mjs',import.meta.url),{type:'module'});
const simulationControls=['loaded-source','loaded-port','loaded-real','loaded-imag','loaded-cap1','loaded-cap2','coupling-first','coupling-second','selfres-search','selfres-start','selfres-end','selfres-step','selfres-mode','selfres-budget','max-width-cells','cell-policy','cell-alpha','cell-beta','simulate','sweep','sim-winding','cell-length','cell-width','cell-thickness','cell-trigger','filament-limit','sweep-start','sweep-stop','sweep-count','frequency-scale','frequency','cap-solve','cap-winding','cap-ground','cap-frequency','cap-length','cap-width','cap-fftx','cap-ffty','contact-limit','cap-mode','eddy-mode','eddy-layer-count','eddy-layer-1','eddy-layer-2','green-budget','network-model','network-pad1','network-pad2','network-solve','network-sweep','network-start','network-end','network-step','trans-sweep','trans-start','trans-end','trans-step','network-parameter','network-format','trans-parameter','trans-format','trans-primary','trans-secondary','trans-termination','trans-solve','three-first','three-second','three-third','three-parameter','three-format','three-backing-file','three-solve','three-sweep','three-start','three-end','three-step'];
function setBusy(value,stoppable=false){
  busy=value;for(const element of $('controls').elements)element.disabled=!ready||value||Boolean(element.closest('label')?.hidden)||Boolean(element.closest('.fields') && $('native-command').value.trim())||Boolean(element.id==='append-layout' && !layout);
  for(const id of ['opt-search','opt-search-kind','opt-keywords','opt-limit'])$(id).disabled=!ready||value;
  $('opt-choice').disabled=value||!optimizationSamples.length;$('opt-stop').disabled=!value||!activeOptimization;$('opt-csv').disabled=value||!optimizationSamples.length;
  const selected=optimizationSamples.find(s=>s.index===Number($('opt-choice').value));$('opt-apply').disabled=value||!selected||!selected.geometry_valid;

  for(const id of simulationControls)$(id).disabled=!ready||!layout?.shapes.length||value;
  const automatic=$('cell-policy').value==='automatic';
  for(const id of ['cell-length','cell-width','cell-thickness','cell-trigger','cap-length','cap-width'])$(id).disabled=!ready||!layout?.shapes.length||value||automatic;
  for(const id of ['cell-alpha','cell-beta'])$(id).disabled=!ready||!layout?.shapes.length||value||!automatic;
  $('apply-edit').disabled=!ready||!layout?.shapes.length||value;
  $('coupling').disabled=!ready||(layout?.shapes.length??0)<2||value;
  $('analyze').disabled=!ready||!layout?.shapes.length||value;$('stop').disabled=!value||!stoppable||activePortSweep!==null||activeSelfResonance||activeOptimization;
  $('estimates-csv').disabled=value||!conductorAnalysis;
  $('selfres-stop').disabled=!value||!stoppable||!activeSelfResonance;$('selfres-csv').disabled=value||!selfResonanceSamples.length;
  $('network-sweep').disabled=!ready||!layout?.shapes.length||value||$('network-model').value==='PI3';
  $('network-stop').disabled=!value||!stoppable||activePortSweep!=='network';$('trans-stop').disabled=!value||!stoppable||activePortSweep!=='transformer';$('three-stop').disabled=!value||!stoppable||activePortSweep!=='three-port';
  $('network-sweep-csv').disabled=value||!portSamples.network.length;$('trans-sweep-csv').disabled=value||!portSamples.transformer.length;$('three-sweep-csv').disabled=value||!portSamples['three-port'].length;
  $('cap-matrix-csv').disabled=value||!capacitanceResult;$('potential-matrix-csv').disabled=value||!capacitanceResult;
  for(const id of ['three-record','three-raw-csv','three-metrics-csv'])$(id).disabled=value||!threeResult;$('three-clear-backing').disabled=value||!threeBacking;
  for(const id of ['trans-y-csv','trans-z-csv','trans-metrics-csv','trans-record','trans-s-csv'])$(id).disabled=value||!transformerResult;
  for(const id of ['network-record','network-s-csv'])$(id).disabled=value||!networkResult;
  $('network-matrix-csv').disabled=value||!networkResult;$('network-metrics-csv').disabled=value||!networkResult;$('network-vectors-csv').disabled=value||!networkResult?.currents;
  $('loaded-calculate').disabled=!ready||value||!($('loaded-source').value==='network'?networkResult:transformerResult);$('loaded-csv').disabled=value||!loadedPortResult;
  $('matrix-csv').disabled=value||!selectedSimulation?.matrix;$('results-csv').disabled=value||!samples.length;
}
function failure(message){if(activeOptimization){activeOptimization=false;$('opt-state').textContent=`Search stopped after ${optimizationSamples.length} completed candidates. ${message}`;}if(activeSelfResonance)$('selfres-state').textContent=`Search stopped after an error. ${selfResonanceSamples.length} analyses retained.`;activePortSweep=null;activeSelfResonance=false;$('status').textContent=message;$('loading').textContent='';setBusy(false);}
worker.onerror=event=>{revealReload(document,location.href);ready=false;$('engine').textContent='Kernel unavailable';failure(event.message||'Unable to load the WebAssembly kernel.');};
worker.onmessage=({data})=>{
  if(data.ready){ready=true;$('engine').textContent='● C++ / WASM ready';setBusy(false);if(technologyText)generate();return;}
  if(data.id!==request)return;
  if(data.cancelled){if(activeOptimization){activeOptimization=false;setBusy(false);$('opt-state').textContent=`Search stopped after ${optimizationSamples.length} completed candidates.`;$('status').textContent='Coil-design search stopped.';return;}const search=activeSelfResonance,count=search?selfResonanceSamples.length:activePortSweep?portSamples[activePortSweep].length:samples.length;activePortSweep=null;activeSelfResonance=false;setBusy(false);$('status').textContent=`${search?'Search':'Sweep'} stopped. ${count} completed samples retained.`;if(search)$('selfres-state').textContent=`Search stopped after ${count} analyses.`;return;}
  if(data.error){if(data.action==='generate' && !data.append){layout=undefined;view=initialView=undefined;$('download').disabled=true;$('preview').replaceChildren();for(const id of ['footprint','segments','resistance'])$(id).textContent='—';clearEstimates();clearSimulation();clearCapacitance();clearNetwork();clearTransformer();clearThreePort();clearSelfResonance();}failure(data.error);return;}
  if(data.resonance){
    selfResonanceResult=data.resonance;selfResonanceSamples.push(data.resonance.sample);activeSelfResonance=!data.resonance.done;renderSelfResonance();setBusy(activeSelfResonance,activeSelfResonance);$('status').textContent=data.resonance.done?'SELFRES search finished.':'Searching with the original SELFRES algorithm…';return;
  }
  if(data.three_port){
    if(data.port_sweep)portSamples['three-port'].push(portSampleSummary(data.three_port));else portSamples['three-port']=[];
    const finished=!data.port_sweep||data.port_sweep.done;if(finished)activePortSweep=null;
    $('three-sweep-state').textContent=portSamples['three-port'].length?`${portSamples['three-port'].length} completed port samples retained.`:'Single-frequency result.';
    threeResult=data.three_port;const t=threeResult,m=t.model,show=(x,scale=1,unit='')=>x===null?'Undefined':(x*scale).toPrecision(7)+(unit?' '+unit:'');setBusy(!finished,!finished);
    for(let i=0;i<3;++i)$('three-l'+(i+1)).textContent=show(m.inductance_h[i],1e9,'nH');
    $('three-details').textContent=`${t.names.join(' / ')} · ${show(t.frequency_hz,1e-9,'GHz')} · ${t.node_count} nodes · ${t.filaments} filaments\n`+
      `R1 / R2 / R3 = ${m.resistance_ohm.map(v=>show(v,1,'Ω')).join(' / ')}\n`+
      ['12','13','23'].map((pair,i)=>`M${pair} = ${show(m.mutual_inductance_h[i],1e9,'nH')} · k${pair} = ${show(m.coupling[i])} · Re(Z${pair}) = ${show(m.cross_resistance_ohm[i],1,'Ω')}`).join('\n')+`\n${t.eddy.evaluations} Green evaluations`+(t.backing?' · historical native-state comparison':'')+automaticMeshDetails(t);
    const diagnostics=capacitanceDiagnostics(data.warnings||''),quadErrors=t.eddy.quadrature_status.reduce((a,b)=>a+b,0);$('status').textContent=`Original 3PORT solve complete${quadErrors?' with QUADPACK nonconvergence':''}.`+(diagnostics?'\n'+diagnostics:'');if(!finished)$('status').textContent=`Solved three-port sample ${data.port_sweep.index+1}/${data.port_sweep.total} at ${(t.frequency_hz/1e9).toPrecision(7)} GHz.`+(diagnostics?'\n'+diagnostics:'');return;
  }
  if(data.loaded_port){loadedPortResult=data.loaded_port;const r=loadedPortResult;setBusy(false);$('loaded-result').textContent=r.status==='finite'?`Port ${r.port} · ${(r.frequency_hz/1e9).toPrecision(7)} GHz · Zin = ${r.impedance_ohm[0].toPrecision(9)} + j ${r.impedance_ohm[1].toPrecision(9)} Ω`:`Loaded impedance: ${r.status==='pole'?'circuit pole':r.status==='overflow'?'outside the numerical output range':'indeterminate circuit'}.`;$('status').textContent='Loaded two-port impedance calculated.';return;}
  if(data.transformer){
    clearLoadedPort();if(!networkResult)$('loaded-source').value='transformer';
    if(data.port_sweep)portSamples.transformer.push(portSampleSummary(data.transformer));else portSamples.transformer=[];
    const finished=!data.port_sweep||data.port_sweep.done;if(finished)activePortSweep=null;
    $('trans-sweep-state').textContent=portSamples.transformer.length?`${portSamples.transformer.length} completed port samples retained.`:'Single-frequency result.';
    transformerResult=data.transformer;const t=transformerResult,m=t.transformer,show=(x,scale=1,unit='')=>x===null?'Undefined':(x*scale).toPrecision(7)+(unit?' '+unit:'');setBusy(!finished,!finished);
    $('trans-l1').textContent=show(m.first_inductance_h,1e9,'nH');$('trans-l2').textContent=show(m.second_inductance_h,1e9,'nH');$('trans-k').textContent=show(m.coupling);
    $('trans-details').textContent=`${t.name} / ${t.secondary} · ${show(t.frequency_hz,1e-9,'GHz')} · ${t.contacts} contacts · ${t.groups} conductor groups · ${t.filaments} filaments\n`+
      `R1 = ${show(m.first_resistance_ohm,1,'Ω')} · R2 = ${show(m.second_resistance_ohm,1,'Ω')} · mutual inductance = ${show(m.mutual_inductance_h,1e9,'nH')}\n`+
      `Ground resistance = ${show(m.ground_resistance_ohm,1,'Ω')} · termination = ${show(t.termination_ohm,1,'Ω')} · ${t.eddy.evaluations} Green evaluations`+automaticMeshDetails(t);
    const diagnostics=capacitanceDiagnostics(data.warnings||''),quadErrors=t.eddy.quadrature_status.reduce((a,b)=>a+b,0);
    $('status').textContent=`Original CALCTRANS solve complete${quadErrors?' with QUADPACK nonconvergence':''}.`+(diagnostics?'\n'+diagnostics:'');if(!finished)$('status').textContent=`Solved port sample ${data.port_sweep.index+1}/${data.port_sweep.total} at ${(t.frequency_hz/1e9).toPrecision(7)} GHz.`+(diagnostics?'\n'+diagnostics:'');return;
  }
  if(data.network){
    clearLoadedPort();if(!transformerResult)$('loaded-source').value='network';
    if(data.port_sweep)portSamples.network.push(portSampleSummary(data.network));else portSamples.network=[];
    const finished=!data.port_sweep||data.port_sweep.done;if(finished)activePortSweep=null;
    $('network-sweep-state').textContent=portSamples.network.length?`${portSamples.network.length} completed port samples retained.`:'Single-frequency result.';
    networkResult=data.network;const n=networkResult,p=n.pi,show=(x,scale=1,unit='')=>x===null?'Undefined':(x*scale).toPrecision(7)+(unit?' '+unit:'');setBusy(!finished,!finished);
    const capacitive=n.model==='CAP';for(const option of $('network-parameter').options)if(option.value==='pi')option.disabled=capacitive;
    if(capacitive){
      if($('network-parameter').value==='pi')$('network-parameter').value='y';
      for(const id of ['network-l','network-r','network-q'])$(id).textContent='—';
      $('network-details').textContent=`${n.dispatch.command} · CAP · ${n.name} · ${show(n.frequency_hz,1e-9,'GHz')} · ${n.contacts} contacts\nThe original capacitor port path provides S and Y parameters. It leaves the previous Pi model unchanged; no Pi model is exported for this solve.`+automaticMeshDetails(n);
      $('status').textContent=finished?'Original capacitor port sweep complete.':`Solved capacitor port sample ${data.port_sweep.index+1}/${data.port_sweep.total}.`;return;
    }
    $('network-l').textContent=show(p.inductance_h,1e9,'nH');$('network-r').textContent=show(p.resistance,1,'Ω');$('network-q').textContent=show(p.quality_differential);
    $('network-details').textContent=`${n.model??'PI2'} · ${n.name} · ${show(n.frequency_hz,1e-9,'GHz')} · ${n.contacts} contacts · ${n.groups} conductor groups · ${n.filaments} filaments
`+
      `Cs1 = ${show(p.capacitance_second_f,1e15,'fF')} · Rs1 = ${show(p.resistance_second,1,'Ω')} · Cs2 = ${show(p.capacitance_first_f,1e15,'fF')} · Rs2 = ${show(p.resistance_first,1,'Ω')}
`+
      `Port Q = ${show(p.quality_first)} / ${show(p.quality_second)} · pi resonance estimate = ${show(n.resonance_hz,1e-9,'GHz')}
`+
      `Input impedance (single-ended / opposite port / differential): ${n.input_impedance.map(z=>z.map(v=>show(v)).join(' + j ')).join(' / ')} Ω
`+
      `Shunt resistance (single / differential): ${show(p.shunt_single,1,'Ω')} / ${show(p.shunt_differential,1,'Ω')} · ${n.eddy.evaluations} Green evaluations`+automaticMeshDetails(n);
    if(n.dispatch?.command==='2PORTX'&&n.dispatch.model==='PI2')$('network-details').textContent+='\n2PORTX uses PI2 for this structure in the original program.';
    if(n.model==='PI4')$('network-details').textContent+=`\nPads: ${n.pads.join(' / ')} · ${n.pad_contacts} pads-only contacts. Broader validation remains in progress.`;
    const diagnostics=capacitanceDiagnostics(data.warnings||''),quadErrors=n.eddy.quadrature_status.reduce((a,b)=>a+b,0);
    $('status').textContent=`Original ${n.model??'PI2'} solve complete${quadErrors?' with QUADPACK nonconvergence':''}.`+(diagnostics?'\n'+diagnostics:'');if(!finished)$('status').textContent=`Solved port sample ${data.port_sweep.index+1}/${data.port_sweep.total} at ${(n.frequency_hz/1e9).toPrecision(7)} GHz.`+(diagnostics?'\n'+diagnostics:'');return;
  }
  if(data.capacitance){
    capacitanceResult=data.capacitance;const c=capacitanceResult;setBusy(false);
    $('cap-total').textContent=c.capacitance_f===null?'—':(c.capacitance_f*1e15).toPrecision(7)+' fF';$('cap-resistance').textContent=c.resistance===null?'—':c.resistance.toPrecision(7)+' Ω';$('cap-contacts').textContent=String(c.contacts);
    $('cap-details').textContent=`${c.name} · ${(c.frequency_hz/1e9).toPrecision(5)} GHz · ${c.mesh.fft_x} × ${c.mesh.fft_y} FFT · ${c.matrix.length} × ${c.matrix.length} ${c.unfolded?'unfolded':'folded'} admittance matrix`+(c.unfolded?' · Unfolded CAP provides matrix values without a total-capacitance report.':'')+automaticMeshDetails(c);
    $('status').textContent='Original layered CAP solve complete.\n'+capacitanceDiagnostics(data.warnings||'');return;
  }
  if(data.simulation){
    const result=data.simulation,summary={...result};delete summary.matrix;
    const index=samples.findIndex(s=>s.frequency_hz===result.frequency_hz);if(index<0)samples.push(summary);else samples[index]=summary;
    samples.sort((a,b)=>a.frequency_hz-b.frequency_hz);selectedSimulation=result;renderSimulation();renderPlot();
    const finished=!data.sweep||data.sweep.done;setBusy(!finished,!finished);
    const diagnostics=(data.warnings||'').split('\n').map(line=>line.trim()).filter(line=>line && line!=='Inverting matrix...').join('\n');
    const eddy=result.eddy,quadErrors=eddy?.quadrature_status?.reduce((a,b)=>a+b,0)??0,model=eddy?.mode?`eddy mode ${eddy.mode}, layers ${eddy.indices.join(', ')||'none'}`:'eddy currents disabled';
    $('status').textContent=(finished?`Original LMAT solve complete${quadErrors?' with QUADPACK nonconvergence':''}. ${result.filaments} filaments; ${model}.`:`Solved ${data.sweep.index+1}/${data.sweep.total} samples · ${result.filaments} filaments.`)+(diagnostics?'\n'+diagnostics:'');
    return;
  }
  setBusy(false);$('loading').textContent='';
  if(data.coupling){const c=data.coupling;$('coupling-result').textContent=`${c.first} ↔ ${c.second} · k = ${c.coefficient.toPrecision(7)} · M = ${c.mutual_nh.toPrecision(7)} nH`;$('status').textContent=data.warnings||'Original conductor coupling estimate calculated.';return;}
  if(data.optimization){
    const r=data.optimization;activeOptimization=!r.done;
    if(r.sample){const s=r.sample;optimizationSamples.push(s);const option=document.createElement('option');option.value=s.index;option.textContent=`${s.index+1}: A ${s.descriptor[4].toPrecision(5)}, W ${s.descriptor[5].toPrecision(4)}, S ${s.descriptor[6].toPrecision(4)}, N ${s.descriptor[7].toPrecision(4)} · ${s.error?'solve failed':`L ${(s.pi.inductance_h*1e9).toPrecision(4)} nH · Q1 ${s.pi.quality_first.toPrecision(4)} · ${s.accepted?'matched':'filtered'}`}`;$('opt-choice').append(option);}
    setBusy(!r.done,!r.done);$('opt-state').textContent=`${optimizationSamples.length}/${r.total} candidates completed · ${optimizationSamples.filter(s=>s.accepted).length} matched · ${optimizationSamples.filter(s=>s.error).length} failed · ${(r.frequency_hz/1e9).toPrecision(5)} GHz`;
    $('status').textContent=r.done?'Coil-design search complete. Select a candidate to display or export.':`Searching original ${r.search} candidates…`;return;
  }
  if(data.analysis){conductorAnalysis=data.analysis;$('estimates-csv').disabled=false;$('electrical').textContent=data.analysis.windings.map(w=>`${w.name} · L = ${w.inductance_nh.toPrecision(7)} nH · R(${(data.analysis.frequency_hz/1e9).toPrecision(4)} GHz) = ${w.resistance_hf.toPrecision(7)} Ω\nLF Q = ${w.quality_low_frequency===null?'Undefined':w.quality_low_frequency.toPrecision(7)} · DC R = ${w.resistance_dc.toPrecision(7)} Ω · METALAREA = ${w.metal_area_um2.toPrecision(7)} µm²`).join('\n\n');$('status').textContent=data.warnings||'Original conductor estimates calculated. Use the combined network solve for high-frequency Q.';return;}
  activePortSweep=null;layout=data.layout;technology=data.technology;$('engine').textContent='● C++ / WASM ready';render();if(data.edited)$('layout-title').textContent='EDITED LAYOUT';if(data.optimization_candidate)$('layout-title').textContent=({0:'SWEEP SQUARE',1:'SWEEP POLYGON',18:'SWEEPMM SQUARE',20:'SWEEPMM POLYGON'})[data.optimization_candidate.descriptor[0]];clearEstimates();clearSimulation();clearCapacitance();clearNetwork();clearTransformer();clearThreePort();clearSelfResonance();
  $('sim-winding').replaceChildren();layout.shapes.forEach((shape,i)=>{const option=document.createElement('option');option.value=i;option.textContent=shape.name;$('sim-winding').append(option);});
  for(const id of ['cap-winding','cap-ground']){$(id).replaceChildren();if(id==='cap-ground'){const option=document.createElement('option');option.value=-1;option.textContent='Substrate reference';$(id).append(option);}layout.shapes.forEach((shape,i)=>{const option=document.createElement('option');option.value=i;option.textContent=shape.name;$(id).append(option);});}
  for(const id of ['trans-primary','trans-secondary','network-pad1','network-pad2','coupling-first','coupling-second']){$(id).replaceChildren();layout.shapes.forEach((shape,i)=>{const option=document.createElement('option');option.value=i;option.textContent=shape.name;$(id).append(option);});}
  $('network-pad1').value=Math.max(0,layout.shapes.findIndex(s=>s.name==='P1'));$('network-pad2').value=layout.shapes.some(s=>s.name==='P2')?layout.shapes.findIndex(s=>s.name==='P2'):Math.min(1,layout.shapes.length-1);
  $('trans-primary').value=Math.max(0,layout.shapes.findIndex(s=>s.name==='P'));$('trans-secondary').value=layout.shapes.some(s=>s.name==='S')?layout.shapes.findIndex(s=>s.name==='S'):Math.min(1,layout.shapes.length-1);
  $('coupling-first').value=Math.max(0,layout.shapes.findIndex(s=>s.name==='P'));$('coupling-second').value=layout.shapes.some(s=>s.name==='S')?layout.shapes.findIndex(s=>s.name==='S'):Math.min(1,layout.shapes.length-1);$('coupling-result').textContent='Select two windings for the original conductor COUPLING estimate.';
  for(const [i,id]of ['three-first','three-second','three-third'].entries()){$(id).replaceChildren();layout.shapes.forEach((shape,j)=>{const option=document.createElement('option');option.value=j;option.textContent=shape.name;$(id).append(option);});$(id).value=Math.min(i,layout.shapes.length-1);}
  $('cap-fftx').value=technology.domain.cells_x;$('cap-ffty').value=technology.domain.cells_y;
  const primary=layout.shapes.findIndex(s=>s.name==='P');if(primary>=0){$('sim-winding').value=primary;$('cap-winding').value=primary;}
  $('download').disabled=!layout.shapes.length;setBusy(false);$('electrical').textContent='Choose a frequency and calculate the conductor estimates.';
  $('edit-state').textContent=layout.selected?`Selected: ${layout.selected} (use . in commands)`:'No selected shape.';
  $('status').textContent=layout.warnings||(data.edited?'Geometry edits applied.':'Layout generated. GDS export uses a 1 nm grid.');
};
function parameters(){return ['length','height','width','spacing','turns','radius','sides','metal','exit','angle','inner','secondary'].map(id=>Number($(id).value));}
function options(){return {nativeInput:true,x:Number($('origin-x').value),y:Number($('origin-y').value),phase:Number($('phase').value),grid:Number($('snap-grid').value),snap:$('snap').checked,append:Boolean(layout && $('append-layout').checked),name:$('shape-name').value,secondaryName:$('secondary-name').value,secondaryWidth:Number($('secondary-width').value),nx:Number($('via-nx').value),ny:Number($('via-ny').value),via:Number($('via-layer').value)};}
function frequency(){const value=Number($('frequency').value);if(!Number.isFinite(value)||value<=0)throw Error('Enter a positive frequency.');return value;}
function send(data,stoppable=false){setBusy(true,stoppable);worker.postMessage({id:++request,...data});}
$('coupling').onclick=()=>{try{if(layout){$('status').textContent='Calculating original coupling estimate…';send({action:'coupling',first:Number($('coupling-first').value),second:Number($('coupling-second').value)});}}catch(e){failure(e.message);}};
$('opt-search-kind').onchange=()=>{
  const text=$('opt-keywords').value;if($('opt-search-kind').value!=='SWEEPMM'||!technology||/(?:^|:)\s*EXIT\s*=/i.test(text))return;
  const match=text.match(/(?:^|:)\s*METAL\s*=\s*([^:]+)/i),value=match?.[1].trim().replace(/[a-z]/g,c=>c.toUpperCase());
  const top=match?(/^[0-9]+$/.test(value)?Number(value):technology.metals.findIndex(m=>m.name===value)):technology.metals.length-1;
  if(top>0&&top<technology.metals.length)$('opt-keywords').value+=':EXIT='+technology.metals[top-1].name;
};
$('opt-search').onclick=()=>{try{optimizationSamples=[];$('opt-choice').replaceChildren();activeOptimization=true;$('status').textContent='Planning original SWEEP candidates…';send({...portRequest('network'),action:'optimize',technology:technologyText,keywords:$('opt-keywords').value,options:{grid:Number($('snap-grid').value),snap:$('snap').checked,candidate_limit:Number($('opt-limit').value),search:$('opt-search-kind').value}},true);}catch(e){failure(e.message);}};
$('opt-stop').onclick=()=>worker.postMessage({id:++request,action:'cancel'});
$('opt-choice').onchange=()=>setBusy(busy);
$('opt-csv').onclick=()=>download(optimizationCSV(optimizationSamples),'reASITIC-coil-designs.csv','text/csv');
$('opt-apply').onclick=()=>{try{send({action:'optimization-apply',index:Number($('opt-choice').value)});}catch(e){failure(e.message);}};
$('analyze').onclick=()=>{try{if(layout){$('status').textContent='Calculating original conductor estimates…';send({action:'analyze',frequency_ghz:frequency()});}}catch(e){failure(e.message);}};
$('estimates-csv').onclick=()=>{try{download(conductorEstimatesCSV(conductorAnalysis),'reASITIC-conductor-estimates.csv','text/csv');}catch(e){failure(e.message);}};
$('apply-edit').onclick=()=>{try{const o=options();$('status').textContent='Applying original geometry edits…';send({action:'edit',commands:$('edit-commands').value,options:{grid:o.grid,snap:o.snap}});}catch(e){failure(e.message);}};
function generate(){if(!ready||!technologyText)return;try{const o=options(),creator=$('native-command').value.trim()?parseCreatorCommand($('native-command').value,$('native-answers').value):creatorRecipe(Number($('kind').value),parameters(),o);$('download').disabled=true;$('loading').textContent='Generating geometry…';send({creator,options:o,technology:technologyText});}catch(e){failure(e.message);}}
function updateMetals(text){const t=parseTechnology(text);for(const id of ['metal','exit']){$(id).replaceChildren();t.metals.forEach((m,i)=>{const o=document.createElement('option');o.value=i;o.textContent=m.name;$(id).append(o);});}const none=document.createElement('option');none.value=-1;none.textContent='None';$('exit').append(none);$('metal').value=Math.min(3,t.metals.length-1);$('exit').value=Math.min(2,t.metals.length-2);$('via-layer').replaceChildren();t.vias.forEach((v,i)=>{const option=document.createElement('option');option.value=i;option.textContent=v.name;$('via-layer').append(option);});for(const id of ['eddy-layer-1','eddy-layer-2']){$(id).replaceChildren();t.dielectric.forEach((layer,i)=>{const option=document.createElement('option');option.value=i;option.textContent=`Layer ${i} · ρ ${layer.resistivity} Ω·cm`;$(id).append(option);});}$('eddy-layer-count').value=t.eddy_layers.length;$('eddy-layer-1').value=t.eddy_layers[0]??0;$('eddy-layer-2').value=t.eddy_layers[1]??Math.min(1,t.dielectric.length-1);}
async function loadProcess(){try{$('append-layout').checked=false;if($('process').value==='custom'){updateMetals(importedText);technologyText=importedText;generate();return;}const response=await fetch(new URL(`./technology/${$('process').value}.tek`,import.meta.url));if(!response.ok)throw Error('Unable to load technology');const text=await response.text();updateMetals(text);technologyText=text;generate();}catch(e){failure(e.message);}}
$('process').onchange=loadProcess;
$('technology-file').onchange=async event=>{try{const file=event.target.files[0];if(!file)return;const text=await file.text();$('append-layout').checked=false;updateMetals(text);technologyText=importedText=text;let option=$('process').querySelector('[value="custom"]');if(!option){option=document.createElement('option');option.value='custom';$('process').append(option);}option.textContent=file.name;$('process').value='custom';generate();}catch(e){failure(e.message);}};
function updateCreatorFields(){
  const kind=Number($('kind').value),fields={0:['length','height','width','spacing','turns','metal','exit'],1:['radius','sides','width','spacing','turns','metal'],2:['length','width','metal'],3:['length','inner','width','spacing','turns','metal','exit'],4:['radius','sides','inner','width','spacing','turns','metal','exit'],5:['radius','sides','width','spacing','metal'],6:['length','height','width','spacing','turns','secondary','secondary-name','metal','exit'],7:['length','height','width','spacing','turns','secondary-name','metal','exit'],8:['length','height','width','spacing','turns','metal','exit'],9:['via-nx','via-ny','via-layer'],10:['length','height','metal','exit'],11:['length','width','secondary-width','spacing','turns','metal','exit']};
  const active=new Set(['shape-name','origin-x','origin-y','phase','snap-grid','snap',...fields[kind]]);
  for(const element of document.querySelectorAll('.fields input,.fields select')){element.closest('label').hidden=!active.has(element.id);element.disabled=!active.has(element.id)||!ready||busy||Boolean($('native-command').value.trim());}
  $('spacing-label').firstChild.textContent=kind===5?'Gap · degrees':'Spacing';
}
$('kind').onchange=()=>{$('spacing').value=$('kind').value==='5'?20:$('kind').value==='7'?8:2;updateCreatorFields();};
updateCreatorFields();
$('native-command').oninput=updateCreatorFields;
$('fill-command').onclick=()=>{const r=creatorRecipe(Number($('kind').value),parameters(),options());const names={0:'SQUARE',1:'SPIRAL',2:'WIRE',3:'TRANS',4:'CAPACITOR',5:'BALUN',10:'3DMIRRORTRANS',14:'VIA',16:'SYMSQ',17:'SYMPOLY',18:'MMSQUARE',22:'RING'};$('native-command').value=names[r.id]+(r.keywords?' '+r.keywords:'');$('native-answers').value=r.answers;updateCreatorFields();};
$('controls').onsubmit=e=>{e.preventDefault();generate();};
function setView(){if(view)$('preview').setAttribute('viewBox',view.join(' '));}
$('fit').onclick=()=>{if(initialView){view=[...initialView];setView();}};
function render(){
  const preview=renderGeometryPreview($('preview'),layout,technology,{colors});
  if(!preview.bounds){initialView=[0,-technology.domain.y,technology.domain.x,technology.domain.y];view=[...initialView];$('legend').replaceChildren();setView();$('footprint').textContent='No visible geometry';$('segments').textContent=layout.shapes.reduce((n,s)=>n+[...segmentRecords(s.segments)].length,0);$('resistance').textContent=layout.shapes.map(s=>`${s.name}: ${s.resistance_dc.toFixed(3)} Ω`).join(' · ')||'—';return;}
  const [x0,y0,x1,y1]=preview.bounds;
  const w=Math.max(1,x1-x0),h=Math.max(1,y1-y0),padding=Math.max(w,h)*.15;
  initialView=[x0-padding,-y1-padding,w+padding*2,h+padding*2];view=[...initialView];setView();
  $('layout-title').textContent=$('native-command').value.trim()?$('native-command').value.match(/[^ \t\r\n\f\v,]+/)[0].toUpperCase():$('kind').selectedOptions[0].textContent.toUpperCase();$('footprint').textContent=`${w.toFixed(1)} × ${h.toFixed(1)} µm`;$('segments').textContent=layout.shapes.reduce((n,s)=>n+[...segmentRecords(s.segments)].length,0);$('resistance').textContent=layout.shapes.map(s=>`${s.name}: ${s.resistance_dc.toFixed(3)} Ω`).join(' · ');
  $('legend').replaceChildren();for(const layer of preview.layers){const el=document.createElement('span');el.style.setProperty('--color',colors[(layer-1)%colors.length]);el.textContent=layer>=100?technology.vias[layer-100].name:technology.metals[layer-1].name;$('legend').append(el);}
}
function clearThreePort(){threeResult=null;portSamples['three-port']=[];for(const id of ['three-record','three-raw-csv','three-metrics-csv','three-sweep-csv'])$(id).disabled=true;$('three-sweep-state').textContent='No port sweep samples.';threeBacking=null;$('three-backing-file').value='';$('three-backing-status').textContent='Normal numerical solve. An optional native-state capture replays historical output for comparison.';for(const id of ['three-l1','three-l2','three-l3'])$(id).textContent='—';$('three-details').textContent='Generate or add three windings, then solve the original three-port model.';}
$('three-backing-file').onchange=async event=>{try{const file=event.target.files[0];if(!file)return;setBusy(true);$('status').textContent='Loading native three-port backing capture…';if(file.size>20*1024*1024)throw Error('Native backing capture exceeds 20 MiB.');const source=JSON.parse(await file.text()),h=validateThreePortBacking(source),indices=h.names.map(name=>layout?.shapes.findIndex(s=>s.name===name)),ground=layout?.shapes.findIndex(s=>s.name===h.ground);if(indices.some(i=>i===undefined||i<0)||ground===undefined||ground<0)throw Error('Generate the captured windings and reference before importing their native backing.');
 const config=source.case;if(config){const positive=(a,n)=>Array.isArray(a)&&a.length===n&&a.every(v=>Number.isFinite(v)&&v>0);if(!positive(config.cells,3)||!positive(config.contact_cells,2)||!positive(config.grid,2)||config.grid.some(n=>!Number.isInteger(n)||n>2147483647||(n&(n-1)))||!Number.isInteger(config.eddy_mode)||config.eddy_mode<0||config.eddy_mode>2||!Array.isArray(config.layers)||config.layers.length>2||config.layers.some(n=>!Number.isInteger(n)||n<0))throw Error('Captured optional control settings are invalid.');}
 threeBacking=h;['three-first','three-second','three-third'].forEach((id,i)=>$(id).value=indices[i]);$('cap-ground').value=ground;$('cap-frequency').value=source.case?.frequency_ghz??h.frequency_hz/1e9;
 if(source.case){const c=source.case;['cell-length','cell-width','cell-thickness'].forEach((id,i)=>$(id).value=c.cells[i]);['cap-length','cap-width'].forEach((id,i)=>$(id).value=c.contact_cells[i]);$('cap-fftx').value=c.grid[0];$('cap-ffty').value=c.grid[1];$('eddy-mode').value=c.eddy_mode;$('eddy-layer-count').value=c.layers.length;$('eddy-layer-1').value=c.layers[0]??0;$('eddy-layer-2').value=c.layers[1]??0;}
 $('three-backing-status').textContent=`Native-state comparison: ${file.name} · ${h.names.join(' / ')} · ${h.node_count} native nodes`;setBusy(false);
}catch(e){event.target.value='';failure(e.message);}};
$('three-clear-backing').onclick=()=>{threeBacking=null;$('three-backing-file').value='';$('three-backing-status').textContent='Normal numerical solve. An optional native-state capture replays historical output for comparison.';setBusy(false);};
function clearTransformer(){clearLoadedPort();transformerResult=null;portSamples.transformer=[];$('trans-sweep-csv').disabled=true;$('trans-sweep-state').textContent='No port sweep samples.';for(const id of ['trans-y-csv','trans-z-csv','trans-metrics-csv','trans-record','trans-s-csv'])$(id).disabled=true;for(const id of ['trans-l1','trans-l2','trans-k'])$(id).textContent='—';$('trans-details').textContent='Generate or add two windings, then solve the original narrowband transformer model.';}
function clearLoadedPort(){loadedPortResult=null;$('loaded-csv').disabled=true;$('loaded-result').textContent='Solve a two-port model, then choose a load.';}
function clearNetwork(){clearLoadedPort();networkResult=null;portSamples.network=[];$('network-sweep-csv').disabled=true;$('network-sweep-state').textContent='No port sweep samples.';for(const id of ['network-matrix-csv','network-metrics-csv','network-vectors-csv','network-record','network-s-csv'])$(id).disabled=true;for(const id of ['network-l','network-r','network-q'])$(id).textContent='—';$('network-details').textContent='Run the combined solve to inspect shunt branches, port quality factors, input impedance and the pi resonance estimate.';}
function portRequest(target){
 const common={...simulationRequest(),ground:Number($('cap-ground').value),frequency_ghz:Number($('cap-frequency').value),contact_mesh:{length:Number($('cap-length').value),width:Number($('cap-width').value)},fft_x:Number($('cap-fftx').value),fft_y:Number($('cap-ffty').value),contact_limit:Number($('contact-limit').value)};
 if(target==='three-port')return {...common,action:'three-port',windings:['three-first','three-second','three-third'].map(id=>Number($(id).value)),backing:threeBacking};
 return target==='transformer'?{...common,action:'transformer',primary:Number($('trans-primary').value),secondary:Number($('trans-secondary').value),termination_ohm:Number($('trans-termination').value)}:{...common,action:'network',model:$('network-model').value,pads:[Number($('network-pad1').value),Number($('network-pad2').value)],winding:Number($('cap-winding').value)};
}
for(const [target,key] of [['network','network'],['transformer','trans'],['three-port','three']]){
 $(key+'-solve').onclick=()=>{try{activePortSweep=null;portSamples[target]=[];$(key+'-sweep-state').textContent='Single-frequency solve in progress.';$('status').textContent='Solving the original port network…';send(portRequest(target));}catch(e){failure(e.message);}};
 $(key+'-sweep').onclick=()=>{try{const config=portRequest(target);portSamples[target]=[];activePortSweep=target;$(key+'-sweep-state').textContent='No completed samples in this sweep yet.';$('status').textContent='Starting the native port frequency sweep…';send({...config,action:'port-sweep',port_kind:target,start_ghz:Number($(key+'-start').value),stop_ghz:Number($(key+'-end').value),step_ghz:Number($(key+'-step').value)},true);}catch(e){failure(e.message);}};
 $(key+'-sweep-csv').onclick=()=>{try{download(portSweepMetricsCSV(portSamples[target]),'reASITIC-'+key+'-sweep.csv','text/csv');}catch(e){failure(e.message);}};
}
function portResults(target){const result=target==='three-port'?threeResult:target==='network'?networkResult:transformerResult;return portSamples[target].length?portSamples[target]:result?[result]:[];}
function clearCapacitance(){capacitanceResult=null;for(const id of ['cap-total','cap-resistance','cap-contacts'])$(id).textContent='—';$('cap-details').textContent='Choose a device, optional ground and contact/FFT mesh, then run the original CAP solve.';}
$('cap-solve').onclick=()=>{try{const o=options(),frequencyGHz=Number($('cap-frequency').value);if(!(frequencyGHz>0))throw Error('Enter a positive CAP frequency.');$('status').textContent='Generating layered Green tables and solving the original CAP matrix…';send({action:'capacitance',...automaticCells(),winding:Number($('cap-winding').value),ground:Number($('cap-ground').value),frequency_ghz:frequencyGHz,mesh:{length:Number($('cap-length').value),width:Number($('cap-width').value)},fft_x:Number($('cap-fftx').value),fft_y:Number($('cap-ffty').value),contact_limit:Number($('contact-limit').value),unfolded:$('cap-mode').value==='unfolded',nativeInput:true,grid:o.grid,snap:o.snap});}catch(e){failure(e.message);}};
function clearEstimates(){conductorAnalysis=null;$('estimates-csv').disabled=true;$('electrical').textContent='Choose a frequency and calculate the conductor estimates.';}
function clearSimulation(){samples=[];selectedSimulation=null;sampleRequest=null;sampleKey='';for(const id of ['sim-l','sim-r','sim-q'])$(id).textContent='—';$('sim-details').textContent='Choose a winding and mesh, then run the original filament solver.';renderPlot();}
function clearSelfResonance(){activeSelfResonance=false;selfResonanceResult=null;selfResonanceSamples=[];$('selfres-state').textContent='No self-resonance search yet.';$('selfres-result').textContent='';$('selfres-rows').replaceChildren();$('selfres-csv').disabled=true;}
function renderSelfResonance(){
 const r=selfResonanceResult,show=(v,scale=1)=>v===undefined?'—':(v*scale).toPrecision(10);if(!r)return;
 $('selfres-state').textContent=`${r.analyses} ${r.source_model} analyses · ${r.differential?'differential':'single-ended'} input · ${r.done?'finished':'searching'}`;
 $('selfres-result').textContent=(r.status==='found'?`Native returned ${show(r.frequency_hz,1e-9)} GHz · convergence by ${r.convergence==='impedance'?'impedance tolerance':'frequency step'}`:r.status==='sample_limit'?'Analysis budget reached; the native search has not completed.':r.status==='native_limit'?'The original search reached its 100-iteration limit.':'')+`\nLast analyzed ${show(r.last_analyzed_hz,1e-9)} GHz · observed sign change: ${r.observed_sign_change?'yes':'no'}`;
 $('selfres-rows').replaceChildren();selfResonanceSamples.forEach((sample,i)=>{const row=document.createElement('tr');for(const value of [i+1,show(sample.frequency_hz,1e-9),r.trace[i]?show(r.trace[i].imaginary_impedance_ohm):'Not used by native iteration']){const cell=document.createElement('td');cell.textContent=value;row.append(cell);}$('selfres-rows').append(row);});
}
$('selfres-search').onclick=()=>{try{const request=portRequest('network');clearSelfResonance();activeSelfResonance=true;activePortSweep=null;$('status').textContent='Starting native SELFRES numerical search…';send({...request,action:'resonance',model:'PI2',start_ghz:Number($('selfres-start').value),end_ghz:Number($('selfres-end').value),step_ghz:Number($('selfres-step').value),mode:$('selfres-mode').value,sample_budget:Number($('selfres-budget').value)},true);}catch(e){failure(e.message);}};
$('selfres-csv').onclick=()=>{try{download(resonanceHistoryCSV(selfResonanceSamples,selfResonanceResult),'reASITIC-SELFRES-history.csv','text/csv');}catch(e){failure(e.message);}};
function automaticCells(){return $('cell-policy').value==='automatic'?{automatic_cells:{alpha:Number($('cell-alpha').value),beta:Number($('cell-beta').value)}}:{};}
$('cell-policy').onchange=()=>setBusy(busy);
function simulationRequest(){const o=options();return {...automaticCells(),max_width_cells:Number($('max-width-cells').value),winding:Number($('sim-winding').value),mesh:{length:Number($('cell-length').value),width:Number($('cell-width').value),thickness:Number($('cell-thickness').value),subdivision_trigger:Number($('cell-trigger').value)},filament_limit:Number($('filament-limit').value),nativeInput:true,grid:o.grid,snap:o.snap,eddy:{mode:Number($('eddy-mode').value),layers:[$('eddy-layer-1').value,$('eddy-layer-2').value].slice(0,Number($('eddy-layer-count').value)).map(Number),evaluation_limit:Number($('green-budget').value)}};}
function prepareSamples(config,clear=false){const key=JSON.stringify(config);if(clear||key!==sampleKey){samples=[];selectedSimulation=null;renderPlot();}sampleRequest=config;sampleKey=key;}
$('simulate').onclick=()=>{try{const config=simulationRequest();prepareSamples(config);const ghz=frequency();$('status').textContent='Assembling and solving the original filament matrix…';send({action:'simulate',...config,frequency_ghz:ghz});}catch(e){failure(e.message);}};
$('sweep').onclick=()=>{try{const frequencies=frequencySweep(Number($('sweep-start').value),Number($('sweep-stop').value),Number($('sweep-count').value),$('frequency-scale').value==='log'),config=simulationRequest();prepareSamples(config,true);$('status').textContent=`Starting ${frequencies.length} original filament solves…`;send({action:'sweep',...config,frequencies_ghz:frequencies},true);}catch(e){failure(e.message);}};
function stopSweep(){worker.postMessage({id:++request,action:'cancel'});for(const id of ['stop','network-stop','trans-stop','three-stop','selfres-stop'])$(id).disabled=true;$('status').textContent='Stopping after the current sample…';}
for(const id of ['stop','network-stop','trans-stop','three-stop','selfres-stop'])$(id).onclick=stopSweep;
function automaticMeshDetails(result){const p=result.automatic_cells;if(!p)return result.max_width_cells===undefined?'':`\nMAXNW = ${result.max_width_cells}`;const m=p.mesh,c=p.contact_mesh,show=v=>v.toPrecision(6);return `\nAUTOCELL · alpha ${show(p.alpha)}, beta ${show(p.beta)} · ${show(m.length)} / ${show(m.width)} / ${show(m.thickness)} µm cells · ${show(c.length)} / ${show(c.width)} µm contacts`+(result.max_width_cells===undefined?'':` · MAXNW ${result.max_width_cells}`);}
function renderSimulation(){const s=selectedSimulation;if(!s)return;$('sim-l').textContent=(s.inductance_h*1e9).toPrecision(7)+' nH';$('sim-r').textContent=s.resistance.toPrecision(7)+' Ω';$('sim-q').textContent=s.series_q===null?'Undefined':s.series_q.toPrecision(7);$('sim-details').textContent=`${s.name} · ${(s.frequency_hz/1e9).toPrecision(5)} GHz · ${s.conductors} conductor groups · ${s.filaments} filaments · ${s.mesh.length}/${s.mesh.width}/${s.mesh.thickness} µm cells`+(s.eddy?` · ${s.eddy.evaluations} Green evaluations · ${s.eddy.quadrature_status.reduce((a,b)=>a+b,0)} quadrature diagnostics`:'');$('sim-details').textContent+=automaticMeshDetails(s);$('matrix-csv').disabled=busy||!s.matrix;}
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
$('download').onclick=()=>{try{download(writeGDS(layout,technology,{maxBytes:Number($('gds-limit').value)*1024*1024}),'reASITIC-coil.gds','application/octet-stream');}catch(e){failure(e.message);}};
$('cap-matrix-csv').onclick=()=>{try{download(capacitanceMatrixCSV(capacitanceResult),'reASITIC-CAP-admittance.csv','text/csv');}catch(e){failure(e.message);}};
$('potential-matrix-csv').onclick=()=>{try{download(capacitanceMatrixCSV(capacitanceResult,true),'reASITIC-CAP-potential.csv','text/csv');}catch(e){failure(e.message);}};
$('three-record').onclick=()=>{try{download(portSweepRecord(portResults('three-port'),$('three-parameter').value,$('three-format').value==='polar'),'reASITIC-three-port-native.txt','text/plain');}catch(e){failure(e.message);}};
$('three-raw-csv').onclick=()=>{try{download(threePortSweepMatrixCSV(portResults('three-port'),$('three-parameter').value),'reASITIC-three-port-raw.csv','text/csv');}catch(e){failure(e.message);}};
$('three-metrics-csv').onclick=()=>{try{download(threePortMetricsCSV(threeResult),'reASITIC-three-port-model.csv','text/csv');}catch(e){failure(e.message);}};
$('trans-y-csv').onclick=()=>{try{download(networkMatrixCSV(transformerResult),'reASITIC-transformer-admittance.csv','text/csv');}catch(e){failure(e.message);}};
$('trans-z-csv').onclick=()=>{try{download(transformerImpedanceCSV(transformerResult),'reASITIC-transformer-impedance.csv','text/csv');}catch(e){failure(e.message);}};
$('trans-metrics-csv').onclick=()=>{try{download(transformerMetricsCSV(transformerResult),'reASITIC-transformer-metrics.csv','text/csv');}catch(e){failure(e.message);}};
$('network-model').onchange=()=>setBusy(busy);
$('network-record').onclick=()=>{try{download(portSweepRecord(portResults('network'),$('network-parameter').value,$('network-format').value==='polar'),'reASITIC-two-port-native.txt','text/plain');}catch(e){failure(e.message);}};
$('network-s-csv').onclick=()=>{try{download(portSweepScatteringCSV(portResults('network')),'reASITIC-two-port-scattering.csv','text/csv');}catch(e){failure(e.message);}};
$('trans-record').onclick=()=>{try{download(portSweepRecord(portResults('transformer'),$('trans-parameter').value,$('trans-format').value==='polar'),'reASITIC-transformer-native.txt','text/plain');}catch(e){failure(e.message);}};
$('trans-s-csv').onclick=()=>{try{download(portSweepScatteringCSV(portResults('transformer')),'reASITIC-transformer-scattering.csv','text/csv');}catch(e){failure(e.message);}};
$('network-vectors-csv').onclick=()=>{try{download(networkVectorsCSV(networkResult),'reASITIC-pix-vectors.csv','text/csv');}catch(e){failure(e.message);}};
$('network-matrix-csv').onclick=()=>{try{download(networkMatrixCSV(networkResult),'reASITIC-network-admittance.csv','text/csv');}catch(e){failure(e.message);}};
$('network-metrics-csv').onclick=()=>{try{download(networkMetricsCSV(networkResult),'reASITIC-pi-model.csv','text/csv');}catch(e){failure(e.message);}};
$('matrix-csv').onclick=()=>{try{download(matrixCSV(selectedSimulation),`reASITIC-${selectedSimulation.name}-matrix.csv`,'text/csv');}catch(e){failure(e.message);}};
$('results-csv').onclick=()=>{try{download(sweepCSV(samples),'reASITIC-frequency-results.csv','text/csv');}catch(e){failure(e.message);}};
renderPlot();setBusy(false);loadProcess();

$('loaded-source').onchange=()=>{clearLoadedPort();setBusy(busy);};
for(const id of ['loaded-port','loaded-real','loaded-imag','loaded-cap1','loaded-cap2'])$(id).oninput=()=>clearLoadedPort();
$('loaded-calculate').onclick=()=>{try{send({action:'loaded-port',source:$('loaded-source').value,port:Number($('loaded-port').value),load_real_ohm:Number($('loaded-real').value),load_imag_ohm:Number($('loaded-imag').value),first_capacitance_f:Number($('loaded-cap1').value)*1e-15,second_capacitance_f:Number($('loaded-cap2').value)*1e-15,nativeInput:true});}catch(e){failure(e.message);}};
$('loaded-csv').onclick=()=>{try{download(loadedPortCSV(loadedPortResult),'reASITIC-loaded-impedance.csv','text/csv');}catch(e){failure(e.message);}};
