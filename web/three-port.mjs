const number=value=>value===null?'':Object.is(value,-0)?'-0':String(value);
export function validateThreePortBacking(value){
  if(value?.format!=='reasitic-three-port-backing-v1')throw Error('Use a native reASITIC three-port backing capture.');
  const n=value.node_count,bytes=n*n*16;
  if(!Number.isInteger(n)||n<1||n>512||!Number.isFinite(value.frequency_hz)||value.frequency_hz<=0)throw Error('Invalid captured frequency or node count.');
  if(!Array.isArray(value.names)||value.names.length!==3||new Set(value.names).size!==3||[...value.names,value.ground].some(name=>typeof name!=='string'||!name.length||name.length>76||name.includes('\0')))throw Error('Invalid captured winding/reference names.');
  if(!Array.isArray(value.addresses)||value.addresses.length!==5||value.addresses.some(address=>!Number.isInteger(address)||address<=0||address>0xffffffff))throw Error('Invalid captured native address layout.');
  for(const key of ['capacitance_hex','inductance_hex'])if(typeof value[key]!=='string'||value[key].length!==bytes*2||!/^[0-9a-f]+$/i.test(value[key]))throw Error('Captured backing arrays do not match the node count.');
  if(typeof value.technology_sha256!=='string'||!/^[0-9a-f]{64}$/i.test(value.technology_sha256))throw Error('Captured technology hash is missing.');
  return {format:value.format,names:[...value.names],ground:value.ground,frequency_hz:value.frequency_hz,node_count:n,addresses:[...value.addresses],capacitance_hex:value.capacitance_hex,inductance_hex:value.inductance_hex,technology_sha256:value.technology_sha256};
}
export function allocateThreePortBacking(kernel,value){
  const h=validateThreePortBacking(value),pointers=[];
  try{
    for(const hex of [h.capacitance_hex,h.inductance_hex]){const bytes=new Uint8Array(hex.length/2);for(let i=0;i<bytes.length;++i)bytes[i]=parseInt(hex.slice(i*2,i*2+2),16);const p=kernel._malloc(bytes.length);if(!p)throw Error('Unable to allocate captured backing.');pointers.push(p);new Uint8Array(kernel.HEAPF64.buffer,p,bytes.length).set(bytes);}
    const p=kernel._malloc(20);if(!p)throw Error('Unable to allocate captured address layout.');pointers.push(p);const view=new DataView(kernel.HEAPF64.buffer,p,20);h.addresses.forEach((address,i)=>view.setUint32(i*4,address,true));
    return {cap:pointers[0],ind:pointers[1],addresses:pointers[2],cells:h.node_count**2,release(){while(pointers.length)kernel._free(pointers.pop());}};
  }catch(error){while(pointers.length)kernel._free(pointers.pop());throw error;}
}
export function threePortRecord(result,parameter='z',polar=false){
  const record=(polar?result?.polar_records:result?.records)?.[parameter];if(typeof record!=='string')throw Error('Run the three-port solve before exporting its native record.');return record;
}
export function threePortMatrixCSV(result,parameter='z'){
  const key={z:'impedance_matrix',y:'admittance_matrix',s:'scattering_matrix'}[parameter],matrix=result?.[key];if(!matrix)throw Error('Run the three-port solve before exporting its raw matrix.');const units={z:'real_ohm,imag_ohm',y:'real_siemens,imag_siemens',s:'real,imag'}[parameter];
  return [`frequency_hz,row,column,${units}`,...matrix.flatMap((row,i)=>row.map((z,j)=>[result.frequency_hz,i,j,...z].map(number).join(',')))].join('\n')+'\n';
}
export function threePortMetricsCSV(result){
  if(!result?.model)throw Error('Run the three-port solve before exporting its model.');const m=result.model,values={frequency_hz:result.frequency_hz};
  for(let i=0;i<3;++i){values[`L${i+1}_h`]=m.inductance_h[i];values[`R${i+1}_ohm`]=m.resistance_ohm[i];}
  for(const [i,pair]of ['12','13','23'].entries()){values[`M${pair}_h`]=m.mutual_inductance_h[i];values[`R${pair}_ohm`]=m.cross_resistance_ohm[i];values[`k${pair}`]=m.coupling[i];}
  Object.assign(values,{eddy_mode:result.eddy.mode,eddy_layers:result.eddy.indices.join(';'),green_evaluations:result.eddy.evaluations,quadpack_diagnostics:result.eddy.quadrature_status.reduce((a,b)=>a+b,0),backing_captured:Boolean(result.backing?.captured)});
  return [Object.keys(values).join(','),Object.values(values).map(number).join(',')].join('\n')+'\n';
}
