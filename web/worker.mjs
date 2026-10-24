import createKernel from './asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from './technology.mjs';
const kernel=await createKernel();
self.onmessage=({data})=>{
  try{
    if(data.action==='analyze'){
      const analysis=JSON.parse(kernel.UTF8ToString(kernel._analyze(data.frequency)));
      if(analysis.error)throw Error(analysis.error);
      postMessage({id:data.id,analysis,warnings:kernel.UTF8ToString(kernel._last_warnings())});return;
    }
    const technology=parseTechnology(data.technology);configureKernel(kernel,technology);const layout=generateGeometry(kernel,data.kind,data.parameters);postMessage({id:data.id,layout,technology});}
  catch(error){postMessage({id:data.id,error:error.message});}
};
postMessage({ready:true});
