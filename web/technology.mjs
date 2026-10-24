export function parseTechnology(text) {
  const blocks={chip:[],layer:[],metal:[],via:[]},eddyLayers=[],warnings=[];let current,currentKind;
  for(const raw of text.split(/\r?\n/)) {
    const line=raw.split(';')[0].trim();if(!line)continue;
    const section=line.match(/^<(chip|layer|metal|via)>\s*(\d+)?$/i);
    if(section){const kind=section[1].toLowerCase();if(section[2]!==undefined && +section[2]!==blocks[kind].length)throw Error('Technology indices must be consecutive');current={};currentKind=kind;blocks[kind].push(current);continue;}
    const split=line.indexOf('=');if(!current || split<0)throw Error('Invalid technology line: '+line);
    const key=line.slice(0,split).trim().toLowerCase(),value=line.slice(split+1).trim();
    current[key]=['name','color','techfile','techpath'].includes(key)?value:Number(value);
    if(typeof current[key]==='number' && !Number.isFinite(current[key]))throw Error('Invalid technology number');
    if(currentKind==='chip' && key==='eddy'){if(!Number.isInteger(current[key]) || current[key]<0)throw Error('Invalid eddy layer index');if(eddyLayers.length<2)eddyLayers.push(current[key]);else warnings.push('Only the first two EDDY layer entries are used, as in native ASITIC.');}
  }
  if(!blocks.layer.length || blocks.layer.length>32 || blocks.layer.some(layer=>!Number.isFinite(layer.t) || layer.t<=0))throw Error('Technology needs 1–32 layers with positive thickness');
  if(!blocks.metal.length || blocks.metal.length>32 || blocks.via.length>32)throw Error('Technology needs 1–32 metals and at most 32 vias');
  const metals=blocks.metal.map((m,i)=>{
    if(!(m.t>0 && m.rsh>0 && m.d>=0 && blocks.layer[m.layer] && m.d<=blocks.layer[m.layer].t))throw Error('Invalid metal '+i);
    return {name:(m.name||`M${i+1}`).toUpperCase(),height:m.d,thickness:m.t,sheet_milliohms:m.rsh,layer:m.layer,minimum_spacing:m.smin??0,minimum_width:m.wmin??0};
  });
  const vias=blocks.via.map((v,i)=>{
    if(!Number.isInteger(v.bottom) || !Number.isInteger(v.top) || !metals[v.bottom] || !metals[v.top] || v.bottom===v.top || !(v.width>0 && v.space>=0 && v.r>=0 && v.overplot1>=0 && v.overplot2>=0))throw Error('Invalid via '+i);
    return {name:(v.name||`VIA${i+1}`).toUpperCase(),bottom:v.bottom,top:v.top,width:v.width,spacing:v.space,over1:v.overplot1,over2:v.overplot2,resistance:v.r};
  });
  const chip=blocks.chip[0]??{};
  const domain={x:chip.chipx??512,y:chip.chipy??512,cells_x:chip.fftx??128,cells_y:chip.ffty??128};
  if(!(domain.x>0 && domain.y>0 && Number.isInteger(domain.cells_x) && Number.isInteger(domain.cells_y) && domain.cells_x>0 && domain.cells_y>0))throw Error('Invalid chip dimensions or FFT cells');
  const dielectric=blocks.layer.map(layer=>({permittivity:layer.eps??11.7,resistivity:layer.rho}));
  if(dielectric.some(layer=>!(layer.permittivity>0 && layer.resistivity>0)))throw Error('Layer permittivity and resistivity must be positive');
  if(eddyLayers.some(i=>i>=blocks.layer.length))throw Error('Eddy layer index is outside the substrate stack');
  return {metals,vias,domain,dielectric,eddy_layers:eddyLayers,warnings,substrate:blocks.layer.map(layer=>layer.t),source:text};
}
export function configureKernel(kernel,technology) {
  kernel._reset_technology();
  technology.substrate.forEach((t,i)=>kernel._set_substrate(i,t));
  if(kernel._set_substrate_material)technology.dielectric?.forEach((layer,i)=>kernel._set_substrate_material(i,layer.permittivity,layer.resistivity));
  kernel._finalize_technology();
  technology.metals.forEach((m,i)=>kernel._set_metal(i,m.height,m.thickness,m.sheet_milliohms,m.layer));
  technology.metals.forEach((m,i)=>{
    const spacing=m.minimum_spacing??0,width=m.minimum_width??0;
    if(kernel._set_metal_rules){const rules=JSON.parse(kernel.UTF8ToString(kernel._set_metal_rules(i,spacing,width)));if(rules.error)throw Error(rules.error);Object.assign(m,rules);}
    else if(spacing!==0||width!==0)throw Error('This kernel cannot apply SMIN/WMIN process rules.');
  });
  technology.vias.forEach((v,i)=>kernel._set_via(i,v.bottom,v.top,v.width,v.spacing,v.over1,v.over2,v.resistance));
  if(kernel._set_layer_name)for(const [via,layers] of [[0,technology.metals],[1,technology.vias]])layers.forEach((layer,i)=>withString(kernel,layer.name,p=>kernel._set_layer_name(via,i,p)));

}
export function generateGeometry(kernel,kind,parameters,options={}) {
  let values=[...parameters],x=options.x??0,y=options.y??0;
  const parse=(value,type)=>kernel._normalize_value(value,type,options.grid??.1,options.snap===false?0:1);
  if(options.nativeInput){
    for(const i of [0,1,2,5,10])values[i]=parse(values[i],7);
    if(kind!==5)values[3]=parse(values[3],7);
    for(const i of [4,11])values[i]=parse(values[i],4);
    values[9]=parse(values[9],10);x=parse(x,7);y=parse(y,7);
  }
  kernel._set_geometry_options(x,y,options.phase??1);
  if(options.nativeInput && kind===5)kernel._set_ring_gap_radians(parse(parameters[3],10));
  const p=kernel._malloc(12*8);
  try{kernel.HEAPF64.set(values,p/8);const result=JSON.parse(kernel.UTF8ToString(kernel._generate(kind,p)));if(result.error)throw Error(result.error);result.warnings=kernel.UTF8ToString(kernel._last_warnings());return result;}
  finally{kernel._free(p);}
}

