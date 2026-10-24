export function frequencySweep(startGHz,endGHz,count,logarithmic=true) {
    if(!Number.isFinite(startGHz) || !Number.isFinite(endGHz) || startGHz<=0 || endGHz<=startGHz || !Number.isInteger(count) || count<2 || count>101)
        throw Error('Use a positive increasing frequency range and 2–101 samples.');
    return Array.from({length:count},(_,i)=>i===0?startGHz:i===count-1?endGHz:logarithmic?startGHz*(endGHz/startGHz)**(i/(count-1)):startGHz+(endGHz-startGHz)*i/(count-1));
}
const number=value=>value===null?'':Object.is(value,-0)?'-0':String(value);
export function matrixCSV(result) {
    if(!result?.matrix)throw Error('Solve a sample before downloading its matrix.');
    const rows=['frequency_hz,row,column,real_ohm,imag_ohm'];
    result.matrix.forEach((row,i)=>row.forEach((z,j)=>rows.push([result.frequency_hz,i,j,...z].map(number).join(','))));
    return rows.join('\n')+'\n';
}
export function sweepCSV(samples) {
    if(!samples.length)throw Error('Run a simulation before downloading results.');
    return ['frequency_hz,inductance_h,resistance_ohm,reactance_ohm,series_q,filaments,conductors',...samples.map(s=>[s.frequency_hz,s.inductance_h,s.resistance,s.reactance,s.series_q,s.filaments,s.conductors].map(number).join(','))].join('\n')+'\n';
}
export function plotSeries(samples,key,logarithmic=true) {
    const data=samples.filter(s=>Number.isFinite(s.frequency_hz) && s.frequency_hz>0 && Number.isFinite(s[key])).toSorted((a,b)=>a.frequency_hz-b.frequency_hz);
    if(!data.length)return null;
    const x=data.map(s=>logarithmic?Math.log10(s.frequency_hz):s.frequency_hz),y=data.map(s=>s[key]);
    const xmin=Math.min(...x),xmax=Math.max(...x),low=Math.min(...y),high=Math.max(...y),pad=(high-low)*.12||Math.abs(high)*.1||1;
    const ymin=low-pad,ymax=high+pad;
    return {points:data.map((sample,i)=>({sample,x:xmax===xmin?.5:(x[i]-xmin)/(xmax-xmin),y:(y[i]-ymin)/(ymax-ymin)})),xRange:[data[0].frequency_hz,data.at(-1).frequency_hz],yRange:[ymin,ymax]};
}
