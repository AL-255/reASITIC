import createKernel from './asitic.js';
import {parseTechnology,configureKernel,generateGeometry} from './technology.mjs';
const kernel=await createKernel();
self.onmessage=({data})=>{
  try{const technology=parseTechnology(data.technology);configureKernel(kernel,technology);const layout=generateGeometry(kernel,data.kind,data.parameters);postMessage({id:data.id,layout,technology});}
  catch(error){postMessage({id:data.id,error:error.message});}
};
postMessage({ready:true});
