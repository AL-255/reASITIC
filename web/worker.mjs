import createKernel from './asitic.js';
import {validateThreePortBacking,allocateThreePortBacking} from './three-port.mjs';
import {parseTechnology,configureKernel,generateGeometry,createGeometry} from './technology.mjs';
const kernel=await createKernel();let epoch=0,processTechnology=null,hasLayout=false,currentLayout=null;
function frequency(data,value=data.frequency_ghz) {
  const result=value===undefined?data.frequency:kernel._normalize_value(Number(value),5,.1,0);
  if(!Number.isFinite(result) || result<=0)throw Error('Enter a positive native ASITIC frequency.');return result;
}
function solve(data,hz) {
  const mesh=data.mesh??{};
  const cells=[mesh.length??50,mesh.width??1.5,mesh.thickness??.5,mesh.subdivision_trigger??mesh.length??50];
  const parsed=data.nativeInput?cells.map(value=>kernel._normalize_value(Number(value),7,data.grid??.1,data.snap===false?0:1)):cells;
  let pointer;
  if(data.eddy){
    const e=data.eddy,layers=e.layers??processTechnology?.eddy_layers??[];
    if(!Array.isArray(layers) || layers.length>2 || layers.some(i=>!Number.isInteger(i) || i<0))throw Error('Select up to two substrate layers.');
    pointer=kernel._simulate_eddy(data.winding??0,hz,...parsed,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,e.evaluation_limit??250000);
  }else pointer=kernel._simulate(data.winding??0,hz,...parsed,data.filament_limit??256);
  const result=JSON.parse(kernel.UTF8ToString(pointer));
  if(result.error)throw Error(result.error);return result;
}
self.onmessage=async({data})=>{
  const task=++epoch;const action=data.action??'generate';
  try{
    if(action==='cancel'){postMessage({id:data.id,cancelled:true});return;}
    if(action==='analyze'){
      const analysis=JSON.parse(kernel.UTF8ToString(kernel._analyze(frequency(data))));
      if(analysis.error)throw Error(analysis.error);
      postMessage({id:data.id,analysis,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='three-port'){
      if(!processTechnology || !hasLayout)throw Error('Generate three windings before running 3PORT.');
      const indices=data.windings??[0,1,2];if(!Array.isArray(indices)||indices.length!==3||new Set(indices).size!==3||indices.some(i=>!Number.isInteger(i)||i<0||!currentLayout.shapes[i]))throw Error('Select three distinct winding records.');
      const domain=processTechnology.domain,mesh=data.mesh??{},contact=data.contact_mesh??{},e=data.eddy??{},layers=e.layers??processTechnology.eddy_layers??[];
      if(!Array.isArray(layers)||layers.length>2||layers.some(i=>!Number.isInteger(i)||i<0))throw Error('Select up to two substrate layers.');
      const cells=[mesh.length??50,mesh.width??1.5,mesh.thickness??.5,contact.length??50,contact.width??20],parsed=data.nativeInput?cells.map(v=>kernel._normalize_value(Number(v),7,data.grid??.1,data.snap===false?0:1)):cells,hz=frequency(data),ground=data.ground??-1;
      const args=[...indices,ground,hz,...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,e.evaluation_limit??250000];let result;
      if(data.backing){
        const h=validateThreePortBacking(data.backing),names=indices.map(i=>currentLayout.shapes[i].name),reference=currentLayout.shapes[ground<0?indices[0]:ground]?.name;
        if(JSON.stringify(h.names)!==JSON.stringify(names)||h.ground!==reference||h.frequency_hz!==hz)throw Error('Captured backing winding order, reference or frequency differs from this request.');
        const digest=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(processTechnology.source))),v=>v.toString(16).padStart(2,'0')).join('');if(task!==epoch)return;if(digest!==h.technology_sha256)throw Error('Captured backing uses a different technology file.');
        const memory=allocateThreePortBacking(kernel,h);try{result=JSON.parse(kernel.UTF8ToString(kernel._probe_three_port(...args,memory.cap,memory.ind,memory.cells,memory.addresses)));}finally{memory.release();}
      }else result=JSON.parse(kernel.UTF8ToString(kernel._simulate_three_port(...args)));
      if(result.error)throw Error(result.error);postMessage({id:data.id,three_port:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='transformer'){
      if(!processTechnology || !hasLayout)throw Error('Generate two windings before running CALCTRANS.');
      const domain=processTechnology.domain,mesh=data.mesh??{},contact=data.contact_mesh??{},e=data.eddy??{},layers=e.layers??processTechnology.eddy_layers??[],termination=Number(data.termination_ohm??0);
      if(!Array.isArray(layers) || layers.length>2 || layers.some(i=>!Number.isInteger(i) || i<0))throw Error('Select up to two substrate layers.');
      if(!Number.isFinite(termination))throw Error('Use a finite transformer termination.');
      if(termination!==0 && (data.ground??-1)<0)throw Error('Choose an explicit ground for a nonzero CALCTRANS termination.');
      const cells=[mesh.length??50,mesh.width??1.5,mesh.thickness??.5,contact.length??50,contact.width??20];
      const parsed=data.nativeInput?cells.map(value=>kernel._normalize_value(Number(value),7,data.grid??.1,data.snap===false?0:1)):cells;
      const resistance=data.nativeInput?kernel._normalize_value(termination,4,.1,0):termination;
      const result=JSON.parse(kernel.UTF8ToString(kernel._simulate_transformer(data.primary??0,data.secondary??1,data.ground??-1,frequency(data),...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,resistance,e.evaluation_limit??250000)));
      if(result.error)throw Error(result.error);postMessage({id:data.id,transformer:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='network'){
      const model=data.model??'PI2';if(!['PI2','PIX','PI4'].includes(model))throw Error('Select PI2, PIX or PI4.');
      const solver=model==='PI4'?kernel._simulate_pad_network:model==='PIX'?kernel._simulate_extended_network:kernel._simulate_network;
      if(!processTechnology || !hasLayout)throw Error('Generate a layout before running the combined network.');
      const pads=model==='PI4'?(data.pads??[]):[];if(model==='PI4'&&(!Array.isArray(pads)||pads.length!==2||pads.some(i=>!Number.isInteger(i)||i<0||!currentLayout.shapes[i])))throw Error('Select valid first and second PI4 pad records.');
      const domain=processTechnology.domain,mesh=data.mesh??{},contact=data.contact_mesh??{},e=data.eddy??{},layers=e.layers??processTechnology.eddy_layers??[];
      if(!Array.isArray(layers) || layers.length>2 || layers.some(i=>!Number.isInteger(i) || i<0))throw Error('Select up to two substrate layers.');
      const cells=[mesh.length??50,mesh.width??1.5,mesh.thickness??.5,contact.length??50,contact.width??20];
      const parsed=data.nativeInput?cells.map(value=>kernel._normalize_value(Number(value),7,data.grid??.1,data.snap===false?0:1)):cells;
      const result=JSON.parse(kernel.UTF8ToString(solver(data.winding??0,...pads,data.ground??-1,frequency(data),...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,e.evaluation_limit??250000)));
      if(result.error)throw Error(result.error);postMessage({id:data.id,network:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    if(action==='capacitance'){
      if(!processTechnology)throw Error('Generate a layout before running CAP.');
      const domain=processTechnology.domain,mesh=data.mesh??{},cells=[mesh.length??50,mesh.width??20];
      const parsed=data.nativeInput?cells.map(value=>kernel._normalize_value(Number(value),7,data.grid??.1,data.snap===false?0:1)):cells;
      const result=JSON.parse(kernel.UTF8ToString(kernel._simulate_capacitance(data.winding??0,data.ground??-1,frequency(data),...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.unfolded?1:0)));
      if(result.error)throw Error(result.error);postMessage({id:data.id,capacitance:result,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
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
    if(!append){hasLayout=false;currentLayout=null;configureKernel(kernel,technology);processTechnology=technology;}
    const layout=data.creator?createGeometry(kernel,data.creator.id,data.creator.keywords,data.creator.answers,{...data.options,domain:technology.domain}):generateGeometry(kernel,data.kind,data.parameters,data.options);hasLayout=true;currentLayout=layout;postMessage({id:data.id,layout,technology});
  }catch(error){postMessage({id:data.id,error:error.message,action,append:Boolean(data.options?.append)});}
};
postMessage({ready:true});
