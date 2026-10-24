import assert from 'node:assert/strict';
import {capacitanceMatrixCSV,capacitanceDiagnostics} from '../web/capacitance.mjs';
const result={frequency_hz:1e9,matrix:[[[1e-6,-0],[-1e-5,1e-3]],[[2e-5,-1e-3],[0,1e-4]]],potential_matrix:[[[10,-20]]]};
assert.equal(capacitanceMatrixCSV(result),'frequency_hz,row,column,real_siemens,imag_siemens\n1000000000,0,0,0.000001,-0\n1000000000,0,1,-0.00001,0.001\n1000000000,1,0,0.00002,-0.001\n1000000000,1,1,0,0.0001\n');
assert.equal(capacitanceMatrixCSV(result,true),'frequency_hz,row,column,real_ohm,imag_ohm\n1000000000,0,0,10,-20\n');
assert.throws(()=>capacitanceMatrixCSV(null),/Run CAP/);
assert.equal(capacitanceDiagnostics('Calculating Green Function...\nComputing FFTs...            \n.\nWarning: collapse\nWarning: collapse\nTotal Capacitance = 3 (fF)\n'),'Warning: collapse\nTotal Capacitance = 3 (fF)');
console.log('CAP scientific exports: admittance/potential SI units, signed zero, negative values and substantive diagnostics preserved');
