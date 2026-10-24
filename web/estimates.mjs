export function conductorEstimatesCSV(analysis){
  if(!analysis||!Number.isFinite(analysis.frequency_hz)||!Array.isArray(analysis.windings))throw Error('Calculate conductor estimates before exporting.');
  const cell=value=>value===null?'':typeof value==='string'?'"'+value.replaceAll('"','""')+'"':String(value);
  const rows=[['frequency_hz','name','inductance_nh','resistance_dc_ohm','resistance_hf_ohm','quality_low_frequency','metal_area_um2']];
  for(const w of analysis.windings)rows.push([analysis.frequency_hz,w.name,w.inductance_nh,w.resistance_dc,w.resistance_hf,w.quality_low_frequency,w.metal_area_um2]);
  return rows.map(row=>row.map(cell).join(',')).join('\n')+'\n';
}
