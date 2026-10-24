const cachedLimits=new WeakMap();
export function solverLimits(kernel){
  if(!cachedLimits.has(kernel)){
    const limits=JSON.parse(kernel.UTF8ToString(kernel._solver_limits()));
    if(!['filaments','matrix_bytes','green_evaluations','eddy_mode','eddy_layers','port_samples'].every(key=>Number.isInteger(limits[key])&&limits[key]>0&&limits[key]<=2147483647))throw Error('Part of the workbench is out of date. Reload to update it.');
    cachedLimits.set(kernel,Object.freeze(limits));
  }
  return cachedLimits.get(kernel);
}
function integer(value,minimum,maximum,label){
  if(!Number.isInteger(value)||value<minimum||value>maximum)throw Error(`Use an integer ${label} from ${minimum} to ${maximum}.`);
  return value;
}
export function windingIndex(value,fallback,layout,label='winding',reference=false){
  if(!layout?.shapes.length)throw Error('Generate a layout and select a valid winding.');
  return integer(value??fallback,reference?-1:0,layout.shapes.length-1,label);
}
export function filamentBudget(kernel,value){return integer(value??256,1,solverLimits(kernel).filaments,'filament limit');}
export function portSampleBudget(kernel,value){return integer(value??101,1,solverLimits(kernel).port_samples,'port sample limit');}
export function eddySettings(kernel,options,technology){
  const e=options===false?{}:options??{};
  if(typeof e!=='object'||Array.isArray(e))throw Error('Use an eddy settings object.');
  const limits=solverLimits(kernel),layers=e.layers??technology?.eddy_layers??[];
  if(!Array.isArray(layers)||layers.length>limits.eddy_layers)throw Error(`Select up to ${limits.eddy_layers} substrate layers.`);
  for(const i of layers)integer(i,0,(technology?.dielectric.length??0)-1,'substrate layer index');
  return {layers,mode:integer(e.mode??0,0,limits.eddy_mode,'eddy mode'),budget:integer(e.evaluation_limit??250000,1,limits.green_evaluations,'Green evaluation budget')};
}
