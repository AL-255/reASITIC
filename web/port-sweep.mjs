import {networkMetricsCSV,twoPortScatteringCSV} from './network.mjs';
import {transformerMetricsCSV} from './transformer.mjs';

export function portSampleSummary(result){
  const {capacitance_matrix,inductance_admittance_matrix,inductance_impedance_matrix,node_capacitance_matrix,currents,voltages,pad_capacitance_matrix,with_pad_admittance_matrix,...summary}=result;
  return summary;
}
function validate(samples){
  if(!Array.isArray(samples)||!samples.length)throw Error('Complete at least one port sample before exporting a sweep.');
  const first=samples[0],model=first.transformer?'CALCTRANS':first.model??'PI2';let previous=0;
  for(const sample of samples){
    if(!sample.two_port||!Number.isFinite(sample.frequency_hz)||sample.frequency_hz<=previous)throw Error('Port samples must have valid output and strictly increasing frequencies.');
    if((sample.transformer?'CALCTRANS':sample.model??'PI2')!==model||sample.name!==first.name||sample.ground!==first.ground||sample.secondary!==first.secondary||JSON.stringify(sample.pads)!==JSON.stringify(first.pads))throw Error('Export samples from the same port model and windings.');
    previous=sample.frequency_hz;
  }
}
export function portSweepRecord(samples,parameter='s',polar=false){
  validate(samples);let header,rows='';
  for(const sample of samples){
    const output=sample.two_port,headers=polar?output.polar_headers:output.headers,records=polar?output.polar_records:output.records;
    if(typeof headers?.[parameter]!=='string'||typeof records?.[parameter]!=='string')throw Error('Select an output parameter supported by this port model.');
    if(header===undefined)header=headers[parameter];else if(header!==headers[parameter])throw Error('Port column headers differ between samples.');
    rows+=records[parameter];
  }
  return header+rows+'\n';
}
function csv(samples,exportSample){
  validate(samples);let header;const rows=[];
  for(const sample of samples){const [columns,...data]=exportSample(sample).trimEnd().split('\n');if(header===undefined)header=columns;else if(header!==columns)throw Error('Port result columns differ between samples.');rows.push(...data);}
  const model=samples[0].transformer?'CALCTRANS':samples[0].model??'PI2';
  return ['model,'+header,...rows.map(row=>model+','+row)].join('\n')+'\n';
}
export const portSweepScatteringCSV=samples=>csv(samples,twoPortScatteringCSV);
export const portSweepMetricsCSV=samples=>csv(samples,samples?.[0]?.transformer?transformerMetricsCSV:networkMetricsCSV);
