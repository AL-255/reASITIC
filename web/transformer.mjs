const number=value=>value===null?'':Object.is(value,-0)?'-0':String(value);
export function transformerImpedanceCSV(result){
  if(!result?.impedance_matrix)throw Error('Run CALCTRANS before exporting its impedance.');
  return ['frequency_hz,row,column,real_ohm,imag_ohm',...result.impedance_matrix.flatMap((row,i)=>row.map((z,j)=>[result.frequency_hz,i,j,...z].map(number).join(',')))].join('\n')+'\n';
}
export function transformerMetricsCSV(result){
  if(!result?.transformer)throw Error('Run CALCTRANS before exporting its model.');
  const values={frequency_hz:result.frequency_hz,...result.transformer,termination_ohm:result.termination_ohm,eddy_mode:result.eddy.mode,eddy_layer_count:result.eddy.layer_count,eddy_layers:result.eddy.indices.join(';'),green_evaluations:result.eddy.evaluations,quadpack_diagnostics:result.eddy.quadrature_status.reduce((a,b)=>a+b,0)};
  return [Object.keys(values).join(','),Object.values(values).map(number).join(',')].join('\n')+'\n';
}
