import {geometryPrimitives,arrayRectangle} from './gds.mjs';
const SVG='http://www.w3.org/2000/svg';
// Native via grids remain repeated vector tiles, including after rotation or
// reflection. DOM size depends on conductor records, not the number of cuts.
export function renderGeometryPreview(svg,layout,technology,{colors,idPrefix='layout-via-'}={}){
  if(!Array.isArray(colors)||!colors.length||!/^[-_a-zA-Z][-_a-zA-Z0-9]*$/.test(idPrefix))throw Error('Invalid preview style');
  const element=(tag,attributes={})=>{const node=svg.ownerDocument.createElementNS(SVG,tag);for(const [key,value] of Object.entries(attributes))node.setAttribute(key,String(value));return node;};
  const nodes=[],defs=element('defs'),layers=new Set();let bounds=null,arrays=0,polygons=0,cuts=0;
  const include=points=>{for(const [x,y]of points){if(!Number.isFinite(x)||!Number.isFinite(y))throw Error('Nonfinite preview geometry');if(!bounds)bounds=[x,y,x,y];else{bounds[0]=Math.min(bounds[0],x);bounds[1]=Math.min(bounds[1],y);bounds[2]=Math.max(bounds[2],x);bounds[3]=Math.max(bounds[3],y);}}};
  const polygon=(points,color,outline=true)=>{include(points);nodes.push(element('polygon',{points:points.map(([x,y])=>`${x},${-y}`).join(' '),fill:color,'fill-opacity':'.77',...(outline?{stroke:color,'stroke-width':'.15'}:{})}));polygons++;};
  for(const p of geometryPrimitives(layout,technology,{visibleOnly:true})){
    layers.add(p.layer);const color=colors[(p.layer-1)%colors.length];
    if(p.kind==='polygon'){polygon(p.points,color);continue;}
    cuts+=p.nx*p.ny;
    if(p.nx*p.ny<=64){for(let x=0;x<p.nx;x++)for(let y=0;y<p.ny;y++)polygon(arrayRectangle(p,x*p.pitch,y*p.pitch,x*p.pitch+p.width,y*p.pitch+p.width),color);continue;}
    if(p.pitch===0){polygon(arrayRectangle(p,0,0,p.width,p.width),color,false);continue;}
    const x0=Math.min(0,(p.nx-1)*p.pitch),y0=Math.min(0,(p.ny-1)*p.pitch),width=Math.abs((p.nx-1)*p.pitch)+p.width,height=Math.abs((p.ny-1)*p.pitch)+p.width;
    include(arrayRectangle(p,x0,y0,x0+width,y0+height));
    // Normalize tile coordinates to a fixed vector viewport. Submicron cuts
    // remain resolved without making the tile surface depend on chip size.
    const tile=1024,scale=Math.abs(p.pitch)/tile,cut=p.width/Math.abs(p.pitch)*tile;
    const origin=arrayRectangle(p,x0,y0,x0,y0)[0],id=idPrefix+arrays++,pattern=element('pattern',{id,patternUnits:'userSpaceOnUse',patternContentUnits:'userSpaceOnUse',patternTransform:`scale(${scale})`,x:0,y:0,width:tile,height:tile});
    pattern.append(element('rect',{x:0,y:0,width:cut,height:cut,fill:color}));defs.append(pattern);
    const group=element('g',{transform:`matrix(${p.ex[0]} ${-p.ex[1]} ${p.ey[0]} ${-p.ey[1]} ${origin[0]} ${-origin[1]})`});
    group.append(element('rect',{x:0,y:0,width,height,fill:`url(#${id})`,'fill-opacity':'.77'}));nodes.push(group);
  }
  svg.replaceChildren(...(arrays?[defs]:[]),...nodes);
  return {bounds,layers:[...layers].sort((a,b)=>a-b),arrays,polygons,cuts};
}
