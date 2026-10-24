import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {portSampleSummary,portSweepRecord} from '../web/port-sweep.mjs';

const source=readFileSync('web/technology/CMOS.tek','utf8'),fixture=JSON.parse(readFileSync('tests/fixtures/original-automatic-cells.json','utf8'));
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),inbox=[],waiters=[];
worker.on('message',data=>waiters.length?waiters.shift()(data):inbox.push(data));
function next(){if(inbox.length)return Promise.resolve(inbox.shift());return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Automatic-cell worker timeout')),30000);waiters.push(data=>{clearTimeout(timer);resolve(data);});});}
let id=0,independentThreePort;
async function send(data){worker.postMessage({id:++id,...data});const result=await next();assert.equal(result.id,id);return result;}
async function checked(data){const result=await send(data);assert.ok(!result.error,result.error);return result;}
async function sweep(data,field){worker.postMessage({id:++id,...data});const frames=[];do{const r=await next();assert.equal(r.id,id);assert.ok(!r.error,r.error);frames.push(r);}while(!(frames.at(-1).port_sweep??frames.at(-1).sweep).done);return frames.map(r=>r[field]);}
function withoutPolicy(result){const {automatic_cells,...rest}=result;return rest;}
function manual(data,result){return {...data,nativeInput:false,automatic_cells:false,frequency_ghz:undefined,frequency:result.frequency_hz,mesh:result.automatic_cells.mesh,contact_mesh:result.automatic_cells.contact_mesh};}
try{
  assert.equal((await next()).ready,true);
  for(const item of fixture.cases){
    let technology=source;
    for(const [key,value]of [['chipx',fixture.domain_um],['chipy',fixture.domain_um],['fftx',fixture.fft_cells],['ffty',fixture.fft_cells]])technology=technology.replace(new RegExp('^\\s*'+key+'\\s*=.*$','mi'),key+' = '+value);
    if(item.thickness){const start=technology.indexOf('<metal> 4'),next=technology.indexOf('\n<',start+1),end=next<0?technology.length:next;let top=technology.slice(start,end);for(const [key,value]of [['t',item.thickness],['rsh',item.sheet_milliohms]])top=top.replace(new RegExp('^(\\s*'+key+'\\s*=\\s*)[^\\n]+','mi'),(_,prefix)=>prefix+value);technology=technology.slice(0,start)+top+technology.slice(end);}
    await checked({technology,creator:{id:2,keywords:'NAME=A:LEN=20:WID=2:METAL=M5:XORG=4:YORG=8'}});
    const request={port_kind:'network',model:'PI2',winding:0,ground:0,automatic_cells:{alpha:item.alpha,beta:item.beta},nativeInput:true,grid:.1,snap:true,fft_x:fixture.fft_cells,fft_y:fixture.fft_cells,filament_limit:256,contact_limit:128,eddy:{mode:0},mesh:{length:NaN,width:NaN,thickness:NaN},contact_mesh:{length:NaN,width:NaN}};
    const samples=await sweep({...request,action:'port-sweep',start_ghz:.5,stop_ghz:1.5,step_ghz:.5},'network');
    assert.equal(samples.length,3);
    const record=portSweepRecord(samples,'s'),actualLines=record.trim().split('\n'),originalLines=item.scattering_record.trim().split('\n');
    assert.equal(actualLines[0],originalLines[0]);assert.equal(actualLines.length,originalLines.length);
    for(let i=1;i<actualLines.length;i++)assert.deepEqual(actualLines[i].trim().split(/\s+/).map(v=>Number(v).toPrecision(8)),originalLines[i].trim().split(/\s+/).map(v=>Number(v).toPrecision(8)),item.name+' production S accuracy');
    assert.equal(new Set(samples.map(s=>s.automatic_cells.mesh.width)).size,3,'Sweep reused a cell width');
    for(const result of samples){
      const policy=result.automatic_cells;assert.equal(policy.frequency_hz,result.frequency_hz);assert.equal(result.mesh.width,policy.mesh.width);assert.equal(result.mesh.contact_width,policy.contact_mesh.width);
      const plain=(await checked({...manual(request,result),action:'network'})).network,{dispatch,...summary}=portSampleSummary(result);assert.deepEqual(portSampleSummary(plain),withoutPolicy(summary),'Automatic limits differ from direct doubles');
      if(dispatch)assert.equal(dispatch.model,request.model??'PI2');
    }
  }
  let technology=source;
  for(const [key,value]of [['chipx',32],['chipy',32],['fftx',64],['ffty',64]])technology=technology.replace(new RegExp('^\\s*'+key+'\\s*=.*$','mi'),key+' = '+value);
  const creators=[['A',20,2,'M5',4,8],['B',20,2,'M3',4,16],['C',20,2,'M1',4,24],['P1',2,2,'M5',2,8],['P2',2,2,'M5',24,8],['G',20,2,'M2',4,8]];
  let layout;
  for(const [name,length,width,metal,x,y]of creators)layout=(await checked({technology,creator:{id:2,keywords:`NAME=${name}:LEN=${length}:WID=${width}:METAL=${metal}:XORG=${x}:YORG=${y}`},options:{append:Boolean(layout)}})).layout;
  const index=name=>layout.shapes.findIndex(s=>s.name===name),base={winding:index('A'),ground:index('G'),primary:index('A'),secondary:index('B'),windings:['A','B','C'].map(index),pads:['P1','P2'].map(index),automatic_cells:{alpha:.0001,beta:2.5},frequency_ghz:1,nativeInput:true,fft_x:64,fft_y:64,filament_limit:256,contact_limit:128,eddy:{mode:0},mesh:{length:NaN,width:NaN,thickness:NaN},contact_mesh:{length:NaN,width:NaN}};
  for(const [action,field,model]of [['simulate','simulation'],['capacitance','capacitance'],['network','network','PI2'],['network','network','PI3'],['network','network','PIX'],['network','network','PI4'],['transformer','transformer'],['three-port','three_port']]){
    const request={...base,action,model},automatic=(await checked(request))[field],plain=(await checked(manual(request,automatic)))[field];
    assert.deepEqual(withoutPolicy(automatic),plain,action+' '+(model??'')+' direct-double mesh');
  }
  for(const [target,field]of [['network','network'],['transformer','transformer'],['three-port','three_port']]){
    const request={...base,port_kind:target,model:'PI2',frequency_ghz:undefined},samples=await sweep({...request,action:'port-sweep',start_ghz:.5,stop_ghz:1.5,step_ghz:.5},field);
    assert.equal(new Set(samples.map(s=>s.automatic_cells.mesh.length)).size,3);
    if(target==='three-port'){
      const actual=samples.at(-1);assert.ok(actual.node_count>3,'Automatic longitudinal cells did not subdivide the windings');
      const oracle=JSON.parse(execFileSync(process.env.MPMATH_PYTHON??'python3',['tools/reference_three_port.py'],{input:JSON.stringify(actual),encoding:'utf8'})),scale=Math.max(...oracle.impedance_matrix.flat(2).map(Math.abs));
      let maximum=0;actual.impedance_matrix.flat(2).forEach((value,i)=>{const error=Math.abs(value-oracle.impedance_matrix.flat(2)[i]);maximum=Math.max(maximum,error);assert.ok(error<=scale*1e-10,'Subdivided automatic mesh differs from independent nodal reduction');});
      independentThreePort={node_count:actual.node_count,winding_groups:actual.winding_groups,max_absolute_impedance_error:maximum};
    }
    for(const sample of samples){const single=(await checked({...manual(request,sample),action:target}))[field],{dispatch,...summary}=portSampleSummary(sample);assert.deepEqual(withoutPolicy(summary),portSampleSummary(single));if(target==='network')assert.equal(dispatch.model,request.model??'PI2');}
  }
  const inductance=await sweep({...base,action:'sweep',frequencies_ghz:[.5,1,1.5]},'simulation');
  for(const sample of inductance){const single=(await checked({...manual(base,sample),action:'simulate'})).simulation;const {matrix,...summary}=single;assert.deepEqual(withoutPolicy(sample),sample.matrix?single:summary);}
  const request={...base,action:'network',model:'PI2'},expected=(await checked(request)).network;
  for(const change of [{automatic_cells:{alpha:0,beta:1}},{automatic_cells:{alpha:1,beta:-1}},{automatic_cells:true},{automatic_cells:{alpha:NaN,beta:1}},{automatic_cells:{alpha:.00000001,beta:1}},{eddy:{mode:1,layers:[0,1],evaluation_limit:1}}]){
    assert.ok((await send({...request,...change})).error);assert.deepEqual((await checked(request)).network,expected,'Invalid automatic mesh contaminated next solve');
  }
  const coarse=technology.replace(/^\s*fftx\s*=.*$/mi,'fftx = 1').replace(/^\s*ffty\s*=.*$/mi,'ffty = 1');
  await checked({technology:coarse,creator:{id:2,keywords:'NAME=A:LEN=20:WID=2:METAL=M5:XORG=4:YORG=8'}});
  assert.match((await send({...base,action:'network',winding:0,ground:0,fft_x:1,fft_y:1})).error,/collapses|degenerate/,'Automatic cells must not silently change FFT or coarsen cell sizes');
  mkdirSync('build/evidence',{recursive:true});writeFileSync('build/evidence/worker-automatic-cells.json',JSON.stringify({solver_paths:8,sweep_families:4,original_sweep_comparisons:2,independent_three_port:independentThreePort,invalid_mesh_recovery:true},null,2)+'\n');
  console.log('Automatic mesh worker: two complete original S sweeps, all eight solver paths, four sweep families, direct-double equivalence and invalid mesh recovery pass');
}finally{await worker.terminate();}
