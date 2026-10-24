// One policy for every metric and every case. No absolute-error allowance.
export const relativeErrorLimit = 0.001;
export function compareMetric(actual, reference) {
  if (reference === null) return {pass: actual === null, relativeError: null, reason: 'undefined classification'};
  if (typeof reference !== 'number' || !Number.isFinite(reference)) throw Error('Invalid reference metric');
  if (typeof actual !== 'number' || !Number.isFinite(actual)) return {pass: false, relativeError: null, reason: 'missing or nonfinite result'};
  if (reference === 0) return {pass: actual === 0, relativeError: null, reason: 'reference zero requires exact zero'};
  const error = Math.abs(actual - reference) / Math.abs(reference);
  return {pass: error < relativeErrorLimit, relativeError: Number.isFinite(error) ? error : null, reason: 'strict relative error < 0.001'};
}
