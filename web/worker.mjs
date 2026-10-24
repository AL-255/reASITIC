import createKernel from './asitic.js';
import {resonancePlan,capacitorPortMatrix,capacitorPortResponse} from './self-resonance.mjs';
import {optimizationPlan,createOptimizationCandidate,optimizationAccept} from './optimization.mjs';
import {solverCells,meshResult} from './mesh-policy.mjs';
import {portSampleSummary} from './port-sweep.mjs';
import {validateThreePortBacking,allocateThreePortBacking} from './three-port.mjs';
import {parseTechnology,configureKernel,generateGeometry,createGeometry,editGeometry} from './technology.mjs';
const kernel=await createKernel();let epoch=0,processTechnology=null,hasLayout=false,currentLayout=null,optimizationKernel=null,optimizationState=null;
function frequency(data,value=data.frequency_ghz) {
  const result=value===undefined?data.frequency:kernel._normalize_value(Number(value),5,.1,0);
  if(!Number.isFinite(result) || result<=0)throw Error('Enter a positive native ASITIC frequency.');return result;
}
function solve(data,hz) {
  const selection=solverCells(kernel,data,hz,'inductance'),parsed=selection.cells;
  let pointer;
  if(data.eddy){
    const e=data.eddy,layers=e.layers??processTechnology?.eddy_layers??[];
    if(!Array.isArray(layers) || layers.length>2 || layers.some(i=>!Number.isInteger(i) || i<0))throw Error('Select up to two substrate layers.');
    pointer=kernel._simulate_eddy(data.winding??0,hz,...parsed,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,e.evaluation_limit??250000);
  }else pointer=kernel._simulate(data.winding??0,hz,...parsed,data.filament_limit??256);
  const result=JSON.parse(kernel.UTF8ToString(pointer));
  return meshResult(result,selection);
}
function solveTransformer(data,hz){
      if(!processTechnology || !hasLayout)throw Error('Generate two windings before running CALCTRANS.');
      const domain=processTechnology.domain,e=data.eddy??{},layers=e.layers??processTechnology.eddy_layers??[],termination=Number(data.termination_ohm??0);
      if(!Array.isArray(layers) || layers.length>2 || layers.some(i=>!Number.isInteger(i) || i<0))throw Error('Select up to two substrate layers.');
      if(!Number.isFinite(termination))throw Error('Use a finite transformer termination.');
      if(termination!==0 && (data.ground??-1)<0)throw Error('Choose an explicit ground for a nonzero CALCTRANS termination.');
      const selection=solverCells(kernel,data,hz,'network'),parsed=selection.cells;
      const resistance=data.nativeInput?kernel._normalize_value(termination,4,.1,0):termination;
      const result=JSON.parse(kernel.UTF8ToString(kernel._simulate_transformer(data.primary??0,data.secondary??1,data.ground??-1,hz,...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,resistance,e.evaluation_limit??250000)));
      return meshResult(result,selection);
}
function solveNetwork(data,hz,context,portCommand=false){
      const target=context?.kernel??kernel,technology=context?.technology??processTechnology,layout=context?.layout??currentLayout;
      const requested=data.model??'PI2';if(!['PI2','PIX','PI4'].includes(requested))throw Error('Select PI2, PIX or PI4.');
      if(!technology || (context?!layout?.shapes.length:!hasLayout))throw Error('Generate a layout before running the combined network.');
      const winding=data.winding??0,reference=data.ground??-1;
      if(!Number.isInteger(winding)||winding<0||!layout.shapes[winding])throw Error('Select an existing integer winding for the port model.');
      if(!Number.isInteger(reference)||reference< -1||(reference>=0&&!layout.shapes[reference]))throw Error('Select an existing integer ground or the device reference.');
      let dispatch,model=requested;
      if(portCommand&&model!=='PI4'){
        dispatch=JSON.parse(target.UTF8ToString(target._port_analysis_plan(winding,model==='PIX'?1:0)));if(dispatch.error)throw Error(dispatch.error);model=dispatch.model;
      }
      const solver=model==='PI4'?target._simulate_pad_network:model==='PIX'?target._simulate_extended_network:target._simulate_network;
      // 2PORTGND acquires its required reference through native argument type
      // 8 even when it is the device itself. 2PORTX's omitted optional ground
      // does not perform that acquisition/reset.
      const ground=portCommand&&requested==='PI2'&&reference===-1?winding:reference;
      const pads=model==='PI4'?(data.pads??[]):[];if(model==='PI4'&&(!Array.isArray(pads)||pads.length!==2||pads.some(i=>!Number.isInteger(i)||i<0||!layout.shapes[i])))throw Error('Select valid first and second PI4 pad records.');
      const domain=technology.domain,e=data.eddy??{},layers=e.layers??technology.eddy_layers??[];
      if(!Array.isArray(layers) || layers.length>2 || layers.some(i=>!Number.isInteger(i) || i<0))throw Error('Select up to two substrate layers.');
      if(model==='CAP'){
        const selection=solverCells(target,{...data,mesh:data.contact_mesh??{}},hz,'capacitance');
        const cap=meshResult(JSON.parse(target.UTF8ToString(target._simulate_capacitance(winding,ground,hz,...selection.cells,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,0))),selection);
        return {...cap,...capacitorPortResponse(target,cap),capacitance_matrix:cap.matrix,dispatch};
      }
      const selection=solverCells(target,data,hz,'network'),parsed=selection.cells;
      const result=JSON.parse(target.UTF8ToString(solver(winding,...pads,ground,hz,...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,e.evaluation_limit??250000)));
      const solved=meshResult(result,selection);if(dispatch)solved.dispatch=dispatch;return solved;
}
async function solveThreePort(data,hz,task){
      if(!processTechnology || !hasLayout)throw Error('Generate three windings before running 3PORT.');
      const indices=data.windings??[0,1,2];if(!Array.isArray(indices)||indices.length!==3||new Set(indices).size!==3||indices.some(i=>!Number.isInteger(i)||i<0||!currentLayout.shapes[i]))throw Error('Select three distinct winding records.');
      const domain=processTechnology.domain,e=data.eddy??{},layers=e.layers??processTechnology.eddy_layers??[];
      if(!Array.isArray(layers)||layers.length>2||layers.some(i=>!Number.isInteger(i)||i<0))throw Error('Select up to two substrate layers.');
      const selection=solverCells(kernel,data,hz,'network'),parsed=selection.cells,ground=data.ground??-1;
      const args=[...indices,ground,hz,...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,e.evaluation_limit??250000];let result;
      if(data.backing){
        const h=validateThreePortBacking(data.backing),names=indices.map(i=>currentLayout.shapes[i].name),reference=currentLayout.shapes[ground<0?indices[0]:ground]?.name;
        if(JSON.stringify(h.names)!==JSON.stringify(names)||h.ground!==reference||h.frequency_hz!==hz)throw Error('Captured backing winding order, reference or frequency differs from this request.');
        const digest=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(processTechnology.source))),v=>v.toString(16).padStart(2,'0')).join('');if(task!==epoch)return;if(digest!==h.technology_sha256)throw Error('Captured backing uses a different technology file.');
        const memory=allocateThreePortBacking(kernel,h);try{result=JSON.parse(kernel.UTF8ToString(kernel._probe_three_port(...args,memory.cap,memory.ind,memory.cells,memory.addresses)));}finally{memory.release();}
      }else result=JSON.parse(kernel.UTF8ToString(kernel._simulate_three_port(...args)));
      return meshResult(result,selection);
}
self.onmessage=async({data})=>{
  const task=++epoch;const action=data.action??'generate';
  try{
    if(action==='cancel'){postMessage({id:data.id,cancelled:true});return;}
    if(action==='optimize'){
      const technology=data.technology?parseTechnology(data.technology):processTechnology;if(!technology)throw Error('Select a technology before running SWEEP.');
      if(!optimizationKernel){optimizationKernel=await createKernel();optimizationKernel._set_potential_memo(1);}if(task!==epoch)return;
      configureKernel(optimizationKernel,technology);
      const planningKernel=processTechnology?.source===technology.source?kernel:optimizationKernel;
      const plan=optimizationPlan(planningKernel,data.keywords,technology.domain,data.options),summary={search:plan.search??'SWEEP',name:plan.name,frequency_hz:plan.frequency_hz,inductance_min:plan.inductance_min,inductance_max:plan.inductance_max,minimum_quality:plan.minimum_quality,total:plan.candidates.length};
      optimizationState={plan,technology,completed:new Set()};
      if(!plan.candidates.length){postMessage({id:data.id,optimization:{...summary,done:true}});return;}
      for(const [index,descriptor]of plan.candidates.entries()){
        if(task!==epoch)return;const sample={index,descriptor,accepted:false,frequency_hz:plan.frequency_hz};
        try{
          const layout=createOptimizationCandidate(optimizationKernel,plan.name,descriptor);sample.geometry_valid=true;
          const solved=solveNetwork({...data,...(plan.search==='SWEEPMM'&&plan.simulation_type===1?{automatic_cells:false}:{}),model:'PI2',winding:0,ground:-1},plan.frequency_hz,{kernel:optimizationKernel,technology,layout});
          sample.pi=solved.pi;if(solved.automatic_cells)sample.automatic_cells=solved.automatic_cells;sample.accepted=optimizationAccept(optimizationKernel,plan,solved.pi);sample.warnings=optimizationKernel.UTF8ToString(optimizationKernel._last_warnings());
        }catch(error){sample.error=error.message;}
        optimizationState.completed.add(index);const done=index===plan.candidates.length-1;postMessage({id:data.id,optimization:{...summary,sample,done}});if(!done)await new Promise(resolve=>setTimeout(resolve,0));
      }return;
    }
    if(action==='optimization-apply'){
      if(!optimizationState||!Number.isInteger(data.index)||!optimizationState.plan.candidates[data.index]||!optimizationState.completed.has(data.index))throw Error('Select a completed SWEEP candidate.');
      const {plan,technology}=optimizationState,append=Boolean(data.options?.append);
      if(append&&hasLayout&&technology.source!==processTechnology?.source)throw Error('Use the same technology when adding a SWEEP candidate.');
      // Admit and construct privately before replacing the live process/layout.
      configureKernel(optimizationKernel,technology);
      createOptimizationCandidate(optimizationKernel,plan.name,plan.candidates[data.index]);
      if(processTechnology?.source!==technology.source)configureKernel(kernel,technology);
      const layout=createOptimizationCandidate(kernel,plan.name,plan.candidates[data.index],append);processTechnology=technology;currentLayout=layout;hasLayout=true;
      postMessage({id:data.id,layout,technology,optimization_candidate:{index:data.index,descriptor:plan.candidates[data.index]}});return;
    }
    if(action==='edit'){
      if(!processTechnology||!hasLayout)throw Error('Generate a layout before editing geometry.');
      const layout=editGeometry(kernel,data.commands,{...data.options,domain:processTechnology.domain});currentLayout=layout;hasLayout=true;postMessage({id:data.id,layout,technology:processTechnology,edited:true});return;
    }
    if(action==='coupling'){
      const first=data.first,second=data.second,limit=data.pair_limit??250000;
      if(!Number.isInteger(first)||!Number.isInteger(second)||!Number.isInteger(limit))throw Error('Select two winding indices and an integer coupling budget.');
      if(!hasLayout||first<0||second<0||first>=currentLayout.shapes.length||second>=currentLayout.shapes.length)throw Error('Select existing windings for COUPLING.');
      if(limit<1||limit>1000000)throw Error('Use a coupling evaluation budget from 1 to 1000000.');
      const result=JSON.parse(kernel.UTF8ToString(kernel._coupling(first,second,limit)));if(result.error)throw Error(result.error);
      postMessage({id:data.id,coupling:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='analyze'){
      const analysis=JSON.parse(kernel.UTF8ToString(kernel._analyze(frequency(data))));
      if(analysis.error)throw Error(analysis.error);
      postMessage({id:data.id,analysis,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='three-port'){const result=await solveThreePort(data,frequency(data),task);if(task!==epoch)return;postMessage({id:data.id,three_port:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;}
    if(action==='transformer'){const result=solveTransformer(data,frequency(data));postMessage({id:data.id,transformer:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;}
    if(action==='network'){const result=solveNetwork(data,frequency(data));postMessage({id:data.id,network:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;}
    if(action==='resonance'){
      if(!processTechnology||!hasLayout)throw Error('Generate a layout before searching for self resonance.');
      const kind=kernel._shape_kind(data.winding??0);if(kind<0)throw Error('Select a valid winding for SELFRES.');
      const fast=Boolean(data.fast_quantization);if(fast&&kind!==4)throw Error('The original FAST model for this structure faults; use the numerical SLOW search.');
      if(data.mode!==undefined&&!['single','differential'].includes(data.mode))throw Error('Select single-ended or differential SELFRES mode.');
      const settings={};for(const [key,field]of [['first','start_ghz'],['second','end_ghz'],['step','step_ghz']]){if(typeof data[field]!=='number')throw Error('Enter SELFRES start, end and step frequencies in GHz.');settings[key]=frequency(data,data[field]);}
      settings.differential=data.mode==='differential';settings.fast=fast;settings.sample_budget=data.sample_budget??101;settings.native_iterations=data.native_iterations??10;
      if(!Number.isInteger(settings.sample_budget)||settings.sample_budget<1||settings.sample_budget>101||!Number.isInteger(settings.native_iterations)||settings.native_iterations<0||settings.native_iterations>1000)throw Error('Use 1–101 SELFRES analyses and a native iteration argument from 0 to 1000.');
      const history=[],source=kind===4?'CAP':'PI2';let plan=resonancePlan(kernel,settings,history);
      while(plan.status==='need_sample'){
        if(task!==epoch)return;const hz=plan.next_frequency_hz;let result;
        try{
          if(kind===4){
            const domain=processTechnology.domain,selection=solverCells(kernel,{...data,mesh:data.contact_mesh??{}},hz,'capacitance');
            const cap=meshResult(JSON.parse(kernel.UTF8ToString(kernel._simulate_capacitance(data.winding??0,data.ground??-1,hz,...selection.cells,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,0))),selection);
            result={name:cap.name,ground:cap.ground,frequency_hz:hz,matrix:capacitorPortMatrix(kernel,cap),mesh:cap.mesh,contacts:cap.contacts,...(cap.automatic_cells?{automatic_cells:cap.automatic_cells}:{})};
          }else result=solveNetwork({...data,model:'PI2'},hz);
        }catch(error){throw Error(`SELFRES analysis ${history.length+1} at ${hz} Hz: ${error.message}`);}
        history.push({frequency_hz:hz,matrix:result.matrix});plan=resonancePlan(kernel,settings,history);
        const done=plan.status!=='need_sample',sample={...portSampleSummary(result),source_model:source};
        postMessage({id:data.id,resonance:{...plan,source_model:source,sample,done},warnings:kernel.UTF8ToString(kernel._last_warnings())});
        if(done)return;await new Promise(resolve=>setTimeout(resolve,0));
      }
      return;
    }
    if(action==='port-sweep'){
      const target=data.port_kind??'network';if(!['network','transformer','three-port'].includes(target))throw Error('Select a network, transformer or three-port sweep.');
      if(!processTechnology||!hasLayout)throw Error('Generate a layout before running a port sweep.');
      const range=['start_ghz','stop_ghz','step_ghz'].map(key=>{if(typeof data[key]!=='number')throw Error('Enter start, stop and step frequencies in GHz.');return frequency(data,data[key]);});
      const limit=data.sample_limit??101;if(!Number.isInteger(limit))throw Error('Use an integer port sample limit.');
      const plan=JSON.parse(kernel.UTF8ToString(kernel._port_frequency_schedule(...range,limit)));if(plan.error)throw Error(plan.error);
      if(target==='three-port'&&data.backing&&plan.frequencies_hz.length>1)throw Error('A native-state capture is valid at one frequency. Clear the capture to sweep the numerical three-port model.');
      const solvePort=target==='three-port'?solveThreePort:target==='transformer'?solveTransformer:solveNetwork,field=target==='three-port'?'three_port':target;
      for(const [index,hz] of plan.frequencies_hz.entries()){
        if(task!==epoch)return;let result;
        try{result=target==='network'?solveNetwork(data,hz,undefined,true):await solvePort(data,hz,task);}catch(error){throw Error(`Port sample ${index+1}/${plan.frequencies_hz.length} at ${hz} Hz: ${error.message}`);}
        if(task!==epoch)return;
        const done=index===plan.frequencies_hz.length-1;
        const summary=portSampleSummary(result);
        postMessage({id:data.id,[field]:done?result:summary,port_sweep:{index,total:plan.frequencies_hz.length,done,target},warnings:[result.dispatch?.warning,kernel.UTF8ToString(kernel._last_warnings())].filter(Boolean).join('\n')});
        if(!done)await new Promise(resolve=>setTimeout(resolve,0));
      }
      return;
    }
    if(action==='capacitance'){
      if(!processTechnology)throw Error('Generate a layout before running CAP.');
      const domain=processTechnology.domain,hz=frequency(data),selection=solverCells(kernel,data,hz,'capacitance'),parsed=selection.cells;
      const result=JSON.parse(kernel.UTF8ToString(kernel._simulate_capacitance(data.winding??0,data.ground??-1,hz,...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.unfolded?1:0)));
      meshResult(result,selection);postMessage({id:data.id,capacitance:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='simulate'){
      const simulation=solve(data,frequency(data));postMessage({id:data.id,simulation,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='sweep'){
      if(!Array.isArray(data.frequencies_ghz) || data.frequencies_ghz.length<2 || data.frequencies_ghz.length>101)throw Error('Use 2–101 frequency samples.');
      const frequencies=data.frequencies_ghz.map(value=>frequency(data,value));
      if(new Set(frequencies).size!==frequencies.length)throw Error('Frequency range is too narrow at native ASITIC precision.');
      for(let index=0;index<frequencies.length;index++){
        if(task!==epoch)return;
        const simulation=solve(data,frequencies[index]),done=index===frequencies.length-1;
        // Stream compact results; send a complete matrix only for the final
        // sample. Selecting another point computes that sample on demand.
        const {matrix,...summary}=simulation;
        postMessage({id:data.id,simulation:done?simulation:summary,sweep:{index,total:frequencies.length,done},warnings:kernel.UTF8ToString(kernel._last_warnings())});
        if(!done)await new Promise(resolve=>setTimeout(resolve,0));
      }
      return;
    }
    if(action!=='generate')throw Error('Unknown workbench action.');
    const append=Boolean(data.options?.append);
    if(append && (!hasLayout || !data.creator || data.technology!==processTechnology?.source))throw Error('Add to layout requires an existing layout, an original creator command, and the same technology.');
    const technology=append?processTechnology:parseTechnology(data.technology);
    if(!append){hasLayout=false;currentLayout=null,optimizationKernel=null,optimizationState=null;configureKernel(kernel,technology);processTechnology=technology;}
    const layout=data.creator?createGeometry(kernel,data.creator.id,data.creator.keywords,data.creator.answers,{...data.options,domain:technology.domain}):generateGeometry(kernel,data.kind,data.parameters,data.options);hasLayout=true;currentLayout=layout;postMessage({id:data.id,layout,technology});
  }catch(error){postMessage({id:data.id,error:error.message,action,append:Boolean(data.options?.append)});}
};
postMessage({ready:true});
