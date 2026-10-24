// These tests audit the recovered executable's original arithmetic. Production
// and worker tests use dist/asitic.js directly with stable geometry mathematics.
import createKernel from '../dist/asitic.js';
export default async function createReferenceKernel(options){const kernel=await createKernel(options);kernel._set_reference_numerics(1);return kernel;}
