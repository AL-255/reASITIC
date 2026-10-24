import assert from 'node:assert/strict';
import {networkMatrixCSV,networkMetricsCSV} from '../web/network.mjs';
const result={frequency_hz:1e9,matrix:[[[1,-0],[-2,-3]],[[-2,-3],[4,5]]],pi:{frequency_hz:1e9,inductance_h:-2e-9,resistance:-0,capacitance_first_f:3e-15,quality_differential:null},resonance_hz:null,eddy:{mode:1,layer_count:2,indices:[0,1],evaluations:90,quadrature_status:[0,0,0,0,0,1,0,0]}};
assert.equal(networkMatrixCSV(result),'frequency_hz,row,column,real_siemens,imag_siemens\n1000000000,0,0,4,5\n1000000000,0,1,-2,-3\n1000000000,1,0,-2,-3\n1000000000,1,1,1,-0\n');
const csv=networkMetricsCSV(result);assert.match(csv,/frequency_hz,inductance_h,resistance_ohm,capacitance_first_f,quality_differential,resonance_hz,eddy_mode/);assert.match(csv,/1000000000,-2e-9,-0,3e-15,,,1,2,0;1,90,1\n$/);assert.throws(()=>networkMatrixCSV(null),/combined/);assert.throws(()=>networkMetricsCSV(null),/combined/);
console.log('Network scientific exports: siemens/H/F/Hz units, signed zero, negative metrics, undefined values and eddy diagnostics preserved');
