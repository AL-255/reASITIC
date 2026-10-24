# reASITIC · C++ / WebAssembly

A browser recreation of ASITIC hosted on [GitHub Pages](https://al-255.github.io/reASITIC/). The recovered C++
implementation from `~/github/asitic-re` is preserved unchanged under
`vendor/asitic`; `provenance.json` pins the source commit and every file hash.

Work in progress. The target is original ASITIC geometry and simulation algorithms,
including its technology model, filament solver, substrate effects, and metrics.
The original models and reported results are checked against the executable.
Production accuracy is checked independently of processor instruction rounding;
the recovered instruction sequence remains available for historical audits.

No original executable is needed by the deployed application. The C++ sources are
the math kernel. JavaScript handles the interface and GDSII serialization.

The browser workbench includes square/polygon/symmetric/multi-metal
windings, planar and 3D mirror transformers, baluns, stacked capacitor plates,
rings, wires and configurable via arrays. The original keyword parsers and
interactive creator dispatch run in C++. The advanced command editor supports
all twelve creator commands and native dimensions such as ILEN / IWID. Add devices
to a shared layout to construct independent ground conductors and coil assemblies;
new devices require unique names and the same technology. Rejected additions,
including late export failures, preserve the previous layout and solver state. Parameters use micrometres. The original `RESIS` DC resistance, `INDUCTANCE` conductor estimate and `RESISHF`
high-frequency resistance paths run inside the C++ kernel. These are the original
conductor estimates; the original LMAT filament solver is available in the workbench,
with disabled eddy currents, native shape selection, or segment folding.
The original layered CAP solver is also available, with substrate conductivity
and permittivity. The original eddy Green kernels, QUADPACK integration and
matrix stamping are integrated. PI2 combines the original capacitance, inductance
and node-equation solvers into a two-port admittance matrix and pi model.
CALCTRANS adds original coupled-winding narrowband simulation. PIX adds the
original extended two-port node system. PI4 pad deembedding retains original
reported model precision with the stable formulation; broader validation continues.
PI2, PIX, PI4 and CALCTRANS export the recovered C++ two-port S conversion,
native rectangular/polar S/Y/model records, and raw scattering CSVs.
PI2, PIX, PI4 and CALCTRANS also support stepped port frequency sweeps, streamed
results, cancellation, combined native records and sweep metrics/S CSV exports.
Original 3PORT provides three-winding models and S/Y/Z conversion. GDSII uses native ribbon boundaries, real via cuts and
landing pads, with 1 µm user units and a 1 nm database grid.

Production numerics use deterministic software arithmetic. The rectangular
cross-section integral is normalized by geometry scale, keeps guard precision
through its cancelling terms, and rounds to binary64 once. It starts at binary128
and increases precision when the conditioning or rounding interval requires it.
QUADPACK's error-power term uses the algebraically equivalent `x * sqrt(x)` with
software guard precision instead of a logarithm/exponential sequence. The
portable fallback is pinned LibBF C with explicit rounding and its SIMD path
disabled; the vendored MIT license is included in deployment.

The primary numerical checks compare the same C++ implementation compiled for a
native host and WASM against independent mpmath references: 203 geometric-mean
distances and 1,040 error-power values. Geometry tests include analytic square and
thin-strip limits, power-of-two scale changes, subnormal dimensions and extreme
aspect ratios. Production solver/worker checks exercise the stable default.
All fourteen PI4 reference cases also retain the original published model
precision, capacitance and Green tables, and nonfinite rejection behavior.
The original instruction/store sequence remains available through the explicitly
named `tests/reference-kernel.mjs` audit helper; those historical bit comparisons
do not choose the production numerical policy or depend on a runtime CPU profile.
Transformer host audits retain bounded rounding observations in inductance-derived
fields while requiring ten-digit relative agreement. CAP, metadata, structure,
signed zero, headers and all captured original Y/node/model checks remain strict.
`node tests/transformer.mjs --strict-native` separately requests host instruction-bit
identity; hosted native x87 results can differ from the original capture machine.
`tools/reference_gmd.py` regenerates the distance fixtures with mpmath 1.3.0 and
checks the antiderivative against direct quadrature of mean log-distance.
`tools/reference_error_power.py` independently uses `sqrt(x**3)` for exact
binary64 inputs at 200 decimal digits.

```sh
# Emscripten 6.0.6, Python 3, Node 24
python3 -m pip install gdstk==1.0.2 mpmath==1.3.0
python3 tools/build.py                    # or --emxx /path/to/em++
python3 tools/build_native.py             # GCC i386 + libc.so.6 reference audit
python3 tools/build_portable_math.py       # native/WASM software-math verification
node tests/portable-gmd.mjs
node tests/portable-error-power.mjs
node tests/portable-simulation.mjs
node tests/portable-pad.mjs
node tests/two-port-output.mjs
node tests/port-schedule.mjs
node tests/port-sweep.mjs
node tests/worker-port-sweep.mjs
node tests/portable-three-port.mjs
node tests/three-port-sweep.mjs
node tests/worker-three-port-sweep.mjs
node tests/native-creators.mjs
node tests/original-creator-binary.mjs
node tests/parity.mjs
node tests/original-geometry.mjs
node tests/estimates.mjs
node tests/connected-analysis.mjs
node tests/capacitance.mjs
node tests/capacitance-data.mjs
node tests/capacitance-grids.mjs
node tests/worker-capacitance.mjs
node tests/eddy.mjs
node tests/eddy-runtime.mjs
node tests/eddy-lifecycle.mjs
node tests/worker-eddy.mjs
node tests/pad.mjs
node tests/pad-lifecycle.mjs
node tests/worker-pad.mjs
node tests/pad-rounding.mjs
node tests/extended.mjs
node tests/extended-lifecycle.mjs
node tests/worker-extended.mjs
node tests/network.mjs
node tests/network-lifecycle.mjs
node tests/worker-network.mjs
node tests/network-data.mjs
node tests/transformer.mjs
node tests/transformer-lifecycle.mjs
node tests/worker-transformer.mjs
node tests/transformer-data.mjs
node tests/three-port.mjs
node tests/three-port-backing.mjs
node tests/three-port-lifecycle.mjs
node tests/worker-three-port.mjs
node tests/three-port-data.mjs
node tests/eddy-math-diagnostics.mjs        # records 3 unresolved finite differences
# Historical instruction-identity check; known differences remain:
# node tests/eddy-math-diagnostics.mjs --strict
node tests/lmat.mjs
node tests/original-lmat.mjs
node tests/lmat-lifecycle.mjs
node tests/worker-simulation.mjs
node tests/command-input.mjs
node tests/simulation-data.mjs
node tests/retained-math.mjs
node tests/worker.mjs
python3 tests/gds_reader.py
python3 -m http.server 8766 --directory dist
```

The native build deliberately keeps x87 instructions. It needs i386 glibc at
`/lib/i386-linux-gnu/libc.so.6`, `/lib/i386-linux-gnu/libm.so.6` and `/lib/ld-linux.so.2`. Production uses only
WebAssembly, without Python, the native executable, or a remote simulation API.

Current measured evidence:

- All twelve original creator families match 3,320 raw physical binary64
  fields from repeated fresh original-executable sessions, pass 1,995 original SAVE printed-field
  checks, and 4,125 numeric fields match the i386 build exactly. Capacitor join,
  balun reflection/phase and mirror-transformer prompt composition are retained.
- All 6,972 numeric fields in 20 i386/WASM construction cases match exactly.
- All 120 conductor-estimate fields across 50 frequency cases match exactly.
- All 33,836 LMAT fields across 33 native/WASM cases match exactly, including
  coarse, cross-section and unequal longitudinal meshes, phase and rotation.
- Two fresh unmodified original-executable runs per winding case produced
  identical physical outputs. WASM matches all 2,277 raw BSAVE geometry values
  and 33,704 matrix components at the original LMAT 20-decimal output boundary.
- Seven additional original SAVE fixtures pass 955 printed-precision checks;
  eleven connected command reports pass 121 L / DC / R(f) checks.
- Eleven complete CAP cases match 1,000 native numeric fields exactly, plus 846
  original printed matrix/report fields. All 17,219,040 bytes of the original
  raw Green-table payloads match their FNV-1a-64 fingerprints. Every reference
  case repeats the original executable with a fresh cache, including one-cell whole-chip and 256×256
  FFT spiral, refined/distinct grounds, capacitor plates, and rotated wires.
- Thirteen eddy cases reproduce the original executable: twelve finite LMAT
  solves match 2,268 printed matrix components, and one original nonfinite case
  is consistently rejected. All 2,554 native numeric fields and 39,089 Green
  evaluation samples/values match exactly through trace fingerprints, including
  the original nonconvergence diagnostics and finite partial results.
- Twelve whole-program PI2 cases match 1,322 native numeric fields and 912
  original printed fields: capacitance/inductance/node-stamping matrices, pi
  components, port Q, input impedance, shunt resistance and pi resonance estimates.
  Original raw Green-table fingerprints also match. Cases cover native default,
  explicit same-device and distinct grounds, refined longitudinal/cross-section
  meshes, rotation, square coils, both eddy kernels and low/high frequencies.
- Sixteen coupled-winding cases match 4,019 native numeric fields, 1,420 original
  printed fields and 2,968,320 raw Green-table bytes through payload hashes.
  Repeated fresh CALCTRANS and 2PORTTRANS runs cover both eddy kernels, native
  terminations (including binary32 parsing at the near-short threshold),
  ground acquisition, phase, rotation, mesh refinement, two square spirals with
  vias, polygons and a planar transformer. Two-port Y records retain the native
  mixed final-column precision; native matrix/model arithmetic compares exactly.
- Twelve three-port cases match 19,725 native numeric fields, 15,012 original
  printed system/model fields, 31,503 bytes of original rectangular/polar S/Y/Z
  records and 1,638,720 raw Green-table bytes through payload hashes. Native
  CELL parsing and signed zeros are retained. Cases include mesh refinement,
  phase, rotation, both eddy kernels, explicit first-winding reference, square
  coils with vias and polygons. Four additional captured separate-reference
  states match all 132 raw original Z/model fields exactly, including live
  backing aliases. Archived ordinary original executions demonstrate six
  different outputs from eight identical scripts for each SAVEMAT setting.
- All 2,118 i386 libm sin/cos probes match both binary64 and raw binary80 return
  words. Pinned glibc 2.36 scalar numerical sources supply these callbacks; the
  recovered Green runtime calls libm, rather than the x87 FSIN instruction.
- All 681 hardware-x87 instruction/arithmetic cases match their consumed
  binary64 outputs. Nine raw binary80 differences are recorded separately;
  this remains a limit on universal microcode identity.
- A separate 533-case QUADPACK error-power diagnostic finds three finite
  one-ULP binary64 discrepancies at extreme inputs. Hardware FYL2X differs from
  mathematically nearest binary80 in these cases. No empirical rounding rule
  or per-input correction is substituted. The differences are retained in
  `build/evidence/eddy-math-diagnostics.json`; `--strict` records failure of historical instruction-bit identity. Production
  error-power accuracy is checked independently against the high-precision
  algebraic reference, with no per-input corrections.
- All 96 recovered scalar/frequency/grid/angle parser cases match i386 exactly,
  including the original negative-angle normalization behavior.
- Both transformer windings match the unchanged i386 solver in reference mode;
  production agrees to ten significant digits. Rejected meshes
  recover, simulation does not mutate source geometry, and 100 repeated solves
  retain a bounded heap. An independent gdstk reader validates 33 GDS exports.

These are bounded tests, not universal parity claims. The historical numerical
comparisons run in explicit reference mode. Production is independently gated by
the software-math references and default-policy solver/worker checks. The explicitly unresolved error-power
measurement is uploaded separately and does not claim a passing identity check. The source translator rejects unknown
assembly. Native Clang AST ranges expose implicit long-double-to-double stores
in creator, layered-potential, Green, quadrature, eddy and network units; the build retains an expression/offset report. This preserves
original narrowing boundaries without changing vendor sources. Arithmetic and square root use Berkeley SoftFloat binary80;
logarithms, trig and exponent fractions use binary128 guard evaluation with
native x87 constants, followed by the original binary80 stores. This avoids
Emscripten's binary64 `logl` fallback. The SoftFloat license ships with the site.
Raw binary80 microcode equality beyond the consumed/tested stores remains
unproven. Native comparison builds use the reference's
`-fexcess-precision=standard` rule.

The workbench exposes one-winding LMAT solves, mesh controls, conductor series
Q, linear/log frequency sweeps, selectable L/R/Q plots, sample-to-matrix inspection,
and matrix/results CSV downloads. Sweeps stream summaries and retain a complete
matrix only for the latest inspected sample. Cancellation stops between samples.
Phase, origin, snap and grid controls use the recovered native input parser;
frequency and angle conversion retain native binary32 boundaries and quirks.
Substantive native solver warnings remain visible.

The layered CAP panel exposes original contact meshing, spectral Green tables,
FFT transforms, potential-matrix inversion, group reduction, terminal folding,
and the unfolded internal solve. The `.tek` layer EPS/RHO values and original
rho × 10000 conversion are retained alongside cumulative layer and metal Z
coordinates. CAP's explicit ground argument resets its mesh words exactly as
native parser type 8 does; a private clone preserves the source layout. Native
CCELL minimum-pitch normalization remains visible in diagnostics. Both the
folded admittance (siemens) and contact potential (ohms) matrices export to CSV.
CAP totals use the native rounded values immediately before the original report,
observed through an added callback without replacing numerical expressions.

`simulate_capacitance(winding, ground, frequency_hz, length_cell, width_cell,
chip_x, chip_y, fft_x, fft_y, contact_limit, unfolded)` uses ground -1 for the
default same-name reference. Power-of-two FFT sizes 1–256, a 1–512 contact limit,
and a 128 MiB table budget bound computation. Collapsed/out-of-bounds contacts
and native fault cases are rejected before matrix indexing. There is no
persistent browser table cache yet; fine grids can take tens of seconds. The
original nonsquare indexing is preserved with sufficient zero backing storage;
original-executable parity is currently measured on square FFT grids only.
PI2/PIX/PI4 and CALCTRANS now include the native two-port numerical writer.
The original fixed 50-ohm conversion, diagonal formulas, reciprocal Y output,
S entry order, GHz frequency stores and truncated column headers are preserved.
These historical records are labelled as native ASITIC output; raw S CSVs expose
every computed matrix entry. Verification compares 36 finite numerical rows and
their headers byte for byte, plus 48 original binary64 S fields, from unmodified
port commands in two fresh processes each. One original command's nonfinite
admittance is rejected instead of exported as a finite result. Another 128
isolated native/WASM comparisons cover nonsymmetric inputs, singular matrices,
signed zero and extreme scales. `tools/capture_two_port.py` reproduces the
read-only writer observations. Six complete reference-mode solves also reproduce
all six record modes directly; the stable production formulation retains eight
significant digits of the S entries in these cases.

Port sweeps use the recovered C++ `run_port_frequency_sweep` recurrence and its
unrounded extended-precision stop comparison. Each start/stop/step is parsed at
the native GHz input precision, then the saved Hz values pass directly to the
selected C++ solve. A bounded recording callback prepares the frequency list
before running any solver; nonadvancing steps and ranges exceeding 101 samples
are rejected. The worker streams compact completed samples, retains full stage
matrices only for the final sample, and yields between samples for cancellation.
A failed solve stops the sweep with its sample/frequency diagnostic; completed
samples remain exportable. Native numerical exports concatenate the original
header once and every completed record in frequency order. S and metrics CSVs
include the selected model and retain SI units and undefined values.

Five repeated unmodified original sweeps cover PI2, PIX, PI4, CALCTRANS and
decimal-step input rounding: all 15 saved frequencies and 20 complete numerical
file sections match in reference mode. Production S entries agree to eight
significant digits in these cases. Another 259 native/WASM scheduling cases
exercise the unrounded stop boundary and saved-double recurrence; actual worker
tests cover all four models, streaming, exports, invalid ranges, solver failure
and cancellation recovery. Three-port sweeps use the same native schedule and
stream the numerical three-winding model, including separate references. Broader original
command coverage remains outstanding.

The worker protocol is tested with actual WASM startup, original wire matrices,
streamed frequency samples, cancellation and error recovery. Scientific export
checks preserve SI units, signed zeros and negative results. Rendered browser QA
remains outstanding because no browser was available in this session.

`simulate(winding, frequency_hz, length_cell, width_cell, thickness_cell,
subdivision_trigger, filament_limit)` exports the complete impedance matrix,
L/R/X and conductor series Q. Mesh dimensions are micrometres; the subdivision
trigger retains the original CCELL interaction. The complete original solver
is compiled, and the legacy API retains its original no-eddy entry. Counts are
produced by the original routines. A configurable 1–1024 filament budget rejects oversized
requests without silently coarsening the mesh. The original source geometry is
preserved, and via cut counts are retained.

`simulate_eddy(winding, frequency_hz, length_cell, width_cell, thickness_cell,
subdivision_trigger, filament_limit, mode, layer_count, first_layer, second_layer,
evaluation_limit)` adds the original substrate calculation. Modes 0/1/2 mean
disabled/native shape selection/segment folding. The first two repeated `.tek`
EDDY entries select substrate layers; a single entry `EDDY=0` selects layer zero.
Original packed square stamping, reflection/oscillating kernels, layer sorting,
and longitudinal corrections are retained, including native quirks. Up to ten
million Green evaluations are allowed by the C++ API; the UI defaults to 250,000.
Budget exhaustion returns an error and discards partial matrices. Native finite
partial integration results remain visible with their QUADPACK diagnostics;
nonfinite matrices return errors. Per-solve state is released on success or
failure. Matrix/sweep CSVs retain model, layers, evaluation counts and diagnostic
counts. Worker and scalar-probe checks cover recovery, immutable geometry and
bounded repeated allocations.

The combined network panel runs original PI2 with the CAP device/ground/frequency
and FFT/contact settings, plus LMAT filament cells, budgets and eddy settings.
It exposes native two-port admittance, pi L/R/C/Rs, all three Q values, single-ended
and differential input/shunt impedances, and the original pi resonance estimate.
This estimate is the original local pi formula, rather than a resonance search.
Undefined derived metrics export as empty CSV values and display as Undefined.
Two-port Y exports use siemens and the original file port ordering (internal
y22 first, reciprocal y21 off-diagonals, y11 last); pi CSV headers retain
H/F/ohm/Hz units.

`simulate_network(winding, ground, frequency_hz, length_cell, width_cell,
thickness_cell, contact_length, contact_width, chip_x, chip_y, fft_x, fft_y,
contact_limit, filament_limit, eddy_mode, layer_count, first_layer, second_layer,
evaluation_limit)` calls the original `solve_combined_network` entry. The native
private clone, folded capacitance, reduced inductance admittance, node stamping,
general LU solve and pi extraction retain their original sequence and arithmetic.
Preflight clones run the same original count routines before admitting matrix
allocations. Ground type-8 acquisition preserves its native -1/-1 mesh behavior,
including explicit self-ground. Stage matrices are copied at original snapshot
boundaries for comparison; all private state is released after every solve.
Worker and lifecycle checks cover budget recovery, alternating CAP/LMAT/network
requests, immutable source geometry and bounded repeated allocations. The model selector also exposes original PIX via
`simulate_extended_network` with the same arguments as `simulate_network`.
PIX uses unfolded capacitance, impedance-form inductance and the original
extended node equations. Its raw current/voltage CSV gives the first unit-voltage
excitation in A/V; the native current-file writer instead normalizes its vector.
The PIX result includes both raw vectors and the two input matrices. Twelve
fixtures compare 1,251 x87 numeric fields, 448 original printed values and 856 raw
binary64 fields read directly from the unchanged executable. The raw observations
cover admittance, pi fields, reduced capacitance/impedance and both vectors.
`tools/capture_extended.py` repeats native commands and read-only GDB captures
in fresh processes; it does not change numerical memory or registers. Admission,
eddy recovery, alternating PI2/CAP/PIX solves and bounded repeated allocations
are regression gates.

The PI4 selector calls `simulate_pad_network(device, first_pad, second_pad,
ground, ...)`, followed by the same frequency/mesh/technology/budget arguments
as `simulate_network`. It executes the original pad copies, device subdivision,
joins, combined solve, pads-only solve and binary64 pad subtraction. Each pad
uses original type-8 -1/-1 main mesh acquisition. Both mesh stages are admitted
before the original caller, which ignores inner failure returns and can otherwise
dereference a missing matrix. Private source records remain immutable.
The response includes final admittance/pi metrics, the combined capacitance and
inductance/node matrices, pads-only capacitance and pre-subtraction admittance.
Omitted ground retains the device reference; explicit self-ground additionally
resets its mesh. Negative native metrics are retained.

**Historical PI4 arithmetic audit.** Fourteen repeated original cases
include seven exact raw reference-mode comparisons, four finite reference-mode rounding discrepancies
and three native nonfinite outputs (reported as errors). The x87 recovered source on the AMD Ryzen 9 7940HX reference machine
matches all finite raw executable observations. Captures include the CPU identity
and an exact FLDLN2/FYL2X instruction signature. A CI machine with a different
signature is retained as a separate hardware observation; its inductance-derived
raw differences cannot replace the original reference. WASM still compares directly
with the same captured original bytes on every host. Capacitance and all other
independent stages remain strict even across differing instruction signatures. The new discrepancies first appear
in filament inductance; both offset-pad eddy sample/value traces match x87 exactly.
One mutual-inductance pair was traced to FLDLN2/FYL2X input binary80 words
`9061ddd199f3f5ef:3fee`: the software result ends in `c1a8dc2e`, hardware in
`c1a8dc2f`. A direct binary64 log store agrees, but subsequent cancellation exposes
the retained difference. `probe_retained_math` and `tests/pad-rounding.mjs`
reproduce this instruction input without substituting captured outputs.
`tools/capture_pad.py` captures both solve stages, inner/final admittances and
pi fields through read-only GDB observations in two fresh processes per case.
`node tests/pad.mjs --strict` and `node tests/pad-rounding.mjs --strict` measure
historical instruction-bit identity. The default production path instead uses
the stable cross-section formulation verified by `portable-gmd.mjs` and
`portable-simulation.mjs`; it is not tuned to reproduce a processor's final-bit
approximation. Captured three-port backing replay explicitly scopes the retained
reference arithmetic and restores the production policy afterwards.

The coupled-winding panel runs the original CALCTRANS backend and extracts its
narrowband transformer model: primary/secondary L and R, mutual inductance,
coupling and ground resistance. It exports both the native two-port Y and Z
matrices and the model in SI units. Y CSV port order is checked directly
against original 2PORTTRANS output; Z uses the original narrowband model labels. Choose any two generated winding records,
including the primary/secondary records of a native planar or 3D transformer.
The panel shares CAP ground/frequency/contact/FFT controls and LMAT filament and
eddy controls. A nonzero termination requires an explicit ground, as in native
CALCTRANS command syntax. Native scalar input retains its binary32 boundary.

`simulate_transformer(primary, secondary, ground, frequency_hz, length_cell,
width_cell, thickness_cell, contact_length, contact_width, chip_x, chip_y,
fft_x, fft_y, contact_limit, filament_limit, eddy_mode, layer_count, first_layer,
second_layer, termination_ohm, evaluation_limit)` uses the original
`solve_transformer_network`. Its geometry preparation is extracted verbatim into
a shared helper for preflight and the real solve. The original winding copies,
second-list reversal, join/translation, boundary index, termination stamp,
node solve and historical reciprocal port ordering remain unchanged. Two added
snapshot callbacks observe matrices without replacing expressions. AST-based
narrowing also covers volatile binary64 copies while preserving the live
extended values. Private ground clones preserve source mesh history and
geometry; lifecycle checks alternate coupled/CAP/PI2 solves and verify recovery
and bounded repeated allocations. Undefined derived metrics retain null/empty
output rather than invented finite values.

The three-port panel runs original 3PORT composition, node assembly, general
factor/solve, narrowband model extraction and S/Y/Z conversion. Production uses
the actual reduced matrix dimension, so a separate reference can be solved
without native process-state captures.
It exposes all three L/R values, three mutual inductances, cross resistances and
couplings. Raw matrix/model CSVs retain SI units and every cell. Native numerical
record downloads use the original C++ writer, including its entry ordering,
repeated cells, zero third-row middle cell, Hz frequency field and polar-writing
behavior. The writer's raw argument words retain the i386 numerical payloads.

`simulate_three_port(first, second, third, ground, frequency_hz, length_cell,
width_cell, thickness_cell, contact_length, contact_width, chip_x, chip_y, fft_x,
fft_y, contact_limit, filament_limit, eddy_mode, layer_count, first_layer,
second_layer, evaluation_limit)` provides default/first-winding and separate references.
Geometry, contact and filament budgets use native counts and preserve
the original subdivision/mesh arithmetic. The source geometry remains private
and unchanged across successful or rejected calls.

Three-port sweeps retain compact S/Y/Z and model samples, with full node/stage
matrices only on the final point. The workbench offers start/stop/step controls,
per-panel cancellation, combined native rectangular/polar S/Y/Z records, all-cell
raw matrix CSVs and sweep metrics. C++ produces both native column headers and
records, preserving the original Hz field, entry order, repeated/zero cells and
polar behavior. Four repeated unmodified original sweeps provide 12 exact saved
frequencies and 24 complete numerical sections; a separately grounded sweep adds
three independent 80-digit nodal checks. Actual worker tests verify saved-frequency
single-solve agreement, compact/final payloads, all exports, budget/range failure,
cancellation and recovery. A historical process-state capture is valid only at
its recorded frequency: one-point replay is supported, and a multi-point sweep
requires clearing that capture to use the numerical model.


A separately named reference triggers an original defect: the legacy driver
increases its node count beyond the reduced matrix dimensions. The node routine reads
unwritten/out-of-bounds data, including bytes aliased to its live LU workspace.
Ordinary original-executable runs with identical physical input can produce
different finite outputs. A uniquely determined result cannot be inferred from
geometry/technology alone on this historical path.

The production correction binds the node count to the joined signal-conductor
count, which equals both solvers’ reduced matrix dimensions. Reference
conductors already contribute through the folded capacitance matrix; counting
them again creates unknowns without coefficients. The node solver now rejects
any matrix/dimension mismatch. Its assembly, general LU solve, port extraction
and model formulas are unchanged. This removes overreads and dependence on
heap layout. It does not claim to reproduce the undefined native-state outputs.

Nine separate-reference cases cover frequency, subdivision, cross-section
meshing, rotation, eddy currents, square coils and polygons. They agree within
1e-10 relative scale with an independent 80-decimal-digit nodal reduction:
`(Y + Dᵀ Z⁻¹ D) V = J`, eliminating currents before solving for voltage instead
of factoring the original 2n block system. The original captured matrix prefixes
are checked separately. Allocation perturbations and historical replay leave
production results unchanged. `tools/reference_three_port.py` uses mpmath 1.3.0;
set `MPMATH_PYTHON` to an environment with that version if needed.

Historical reference mode still requires captured backing for this defect.

`probe_three_port(...same arguments..., capacitance_backing, inductance_backing,
backing_cells, native_addresses)` takes the measured incoming state. The two
arrays contain node_count² complex cells. The five original 32-bit addresses are
capacitance, inductance, LU, RHS and pivots. A bounded memory overlay preserves
actual overlaps while the unchanged original node arithmetic runs. Computed
matrix prefixes must match the captured prefixes byte for byte. The adapter
validates address ranges, shared bytes, zero-initialized overlaps and allocation
sizes; it releases the overlay after each call. This supplies the bytes and live
aliases read by the original algorithm. No expected result fields enter the
calculation. Unwritten native process state is an explicit input, as with the
other recovered construction/cache histories; input-only universal identity on
this undefined original path is not claimed.

Optionally import `backing.json` for a historical native-state comparison after
creating its winding/reference records. The import restores available mesh,
frequency and eddy settings. The worker checks winding order, reference,
frequency and technology hash, and C++ verifies the actual matrix prefixes.
Capture examples are written under
`build/original-three-port/capture-*/backing/*/backing.json`. The capture uses GDB
at the pinned post-allocation node boundary and model return, without changing
numerical inputs, registers or results. `--cases`, `--technology`,
`--backing-ground` and `--output-dir` select user-owned inputs/destinations.
The deployed application runs C++/WASM locally and does not invoke the original
executable or GDB. Lifecycle checks cover capture validation/recovery, repeated
heaps, source immutability and alternating three-port/PI2 solves.

The portable glibc callbacks preserve the numerical C sources unchanged and adapt
runtime headers only. They do not expose errno or floating exception flags.
LGPL notices and licenses are available under `dist/licenses/glibc-math`; source
hashes and rebuild/relink scripts are in `vendor/glibc-math` and
`tools/port_libm.py`. No native libm binary is shipped to the browser.

Regenerate the independent original executable fixtures with:

```sh
python3 tools/capture_lmat.py --reference ~/github/asitic-re
python3 tools/capture_creators.py --reference ~/github/asitic-re
python3 tools/capture_capacitance.py --reference ~/github/asitic-re
python3 tools/capture_eddy.py --reference ~/github/asitic-re
python3 tools/capture_network.py --reference ~/github/asitic-re
python3 tools/capture_transformer.py --reference ~/github/asitic-re
python3 tools/capture_three_port.py --reference ~/github/asitic-re # also requires GDB
python3 tools/audit_three_port_backing.py --reference ~/github/asitic-re
```

These test-only commands require the original executable's legacy runtime DSOs;
`--library-path` selects them. The executable hash is pinned. Raw paired logs, BSAVE files, CAP cache files and
matrices are retained under `build/original-lmat`, `build/original-creators` and
`build/original-capacitance`; eddy logs and matrices are under
`build/original-eddy` with the original executable and libm hashes pinned.
PI2 stage matrices, Green caches and reports are under `build/original-network`.
CALCTRANS reports, 2PORTTRANS files and raw caches are under
`build/original-transformer`. Three-port records, systems and read-only backing
captures are under `build/original-three-port`; the variability audit is under
`build/original-three-port-variability`.

Creator input is bounded to native fixed buffers, 64 turns, 256 sides/via rows
or columns, and a 32 MiB creation allocation budget. Invalid commands recover
without terminating the worker. Admission guards reject native fault cases;
valid descriptor values are passed unchanged to the original builders. Native
unwritten parser/history and mesh words use explicit zero initial state, rather
than claiming process-independent parity for uninitialized bytes.

Next recreation work includes native automatic-cell controls, broader
original-executable creator/input coverage, additional validation of stable full-network metrics, and original-executable
end-to-end parity for every supported path. All recovered source is already pinned here for
that work. Browser visual QA remains outstanding when a browser is available.

GitHub Actions rebuilds the module, runs these checks, retains the numerical
difference report and GDS artifacts, then deploys `dist/` to GitHub Pages.
