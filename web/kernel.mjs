import instantiate from './asitic.js';

async function compile(url){
  let bytes;
  if(url.protocol==='file:'){
    const {readFile}=await import('node:fs/promises');bytes=await readFile(url);
  }else{
    const response=await fetch(url,{credentials:'same-origin'});
    if(!response.ok)throw Error('Unable to load the workbench. Reload to get the current version.');
    bytes=await response.arrayBuffer();
  }
  return WebAssembly.compile(bytes);
}

// Cache immutable code, never a kernel instance or its mutable WASM memory.
// Later optimizer instances remain usable after this release leaves the server.
export function createKernelFactory(url=new URL('./asitic.wasm',import.meta.url)){
  let compiled=null;
  return async function createKernel(){
    if(!compiled){
      const pending=compile(url);compiled=pending;
      pending.catch(()=>{if(compiled===pending)compiled=null;});
    }
    const module=await compiled;
    return instantiate({instantiateWasm(imports,receive){
      const instance=new WebAssembly.Instance(module,imports);receive(instance);
      return instance.exports;
    }});
  };
}

export default createKernelFactory();
