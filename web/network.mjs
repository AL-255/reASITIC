const number=value=>value===null?'':Object.is(value,-0)?'-0':String(value);
export function networkMatrixCSV(result){
  if(!result?.matrix)throw Error('Run the combined network before exporting its admittance.');
  return ['frequency_hz,row,column,real_siemens,imag_siemens',...result.matrix.flatMap((row,i)=>row.map((z,j)=>[result.frequency_hz,i,j,...z].map(number).join(',')))].join('\n')+'\n';
}
export function networkMetricsCSV(result){
  if(!result?.pi)throw Error('Run the combined network before exporting the pi model.');
  const values={...result.pi,resonance_hz:result.resonance_hz,eddy_mode:result.eddy.mode,eddy_layer_count:result.eddy.layer_count,eddy_layers:result.eddy.indices.join(';'),green_evaluations:result.eddy.evaluations,quadpack_diagnostics:result.eddy.quadrature_status.reduce((a,b)=>a+b,0)};
  const columns=Object.keys(values).map(key=>['resistance','resistance_first','resistance_second','shunt_single','shunt_differential'].includes(key)?key+'_ohm':key);
  return [columns.join(','),Object.values(values).map(number).join(',')].join('\n')+'\n';
}
