// The webpage's exact default analysis task, in a fresh production worker.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {Worker} from 'node:worker_threads';
const text=readFileSync('web/technology/CMOS.tek','utf8');
const bootstrap=`import {parentPort} from 'node:worker_threads';globalThis.self=globalThis;globalThis.postMessage=m=>parentPort.postMessage(m);await import(${JSON.stringify(new URL('../dist/worker.mjs',import.meta.url).href)});parentPort.on('message',data=>self.onmessage({data}));`;
const start=performance.now(),worker=new Worker(new URL('data:text/javascript,'+encodeURIComponent(bootstrap))),samples=[];
const timer=setTimeout(()=>{console.error('PI2 benchmark timed out');worker.terminate();process.exitCode=1;},120000);
try{
  await new Promise((resolve,reject)=>{
    worker.on('error',reject);
    worker.on('message',data=>{
      if(data.error){reject(Error(data.error));return;}
      if(data.ready)worker.postMessage({id:1,technology:text,creator:{id:0,keywords:'NAME=P:LEN=100:WID=90:W=3:S=2:N=2:METAL=M4:EXIT=M3:XORG=0:YORG=0:ORIENT=0:PHASE=1'}});
      else if(data.layout)worker.postMessage({id:2,action:'quality-sweep',model:'PI2',start_ghz:1,stop_ghz:40,step_ghz:2,nativeInput:true,max_width_cells:500,mesh:{length:50,width:1.5,thickness:.5},contact_mesh:{length:50,width:20},fft_x:256,fft_y:256,contact_limit:128,filament_limit:256,eddy:{mode:0,layers:[0],evaluation_limit:250000}});
      else if(data.port_sweep){
        assert.equal(data.port_sweep.command,'PI2');assert.equal(data.network.contacts,17);assert.equal(data.network.filaments,34);
        samples.push({frequency_hz:data.network.frequency_hz,contacts:data.network.contacts,filaments:data.network.filaments,q:[data.network.pi.quality_first,data.network.pi.quality_second,data.network.pi.quality_differential]});
        if(data.port_sweep.done)resolve();
      }
    });
  });
  assert.equal(samples.length,20);console.log(JSON.stringify({wallSeconds:(performance.now()-start)/1000,samples}));
}finally{clearTimeout(timer);await worker.terminate();}
