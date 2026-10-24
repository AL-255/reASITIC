import assert from 'node:assert/strict';
// Original-capture checks remain exact in the caller. This comparison records
// bounded host-x87 differences only in inductance-derived transformer outputs;
// geometry, CAP stages, Green data, metadata and signed zero remain strict.
const derived=/^[^.]+\.(matrix|impedance_matrix|input_impedance|inductance_admittance_matrix|transformer|two_port\.scattering_matrix)(\.|$)/;
const numericalRecord=/^[^.]+\.two_port\.(records|polar_records)\.(s|y)$/;
export function compareTransformerHost(a,b,path,report){
  if(typeof a==='number'){
    report.fields++;
    if(Object.is(a,b))return;
    assert.ok(derived.test(path),path+' changed outside the inductive host audit');
    assert.ok(Number.isFinite(a)&&Number.isFinite(b)&&a!==0&&b!==0,path+' changed finite classification or signed zero');
    const relative=Math.abs(a-b)/Math.max(Math.abs(a),Math.abs(b));
    assert.ok(relative<=1e-10,path+' exceeds ten-digit host agreement: '+a+' vs '+b);
    report.rounding.push({path,software:a,host:b,relative});return;
  }
  if(a&&typeof a==='object'){
    assert.deepEqual(Object.keys(a),Object.keys(b),path+' structure');
    for(const key of Object.keys(a))compareTransformerHost(a[key],b[key],path+'.'+key,report);return;
  }
  if(typeof a==='string'&&a!==b&&numericalRecord.test(path)){
    const actual=a.trim().split(/\s+/),native=b.trim().split(/\s+/);assert.equal(actual.length,9,path+' numerical record size');assert.equal(native.length,9,path+' host numerical record size');let changed=false;
    actual.forEach((token,i)=>{
      const x=Number(token),y=Number(native[i]);assert.ok(Number.isFinite(x)&&Number.isFinite(y),path+' nonfinite record');
      if(!Object.is(x,y)){changed=true;compareTransformerHost(x,y,'record.matrix.'+i,report);}
    });
    assert.ok(changed,path+' formatting changed without numerical differences');
    report.rounding.push({path,software:a,host:b});return;
  }
  assert.equal(a,b,path);
}
