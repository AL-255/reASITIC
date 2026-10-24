import {withString} from './technology.mjs';
export function optimizationPlan(kernel,keywords,domain,options={}){
  if(typeof keywords!=='string'||keywords.includes('\0'))throw Error('Enter original SWEEP keywords without NUL characters.');
  const limit=options.candidate_limit??100;if(!Number.isInteger(limit)||limit<1||limit>1000)throw Error('Use a candidate budget from 1 to 1000.');
  const search=options.search??'SWEEP';if(!['SWEEP','SWEEPMM'].includes(search))throw Error('Select SWEEP or SWEEPMM.');
  const planner=search==='SWEEPMM'?kernel._optimization_multimetal_plan:kernel._optimization_plan;
  if(typeof planner!=='function')throw Error('This kernel does not support the selected search.');
  return withString(kernel,keywords,p=>{const result=JSON.parse(kernel.UTF8ToString(planner(p,domain.x,domain.y,options.grid??.1,options.snap===false?0:1,limit)));if(result.error)throw Error(result.error);return result;});
}
export function createOptimizationCandidate(kernel,name,descriptor,append=false){
  if(typeof name!=='string'||name.includes('\0'))throw Error('Use an optimizer candidate name without NUL characters.');
  if(!Array.isArray(descriptor)||descriptor.length!==10||descriptor.some(v=>!Number.isFinite(v)))throw Error('Use a complete optimizer descriptor.');
  const p=kernel._malloc(80);try{kernel.HEAPF64.set(descriptor,p/8);return withString(kernel,name,text=>{const result=JSON.parse(kernel.UTF8ToString(kernel._optimization_create(text,p,append?1:0)));if(result.error)throw Error(result.error);result.warnings=kernel.UTF8ToString(kernel._last_warnings());return result;});}finally{kernel._free(p);}
}
export function optimizationAccept(kernel,plan,pi){return Boolean(kernel._optimization_accept(pi.inductance_h,pi.quality_first,plan.inductance_min,plan.inductance_max,plan.minimum_quality));}
export function optimizationCSV(samples){
  const metrics=['inductance_h','resistance','capacitance_second_f','capacitance_first_f','resistance_second','resistance_first','quality_first','quality_second','quality_differential'];
  const columns=['index','accepted','length_or_radius_um','width_um','spacing_um','turns','frequency_hz',...metrics.map(k=>k.startsWith('resistance')?k+'_ohm':k),'error'];
  const quote=v=>'"'+String(v??'').replaceAll('"','""')+'"';
  return columns.join(',')+'\n'+samples.map(s=>[s.index,s.accepted,...s.descriptor.slice(4,8),s.frequency_hz,...metrics.map(key=>s.pi?.[key]),s.error].map(quote).join(',')).join('\n')+'\n';
}
