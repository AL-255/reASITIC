const number=value=>value===null?'':Object.is(value,-0)?'-0':String(value);
export function networkMatrixCSV(result){
  if(!result?.matrix)throw Error('Run the combined network before exporting its admittance.');
  // Original write_two_port_record labels y22 as port 1 and emits y21 twice.
  const y=result.matrix,native=[[y[1][1],y[1][0]],[y[1][0],y[0][0]]];
  return ['frequency_hz,row,column,real_siemens,imag_siemens',...native.flatMap((row,i)=>row.map((z,j)=>[result.frequency_hz,i,j,...z].map(number).join(',')))].join('\n')+'\n';
}
export function networkMetricsCSV(result){
  if(result?.model==='CAP'&&result.matrix){
    const columns=['frequency_hz',...['Y11','Y12','Y21','Y22'].flatMap(key=>[key+'_real_siemens',key+'_imag_siemens'])];
    return [columns.join(','),[result.frequency_hz,...result.matrix.flat(2)].map(number).join(',')].join('\n')+'\n';
  }
  if(!result?.pi)throw Error('Run the combined network before exporting the pi model.');
  const values={...result.pi,resonance_hz:result.resonance_hz,eddy_mode:result.eddy.mode,eddy_layer_count:result.eddy.layer_count,eddy_layers:result.eddy.indices.join(';'),green_evaluations:result.eddy.evaluations,quadpack_diagnostics:result.eddy.quadrature_status.reduce((a,b)=>a+b,0)};
  const columns=Object.keys(values).map(key=>['resistance','resistance_first','resistance_second','shunt_single','shunt_differential'].includes(key)?key+'_ohm':key);
  return [columns.join(','),Object.values(values).map(number).join(',')].join('\n')+'\n';
}

export function networkVectorsCSV(result){
  if(!result?.currents || !result?.voltages)throw Error('Run PIX before exporting current and voltage vectors.');
  return ['frequency_hz,node,current_real_ampere,current_imag_ampere,voltage_real_volt,voltage_imag_volt',...result.currents.map((row,i)=>[result.frequency_hz,i,...row[0],...result.voltages[i][0]].map(number).join(','))].join('\n')+'\n';
}

export function twoPortRecord(result,parameter='s',polar=false){
  const output=result?.two_port,records=polar?output?.polar_records:output?.records,headers=polar?output?.polar_headers:output?.headers;
  if(!records || typeof records[parameter]!=='string' || typeof headers?.[parameter]!=='string')throw Error('Solve the selected two-port model before exporting this native record.');
  return headers[parameter]+records[parameter]+'\n';
}
export function twoPortScatteringCSV(result){
  const matrix=result?.two_port?.scattering_matrix;
  if(!matrix)throw Error('Run a two-port solve before exporting scattering parameters.');
  return ['frequency_hz,row,column,reference_ohm,real,imag',...matrix.flatMap((row,i)=>row.map((z,j)=>[result.frequency_hz,i,j,result.two_port.reference_ohm,...z].map(number).join(',')))].join('\n')+'\n';
}
