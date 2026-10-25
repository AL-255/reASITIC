import assert from 'node:assert/strict';
import {compareMetric} from './analysis-accuracy.mjs';

export function originalMetric(actual,expected,label){
  const result=compareMetric(actual,expected);
  assert.ok(result.pass,`${label}: ${actual} vs ${expected}; ${result.reason}; relative error ${result.relativeError}`);
}

// Production Green arithmetic is binary64. Exact original table fingerprints
// belong to reference-numerics audits; compare physical values here.
export function relativeClose(actual,expected,label,tolerance=1e-12){
  assert.ok(Number.isFinite(actual)&&Number.isFinite(expected),label+' finite values');
  if(expected===0)assert.equal(actual,0,label);
  else assert.ok(Math.abs(actual-expected)/Math.abs(expected)<tolerance,`${label}: ${actual} vs ${expected}`);
}
export function matrixClose(actual,expected,label,tolerance=1e-9){
  assert.equal(actual.length,expected.length,label+' rows');
  actual.forEach((row,i)=>assert.equal(row.length,expected[i].length,label+' columns'));
  const a=actual.flat(Infinity),e=expected.flat(Infinity),norm=e.reduce((m,v)=>Math.max(m,Math.abs(v)),0);
  assert.equal(a.length,e.length,label+' fields');
  for(let i=0;i<a.length;i++){
    assert.ok(Number.isFinite(a[i])&&Number.isFinite(e[i]),label+' finite field '+i);
    if(norm===0)assert.equal(a[i],0,label+' zero matrix');
    else assert.ok(Math.abs(a[i]-e[i])/norm<tolerance,`${label}[${i}]: ${a[i]} vs ${e[i]}`);
  }
}
