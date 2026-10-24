import assert from 'node:assert/strict';
import {readFileSync, mkdirSync, writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import make from '../dist/asitic.js';
import {parseTechnology, configureKernel, createGeometry} from '../web/technology.mjs';
import {parseCreatorCommand} from '../web/creators.mjs';
import {compareMetric, relativeErrorLimit} from './analysis-accuracy.mjs';

const args = process.argv.slice(2);
let shard = 0, shards = 1, only;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--shard') shard = Number(args[++i]);
  else if (args[i] === '--shards') shards = Number(args[++i]);
  else if (args[i] === '--case') only = args[++i];
  else throw Error('Unknown argument: ' + args[i]);
}
assert.ok(Number.isInteger(shards) && shards > 0 && shards <= 200);
assert.ok(Number.isInteger(shard) && shard >= 0 && shard < shards);
assert.ok(!process.env.CI || !only, 'CI must run the complete shard, without --case');
const sha = x => createHash('sha256').update(x).digest('hex');
const prefix = 'tests/fixtures/original-analysis-200';
const blob = readFileSync(prefix + '.json.gz'), raw = gunzipSync(blob);
const manifest = JSON.parse(readFileSync(prefix + '.manifest.json', 'utf8'));
const original = JSON.parse(raw), definitions = readFileSync('tests/fixtures/analysis-200-cases.json');
const cases = JSON.parse(definitions).cases;
assert.equal(original.schema, 1); assert.equal(manifest.schema, 1);
assert.equal(manifest.blob, 'original-analysis-200.json.gz');
assert.equal(sha(blob), manifest.blob_sha256); assert.equal(sha(raw), manifest.uncompressed_sha256);
assert.equal(sha(definitions), manifest.definitions_sha256); assert.equal(sha(definitions), original.definitions_sha256);
assert.equal(sha(readFileSync('tools/capture_analysis_200.py')), original.capture_script_sha256);
assert.equal(original.executable_sha256, '72bcb93444604b89a65cb64e5825bfcff0ac740a816120d0f171855206bd572f');
assert.equal(original.repetitions, 2); assert.equal(manifest.relative_error_limit, relativeErrorLimit);
assert.equal(manifest.case_count, 200); assert.equal(original.case_count, 200);
assert.equal(cases.length, 200); assert.equal(original.cases.length, 200);
assert.equal(new Set(cases.map(c => c.id)).size, 200);
assert.equal(new Set(original.cases.map(c => c.id)).size, 200);
assert.deepEqual(original.cases.map(c => c.id), cases.map(c => c.id));
const metrics = [
  'conductor.inductance_nh', 'conductor.resistance_dc', 'conductor.resistance_hf',
  'conductor.quality_low_frequency', 'conductor.metal_area_um2',
  'lmat.resistance_ohm', 'lmat.reactance_ohm', 'lmat.inductance_nh', 'lmat.q',
  'pi.inductance_nh', 'pi.resistance_ohm', 'pi.c1_ff', 'pi.c2_ff',
  'pi.r1_ohm', 'pi.r2_ohm', 'pi.q1', 'pi.q2', 'pi.qd', 'pi.resonance_ghz',
].sort();
const technologies = Object.fromEntries(Object.entries(original.technology_sha256).map(([name, hash]) => {
  const text = readFileSync(`web/technology/${name}.tek`); assert.equal(sha(text), hash);
  return [name, parseTechnology(text.toString())];
}));
for (const ref of original.cases) {
  assert.deepEqual(Object.keys(ref.values).sort(), metrics, ref.id);
  assert.equal(Object.keys(ref.words).length, 15);
  for (const [key, hex] of Object.entries(ref.words)) {
    assert.match(hex, /^[0-9a-f]{16}$/);
    const value = Buffer.from(hex, 'hex').readDoubleLE();
    assert.equal(ref.values[key], Number.isFinite(value) ? value : null, ref.id + ' original binary64 ' + key);
  }
  assert.ok(ref.frequency_hz > 0 && Number.isFinite(ref.frequency_hz));
  for (const v of Object.values(ref.values)) assert.ok(v === null || typeof v === 'number' && Number.isFinite(v));
}

