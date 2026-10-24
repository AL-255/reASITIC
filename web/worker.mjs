import createKernel from './asitic.js';
import {parseTechnology,configureKernel,generateGeometry,createGeometry} from './technology.mjs';
const kernel=await createKernel();let epoch=0,processTechnology=null,hasLayout=false;
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
      if(!processTechnology || !hasLayout)throw Error('Generate a layout before running the combined network.');
      const domain=processTechnology.domain,mesh=data.mesh??{},contact=data.contact_mesh??{},e=data.eddy??{},layers=e.layers??processTechnology.eddy_layers??[];
      if(!Array.isArray(layers) || layers.length>2 || layers.some(i=>!Number.isInteger(i) || i<0))throw Error('Select up to two substrate layers.');
      const cells=[mesh.length??50,mesh.width??1.5,mesh.thickness??.5,contact.length??50,contact.width??20];
      const parsed=data.nativeInput?cells.map(value=>kernel._normalize_value(Number(value),7,data.grid??.1,data.snap===false?0:1)):cells;
      const result=JSON.parse(kernel.UTF8ToString(kernel._simulate_network(data.winding??0,data.ground??-1,frequency(data),...parsed,domain.x,domain.y,data.fft_x??domain.cells_x,data.fft_y??domain.cells_y,data.contact_limit??128,data.filament_limit??256,e.mode??0,layers.length,layers[0]??0,layers[1]??0,e.evaluation_limit??250000)));
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
    if(!append){hasLayout=false;configureKernel(kernel,technology);processTechnology=technology;}
    const layout=data.creator?createGeometry(kernel,data.creator.id,data.creator.keywords,data.creator.answers,{...data.options,domain:technology.domain}):generateGeometry(kernel,data.kind,data.parameters,data.options);hasLayout=true;postMessage({id:data.id,layout,technology});
  }catch(error){postMessage({id:data.id,error:error.message,action,append:Boolean(data.options?.append)});}
};
postMessage({ready:true});