export function withString(kernel,value,callback) {
  const bytes=new TextEncoder().encode(value+'\0'),p=kernel._malloc(bytes.length);
  try { new Uint8Array(kernel.HEAPF64.buffer,p,bytes.length).set(bytes);return callback(p); }
  finally { kernel._free(p); }
}
export function createGeometry(kernel,id,keywords='',answers='',options={}) {
  const domain=options.domain??{x:512,y:512,cells_x:128,cells_y:128};
  return withString(kernel,keywords,line=>withString(kernel,answers,input=>{
    const result=JSON.parse(kernel.UTF8ToString(kernel._create_geometry(id,line,input,domain.x,domain.y,domain.cells_x,domain.cells_y,options.grid??.1,options.snap===false?0:1,options.append?1:0)));
    if(result.error)throw Error(result.error);result.warnings=kernel.UTF8ToString(kernel._last_warnings());return result;
  }));
}

export function editGeometry(kernel,commands,options={}) {
  if(typeof commands!=='string'||commands.includes('\0'))throw Error('Enter geometry edit commands without NUL characters.');
  const domain=options.domain??{x:512,y:512};
  return withString(kernel,commands,text=>{const result=JSON.parse(kernel.UTF8ToString(kernel._edit_geometry(text,domain.x,domain.y,options.grid??.1,options.snap===false?0:1)));if(result.error)throw Error(result.error);result.warnings=kernel.UTF8ToString(kernel._last_warnings());return result;});
}