const selected = cases.filter((c, i) => i % shards === shard && (!only || c.id === only));
assert.ok(selected.length > 0, 'No matching cases');
if (!only) assert.equal(selected.length, Math.floor((199 - shard) / shards) + 1);
// The production Green-table memo uses the complete process/frequency/grid key.
// Order equal keys together to avoid rebuilding identical tables for each coil.
selected.sort((a, b) => a.technology.localeCompare(b.technology) || a.fft[0] - b.fft[0] || a.frequency_ghz - b.frequency_ghz);
const k = await make(); k._set_potential_memo(1);
const decode = ptr => { const r = JSON.parse(k.UTF8ToString(ptr)); assert.ok(!r.error, r.error); return r; };
const reports = []; let checked = 0, maximumRelativeError = 0;
for (const c of selected) {
  const ref = original.cases.find(r => r.id === c.id), started = performance.now();
  try {
    assert.equal(c.eddy, false);
    const technology = technologies[c.technology], domain = {...technology.domain, cells_x: c.fft[0], cells_y: c.fft[1]};
    configureKernel(k, technology);
    const creator = parseCreatorCommand(c.command); assert.equal(creator.id, c.creator_id);
    createGeometry(k, creator.id, creator.keywords, creator.answers, {domain});
    k._set_max_width_cells(c.maxnw);
    const hz = k._normalize_value(c.frequency_ghz, 5, .1, 0); assert.equal(hz, ref.frequency_hz);
    const a = decode(k._analyze(hz)), winding = a.windings.findIndex(w => w.name === c.winding);
    assert.ok(winding >= 0, 'Missing winding ' + c.winding);
    const w = a.windings[winding], mesh = c.mesh.map(x => k._normalize_value(x, 7, .1, 1));
    const contacts = c.contact_mesh.map(x => k._normalize_value(x, 7, .1, 1));
    const s = decode(k._simulate(winding, hz, ...mesh, contacts[0], 1024));
    const n = decode(k._simulate_network(winding, -1, hz, ...mesh, ...contacts, domain.x, domain.y, ...c.fft, 1024, 1024, 0, 0, 0, 0, 250000)), p = n.pi;
    const actual = {
      'conductor.inductance_nh': w.inductance_nh, 'conductor.resistance_dc': w.resistance_dc,
      'conductor.resistance_hf': w.resistance_hf, 'conductor.quality_low_frequency': w.quality_low_frequency,
      'conductor.metal_area_um2': w.metal_area_um2,
      'lmat.resistance_ohm': s.resistance, 'lmat.reactance_ohm': s.reactance,
      'lmat.inductance_nh': s.inductance_h * 1e9, 'lmat.q': s.series_q,
      'pi.inductance_nh': p.inductance_h / 1e-9, 'pi.resistance_ohm': p.resistance,
      'pi.c1_ff': p.capacitance_first_f === null ? null : p.capacitance_first_f / 1e-15,
      'pi.c2_ff': p.capacitance_second_f === null ? null : p.capacitance_second_f / 1e-15,
      'pi.r1_ohm': p.resistance_first, 'pi.r2_ohm': p.resistance_second,
      'pi.q1': p.quality_first, 'pi.q2': p.quality_second, 'pi.qd': p.quality_differential,
      'pi.resonance_ghz': n.resonance_hz === null ? null : n.resonance_hz / 1e9,
    };
    const results = metrics.map(key => {
      const result = compareMetric(actual[key], ref.values[key]); checked++;
      if (result.relativeError !== null) maximumRelativeError = Math.max(maximumRelativeError, result.relativeError);
      return {metric: key, actual: actual[key] ?? null, reference: ref.values[key], ...result};
    });
    const pass = results.every(r => r.pass);
    reports.push({id: c.id, pass, milliseconds: performance.now() - started, results});
    console.log(`${c.id}: ${pass ? 'PASS' : 'FAIL'} (${((performance.now() - started) / 1000).toFixed(1)}s)`);
    for (const r of results.filter(r => !r.pass)) console.error(JSON.stringify(r));
  } catch (error) {
    reports.push({id: c.id, pass: false, error: error.stack, milliseconds: performance.now() - started});
    console.error(c.id + ': ERROR ' + error.message);
  }
  // Preserve useful evidence even if a later solve or CI timeout interrupts us.
  mkdirSync('build/evidence', {recursive: true});
  writeFileSync(`build/evidence/analysis-200-${only ? 'single' : 'shard-' + shard}.json`, JSON.stringify({
    schema: 1, totalCases: 200, shard, shards, selectedCases: selected.length,
    blobSHA256: sha(blob), kernelBuild: JSON.parse(readFileSync('dist/build.json')),
    arithmetic: 'production portable math', relativeErrorLimit, checkedMetrics: checked,
    maximumRelativeError, maximumErrorPercent: 100 * maximumRelativeError,
    passed: reports.filter(r => r.pass).length, failed: reports.filter(r => !r.pass).length, reports,
  }, null, 2) + '\n');
}
console.log(`Original analysis regression: ${reports.length} cases, ${checked} metrics, max error ${(100 * maximumRelativeError).toPrecision(6)}%, ${reports.filter(r => !r.pass).length} failed`);
assert.equal(reports.filter(r => !r.pass).length, 0, 'Every metric must have error strictly below 0.1%; inspect build/evidence/analysis-200-*.json');
