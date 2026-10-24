const number=value=>Object.is(value,-0)?'-0':String(value);
export function capacitanceMatrixCSV(result,potential=false) {
  const matrix=potential?result?.potential_matrix:result?.matrix;
  if(!matrix)throw Error('Run CAP before downloading its matrix.');
  const columns=potential?'real_ohm,imag_ohm':'real_siemens,imag_siemens';
  return [`frequency_hz,row,column,${columns}`,...matrix.flatMap((row,i)=>row.map((z,j)=>[result.frequency_hz,i,j,...z].map(number).join(',')))].join('\n')+'\n';
}
export function capacitanceDiagnostics(text) {
  const progress=new Set(['Calculating Green Function...','Computing FFTs...','.']);
  return [...new Set(text.split('\n').map(line=>line.trim()).filter(line=>line && !progress.has(line)))].join('\n');
}
