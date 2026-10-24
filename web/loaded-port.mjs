export function loadedPortImpedance(kernel,result,options={}){
  if(!result?.matrix||!Number.isFinite(result.frequency_hz)||result.frequency_hz<=0)throw Error('Solve a two-port model before calculating loaded input impedance.');
  const port=options.port??1;if(!Number.isInteger(port)||(port!==1&&port!==2))throw Error('Select integer port 1 or 2.');
  const numbers=[options.load_real_ohm??50,options.load_imag_ohm??0,options.first_capacitance_f??0,options.second_capacitance_f??0];
  if(numbers.some(v=>typeof v!=='number'||!Number.isFinite(v)))throw Error('Use a finite complex load and shunt capacitances.');
  const parsed=options.nativeInput?numbers.map(v=>kernel._normalize_value(v,4,.1,0)):numbers;
  if(parsed.some(v=>!Number.isFinite(v)))throw Error('Load or capacitance exceeds the original input range.');
  const y=result.matrix;if(y.length!==2||y.some(row=>!Array.isArray(row)||row.length!==2||row.some(z=>!Array.isArray(z)||z.length!==2||z.some(v=>typeof v!=='number'||!Number.isFinite(v)))))throw Error('The two-port model must contain finite admittance values.');
  const pointer=kernel._malloc(64);if(!pointer)throw Error('Unable to allocate the two-port input matrix.');
  try{
    kernel.HEAPF64.set([...y[0][0],...y[1][0],...y[0][1],...y[1][1]],pointer/8);
    const calculated=JSON.parse(kernel.UTF8ToString(kernel._loaded_port_impedance(pointer,parsed[0],parsed[1],port,result.frequency_hz,parsed[2],parsed[3])));if(calculated.error)throw Error(calculated.error);
    return {...calculated,frequency_hz:result.frequency_hz,port,load_ohm:parsed.slice(0,2),shunt_capacitance_f:parsed.slice(2)};
  }finally{kernel._free(pointer);}
}
export function loadedPortCSV(result){
  if(!result?.impedance_ohm)throw Error('Calculate loaded input impedance before exporting it.');
  const columns=['frequency_hz','port','load_real_ohm','load_imag_ohm','first_shunt_capacitance_f','second_shunt_capacitance_f','input_real_ohm','input_imag_ohm','status'];
  const number=v=>v===null?'':Object.is(v,-0)?'-0':String(v);
  return columns.join(',')+'\n'+[result.frequency_hz,result.port,...result.load_ohm,...result.shunt_capacitance_f,...result.impedance_ohm,result.status].map(number).join(',')+'\n';
}
