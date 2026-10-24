export function parseTechnology(text) {
  const blocks={chip:[],layer:[],metal:[],via:[]};let current;
  for(const raw of text.split(/\r?\n/)) {
    const line=raw.split(';')[0].trim();if(!line)continue;
    const section=line.match(/^<(chip|layer|metal|via)>\s*(\d+)?$/i);
    if(section){const kind=section[1].toLowerCase();if(section[2]!==undefined && +section[2]!==blocks[kind].length)throw Error('Technology indices must be consecutive');current={};blocks[kind].push(current);continue;}
    const split=line.indexOf('=');if(!current || split<0)throw Error('Invalid technology line: '+line);
    const key=line.slice(0,split).trim().toLowerCase(),value=line.slice(split+1).trim();
    current[key]=['name','color','techfile','techpath'].includes(key)?value:Number(value);
    if(typeof current[key]==='number' && !Number.isFinite(current[key]))throw Error('Invalid technology number');
  }
  if(!blocks.metal.length || blocks.metal.length>32 || blocks.via.length>32)throw Error('Technology needs 1–32 metals and at most 32 vias');
  const metals=blocks.metal.map((m,i)=>{
    if(!(m.t>0 && m.rsh>0 && m.d>=0 && blocks.layer[m.layer] && m.d<=blocks.layer[m.layer].t))throw Error('Invalid metal '+i);
    return {name:(m.name||`M${i+1}`).toUpperCase(),height:m.d,thickness:m.t,resistance:m.rsh*.001};
  });
  const vias=blocks.via.map((v,i)=>{
    if(!Number.isInteger(v.bottom) || !Number.isInteger(v.top) || !metals[v.bottom] || !metals[v.top] || v.bottom===v.top || !(v.width>0 && v.space>=0 && v.r>=0 && v.overplot1>=0 && v.overplot2>=0))throw Error('Invalid via '+i);
    return {name:(v.name||`VIA${i+1}`).toUpperCase(),bottom:v.bottom,top:v.top,width:v.width,spacing:v.space,over1:v.overplot1,over2:v.overplot2,resistance:v.r};
  });
  return {metals,vias,source:text};
}
export function configureKernel(kernel,technology) {
  kernel._reset_technology();
  technology.metals.forEach((m,i)=>kernel._set_metal(i,m.height,m.thickness,m.resistance));
  technology.vias.forEach((v,i)=>kernel._set_via(i,v.bottom,v.top,v.width,v.spacing,v.over1,v.over2,v.resistance));
}
export function generateGeometry(kernel,kind,parameters) {
  const p=kernel._malloc(12*8);
  try{kernel.HEAPF64.set(parameters,p/8);const result=JSON.parse(kernel.UTF8ToString(kernel._generate(kind,p)));if(result.error)throw Error(result.error);result.warnings=kernel.UTF8ToString(kernel._last_warnings());return result;}
  finally{kernel._free(p);}
}
