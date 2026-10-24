// GDSII boundaries from native ribbon edges. User units: µm; database: 1 nm.
export function* polygons(layout,technology) {
  const n=technology.metals.length;
  for(const shape of layout.shapes)for(const s of shape.segments) {
    const add=points=>points.map(([x,y])=>[x+shape.origin[0],y+shape.origin[1]]);
    if(s.metal<n){yield {layer:s.metal+1,points:add([s.points[2],s.points[3],s.points[5],s.points[4]])};continue;}
    const via=technology.vias[s.metal-n];if(!via)throw Error('Unknown via layer');
    const [a,b,c]=[s.points[2],s.points[3],s.points[4]],dx=b[0]-a[0],dy=b[1]-a[1],length=Math.hypot(dx,dy),height=Math.hypot(a[0]-c[0],a[1]-c[1]);
    if(!(length>0 && height>0))throw Error('Degenerate via geometry');
    const ex=[dx/length,dy/length],ey=[(a[0]-c[0])/height,(a[1]-c[1])/height];
    const rect=(x0,y0,x1,y1)=>add([[x0,y0],[x1,y0],[x1,y1],[x0,y1]].map(([x,y])=>[c[0]+x*ex[0]+y*ey[0],c[1]+x*ex[1]+y*ey[1]]));
    for(const [metal,over] of [[via.bottom,via.over1],[via.top,via.over2]])yield {layer:metal+1,points:rect(-over,-over,length+over,height+over)};
    for(let x=0;x<s.nx;x++)for(let y=0;y<s.ny;y++){const pitch=via.width+via.spacing;yield {layer:100+s.metal-n,points:rect(x*pitch,y*pitch,x*pitch+via.width,y*pitch+via.width)};}
  }
}
function real8(x){if(x===0)return new Uint8Array(8);let e=64,sign=x<0?128:0;x=Math.abs(x);while(x>=1){x/=16;e++;}while(x<1/16){x*=16;e--;}const bytes=new Uint8Array(8);bytes[0]=sign|e;let mantissa=BigInt(Math.round(x*2**56));for(let i=7;i>0;i--){bytes[i]=Number(mantissa&255n);mantissa>>=8n;}return bytes;}
export function writeGDS(layout,technology) {
  const records=[];
  const rec=(kind,type=0,payload=new Uint8Array())=>{const b=new Uint8Array(4+payload.length+(payload.length%2)),d=new DataView(b.buffer);d.setUint16(0,b.length);b[2]=kind;b[3]=type;b.set(payload,4);records.push(b);};
  const ints=(values,size)=>{const b=new Uint8Array(values.length*size),d=new DataView(b.buffer);values.forEach((x,i)=>size===2?d.setInt16(i*size,x):d.setInt32(i*size,x));return b;};
  const text=x=>new TextEncoder().encode(x),stamp=ints([2000,1,1,0,0,0,2000,1,1,0,0,0],2);
  rec(0,2,ints([600],2));rec(1,2,stamp);rec(2,6,text('reASITIC'));rec(3,5,new Uint8Array([...real8(.001),...real8(1e-9)]));rec(5,2,stamp);rec(6,6,text('COIL'));
  let count=0;
  for(const {layer,points} of polygons(layout,technology)) {
    const xy=points.map(p=>p.map(x=>Math.round(x*1000)));
    if(xy.some(p=>p.some(x=>!Number.isFinite(x) || x< -2147483648 || x>2147483647)))throw Error('GDS coordinates exceed signed 32-bit range');
    if(new Set(xy.map(p=>p.join(','))).size<3)throw Error('Polygon collapses on the 1 nm export grid');
    xy.push(xy[0]);rec(8);rec(13,2,ints([layer],2));rec(14,2,ints([0],2));rec(16,3,ints(xy.flat(),4));rec(17);count++;
  }
  if(!count)throw Error('No geometry to export');rec(7);rec(4);
  const output=new Uint8Array(records.reduce((n,b)=>n+b.length,0));let offset=0;for(const b of records){output.set(b,offset);offset+=b.length;}return output;
}
