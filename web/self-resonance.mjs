export function resonancePlan(kernel,settings,samples){
  if(!Array.isArray(samples)||samples.length>101||samples.some(s=>!Number.isFinite(s.frequency_hz)||s.frequency_hz<=0||s.matrix?.length!==2||s.matrix.some(row=>row.length!==2||row.some(z=>!Array.isArray(z)||z.length!==2||z.some(v=>!Number.isFinite(v))))))throw Error('Use finite 2x2 admittance matrices at their saved positive frequencies.');
  if(!Number.isInteger(settings.sample_budget??101)||!Number.isInteger(settings.native_iterations??10))throw Error('Use integer SELFRES sample and iteration limits.');
  const values=samples.flatMap(s=>[s.frequency_hz,...s.matrix.flat(2)]),pointer=values.length?kernel._malloc(values.length*8):0;
  try{
    if(values.length)kernel.HEAPF64.set(values,pointer/8);
    const result=JSON.parse(kernel.UTF8ToString(kernel._self_resonance_plan(settings.first,settings.second,settings.step,settings.differential?1:0,settings.fast?1:0,settings.native_iterations??10,settings.sample_budget??101,pointer,samples.length)));
    if(result.error)throw Error(result.error);return result;
  }finally{if(pointer)kernel._free(pointer);}
}
function capacitorPorts(kernel,result,records){
  if(result.matrix?.length!==2||result.matrix.some(row=>row.length!==2||row.some(z=>!Array.isArray(z)||z.length!==2||z.some(v=>!Number.isFinite(v)))))throw Error('The original capacitor model needs a folded 2x2 matrix.');
  const pointer=kernel._malloc(64);
  try{kernel.HEAPF64.set(result.matrix.flat(2),pointer/8);const port=JSON.parse(kernel.UTF8ToString(records?kernel._capacitor_port_response(pointer,result.frequency_hz):kernel._capacitor_port_matrix(pointer)));if(port.error)throw Error(port.error);return port;}
  finally{kernel._free(pointer);}
}
export const capacitorPortMatrix=(kernel,result)=>capacitorPorts(kernel,result,false).matrix;
export const capacitorPortResponse=(kernel,result)=>capacitorPorts(kernel,result,true);
export function resonanceHistoryCSV(samples,plan){
  if(!samples.length)throw Error('Complete at least one SELFRES analysis before exporting.');
  const number=x=>x===undefined?'':Object.is(x,-0)?'-0':String(x),mode=plan?.differential?'differential':'single';
  const columns=['analysis','source_model','mode','frequency_hz','used_by_native_iteration','imaginary_impedance_ohm','Y11_real_siemens','Y11_imag_siemens','Y12_real_siemens','Y12_imag_siemens','Y21_real_siemens','Y21_imag_siemens','Y22_real_siemens','Y22_imag_siemens'];
  return [columns.join(','),...samples.map((s,i)=>[i+1,s.source_model??'PI2',mode,s.frequency_hz,Boolean(plan?.trace[i]),plan?.trace[i]?.imaginary_impedance_ohm,...s.matrix.flat(2)].map(number).join(','))].join('\n')+'\n';
}
