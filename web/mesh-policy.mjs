// All physics stays in C++. In particular, automatic limits are already
// computed doubles and must not pass through the command dimension parser.
export function solverCells(kernel,data,hz,kind){
  if(!['inductance','network','capacitance'].includes(kind))throw Error('Unknown solver mesh.');
  const maximum=data.max_width_cells??500;
  if(!Number.isInteger(maximum)||maximum<1||maximum>2147483647||!kernel._set_max_width_cells(maximum))throw Error('Use a positive integer MAXNW width limit up to 2147483647.');
  const automatic=data.automatic_cells;
  if(automatic!==undefined&&automatic!==null&&automatic!==false){
    if(typeof automatic!=='object'||Array.isArray(automatic))throw Error('Use alpha and beta for automatic cells.');
    let alpha=Number(automatic.alpha??.03),beta=Number(automatic.beta??.6);
    if(data.nativeInput){alpha=kernel._normalize_value(alpha,4,.1,0);beta=kernel._normalize_value(beta,4,.1,0);}
    const policy=JSON.parse(kernel.UTF8ToString(kernel._automatic_cell_limits(hz,alpha,beta)));
    if(policy.error)throw Error(policy.error);
    const m=policy.mesh,c=policy.contact_mesh;
    return {cells:kind==='inductance'?[m.length,m.width,m.thickness,m.subdivision_trigger]:kind==='network'?[m.length,m.width,m.thickness,c.length,c.width]:[c.length,c.width],policy};
  }
  const m=data.mesh??{},c=data.contact_mesh??{};
  const cells=kind==='inductance'?[m.length??50,m.width??1.5,m.thickness??.5,m.subdivision_trigger??m.length??50]:kind==='network'?[m.length??50,m.width??1.5,m.thickness??.5,c.length??50,c.width??20]:[m.length??50,m.width??20];
  return {cells:data.nativeInput?cells.map(v=>kernel._normalize_value(Number(v),7,data.grid??.1,data.snap===false?0:1)):cells};
}

export function meshResult(result,selection){
  if(result.error)throw Error(result.error+(selection.policy&&/contact collapses/.test(result.error)?' With AUTOCELL, increasing beta can also resolve this.':''));
  if(selection.policy)result.automatic_cells=selection.policy;
  return result;
}
