import assert from 'node:assert/strict';
import {compareMetric, relativeErrorLimit} from './analysis-accuracy.mjs';
assert.equal(relativeErrorLimit, .001);
for (const [actual, expected, pass] of [
  [1000.9, 1000, true], [1001, 1000, false], [1001.1, 1000, false],
  [999, 1000, false], [-1000.9, -1000, true], [-1001, -1000, false],
  [0, 0, true], [-0, 0, true], [Number.MIN_VALUE, 0, false], [0, Number.MIN_VALUE, false],
  [null, null, true], [0, null, false], [null, 1, false], [undefined, 1, false],
  [NaN, 1, false], [Infinity, 1, false], [-Infinity, 1, false], ['1', 1, false],
  [2e-200, 2e-200, true], [2.01e-200, 2e-200, false],
]) assert.equal(compareMetric(actual, expected).pass, pass, `${actual} versus ${expected}`);
assert.throws(() => compareMetric(1, NaN), /Invalid reference/);
console.log('Analysis accuracy policy: strict 0.1% boundary, both signs, tiny metrics, exact zero, missing/nonfinite and undefined classification pass');
