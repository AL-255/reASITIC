// GDSII boundaries from native ribbon edges. User units: µm; database: 1 nm.
export const GDS_BYTE_BUDGET=64*1024*1024;
const LIBRARY_BYTES=110,BOUNDARY_BYTES=64;
export function* segmentRecords(segments){for(const segment of segments){yield segment;if(segment.branches)yield* segmentRecords(segment.branches);}}
// Retain a via grid as one affine primitive until an export actually needs
// its cuts. Previewing a million-cut grid therefore needs no million-item list.
export function* geometryPrimitives(layout,technology,{visibleOnly=false}={}){
  const n=technology.metals.length;
  for(const shape of layout.shapes){if(visibleOnly&&shape.hidden)continue;
    for(const s of segmentRecords(shape.segments)){
      const add=points=>points.map(([x,y])=>[x+shape.origin[0],y+shape.origin[1]]);
      if(s.metal<n){yield {kind:'polygon',layer:s.metal+1,points:add([s.points[2],s.points[3],s.points[5],s.points[4]])};continue;}
      const via=technology.vias[s.metal-n];if(!via)throw Error('Unknown via layer');
      const [a,b,c]=[s.points[2],s.points[3],s.points[4]],dx=b[0]-a[0],dy=b[1]-a[1],length=Math.hypot(dx,dy),height=Math.hypot(a[0]-c[0],a[1]-c[1]);
      if(!(length>0&&height>0))throw Error('Degenerate via geometry');
      const ex=[dx/length,dy/length],ey=[(a[0]-c[0])/height,(a[1]-c[1])/height];
      const array={kind:'via-array',layer:100+s.metal-n,base:c,offset:shape.origin,ex,ey,nx:s.nx,ny:s.ny,width:via.width,pitch:via.width+via.spacing};
      if(!Number.isInteger(s.nx)||!Number.isInteger(s.ny)||!Number.isFinite(array.width)||!Number.isFinite(array.pitch))throw Error('Invalid via grid');
      for(const [metal,over] of [[via.bottom,via.over1],[via.top,via.over2]])yield {kind:'polygon',layer:metal+1,points:arrayRectangle(array,-over,-over,length+over,height+over)};
      if(s.nx>0&&s.ny>0)yield array;
    }
  }
}
export function arrayRectangle(a,x0,y0,x1,y1){
  // Keep the former flat writer's operation order, including origin addition.
  return [[x0,y0],[x1,y0],[x1,y1],[x0,y1]].map(([x,y])=>[a.base[0]+x*a.ex[0]+y*a.ey[0]+a.offset[0],a.base[1]+x*a.ex[1]+y*a.ey[1]+a.offset[1]]);
}
export function gdsSize(layout,technology,{visibleOnly=false,maxBytes=GDS_BYTE_BUDGET}={}){
  if(!Number.isSafeInteger(maxBytes)||maxBytes<LIBRARY_BYTES||maxBytes>2147483647)throw Error('Use a GDS output budget between 110 bytes and 2 GiB');
  const limit=Math.floor((maxBytes-LIBRARY_BYTES)/BOUNDARY_BYTES);let count=0;
  for(const p of geometryPrimitives(layout,technology,{visibleOnly})){
    const next=p.kind==='via-array'?p.nx*p.ny:1;
    if(!Number.isSafeInteger(next)||next>limit-count)throw Error(`GDS export exceeds the ${maxBytes} byte output budget`);
    count+=next;
  }
  return {polygons:count,bytes:LIBRARY_BYTES+BOUNDARY_BYTES*count};
}
export function* polygons(layout,technology,options={}){
  gdsSize(layout,technology,options); // Reject oversized grids before expanding.
  for(const p of geometryPrimitives(layout,technology,options)){
    if(p.kind==='polygon'){yield {layer:p.layer,points:p.points};continue;}
    for(let x=0;x<p.nx;x++)for(let y=0;y<p.ny;y++)yield {layer:p.layer,points:arrayRectangle(p,x*p.pitch,y*p.pitch,x*p.pitch+p.width,y*p.pitch+p.width)};
  }
}
function real8(x){if(x===0)return new Uint8Array(8);let e=64,sign=x<0?128:0;x=Math.abs(x);while(x>=1){x/=16;e++;}while(x<1/16){x*=16;e--;}const bytes=new Uint8Array(8);bytes[0]=sign|e;let mantissa=BigInt(Math.round(x*2**56));for(let i=7;i>0;i--){bytes[i]=Number(mantissa&255n);mantissa>>=8n;}return bytes;}
export function writeGDS(layout,technology,options={}){
  const size=gdsSize(layout,technology,options);if(!size.polygons)throw Error('No geometry to export');
  const output=new Uint8Array(size.bytes),view=new DataView(output.buffer);let offset=0;
  const header=(kind,type,length)=>{view.setUint16(offset,length);output[offset+2]=kind;output[offset+3]=type;const start=offset;offset+=length;return start;};
  const rec=(kind,type=0,payload=new Uint8Array())=>{const start=header(kind,type,4+payload.length+(payload.length%2));output.set(payload,start+4);};
  const ints=(values,size)=>{const b=new Uint8Array(values.length*size),d=new DataView(b.buffer);values.forEach((x,i)=>size===2?d.setInt16(i*size,x):d.setInt32(i*size,x));return b;};
  const text=x=>new TextEncoder().encode(x),stamp=ints([2000,1,1,0,0,0,2000,1,1,0,0,0],2);
  rec(0,2,ints([600],2));rec(1,2,stamp);rec(2,6,text('reASITIC'));rec(3,5,new Uint8Array([...real8(.001),...real8(1e-9)]));rec(5,2,stamp);rec(6,6,text('COIL'));
  for(const {layer,points} of polygons(layout,technology,options)){
    const xy=points.map(p=>p.map(x=>Math.round(x*1000)));
    if(xy.some(p=>p.some(x=>!Number.isFinite(x)||x< -2147483648||x>2147483647)))throw Error('GDS coordinates exceed signed 32-bit range');
    if(new Set(xy.map(p=>p.join(','))).size<3)throw Error('Polygon collapses on the 1 nm export grid');
    header(8,0,4);view.setInt16(header(13,2,6)+4,layer);view.setInt16(header(14,2,6)+4,0);
    const start=header(16,3,44);for(let i=0;i<5;i++){view.setInt32(start+4+i*8,xy[i%4][0]);view.setInt32(start+8+i*8,xy[i%4][1]);}header(17,0,4);
  }
  rec(7);rec(4);if(offset!==size.bytes)throw Error('GDS size changed during export');return output;
}
