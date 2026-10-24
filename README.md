# reASITIC · C++ / WebAssembly

A browser recreation of ASITIC hosted on [GitHub Pages](https://al-255.github.io/reASITIC/). The recovered C++
implementation from `~/github/asitic-re` is preserved unchanged under
`vendor/asitic`; `provenance.json` pins the source commit and every file hash.

Work in progress. The target is original ASITIC geometry and simulation algorithms,
including its technology model, filament solver, substrate effects, and metrics.
Identical results must be demonstrated against the original; compiling recovered
source to WebAssembly alone is not proof of numerical identity. In particular,
x87 extended arithmetic and transcendental instructions require a fidelity layer.

No original executable is needed by the deployed application. The C++ sources are
the math kernel. JavaScript handles the interface and GDSII serialization.

The browser workbench includes square/polygon/symmetric/multi-metal
windings, planar and 3D mirror transformers, baluns, stacked capacitor plates,
rings, wires and configurable via arrays. The original keyword parsers and
interactive creator dispatch run in C++. The advanced command editor supports
all twelve creator commands and native dimensions such as ILEN / IWID. Parameters use micrometres. The original `RESIS` DC resistance, `INDUCTANCE` conductor estimate and `RESISHF`
high-frequency resistance paths run inside the C++ kernel. These are the original
conductor estimates; the original no-eddy LMAT filament solver is available in the workbench.
The original layered CAP solver is also available, with substrate conductivity
and permittivity. Eddy-current and combined circuit/network simulation are still being integrated. GDSII uses native ribbon boundaries, real via cuts and
landing pads, with 1 µm user units and a 1 nm database grid.

```sh
# Emscripten 6.0.6, Python 3, Node 24
python3 tools/build.py                    # or --emxx /path/to/em++
python3 tools/build_native.py             # GCC i386 + libc.so.6 comparison
node tests/native-creators.mjs
node tests/original-creator-binary.mjs
node tests/parity.mjs
node tests/original-geometry.mjs
node tests/estimates.mjs
node tests/connected-analysis.mjs
node tests/capacitance.mjs
node tests/capacitance-data.mjs
node tests/worker-capacitance.mjs
node tests/lmat.mjs
node tests/original-lmat.mjs
node tests/lmat-lifecycle.mjs
node tests/worker-simulation.mjs
node tests/command-input.mjs
node tests/simulation-data.mjs
node tests/retained-math.mjs
node tests/worker.mjs
python3 -m pip install gdstk==1.0.2
python3 tests/gds_reader.py
python3 -m http.server 8766 --directory dist
```

The native build deliberately keeps x87 instructions. It needs i386 glibc at
`/lib/i386-linux-gnu/libc.so.6` and `/lib/ld-linux.so.2`. Production uses only
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
- Ten complete CAP cases match 985 native numeric fields exactly, plus 840
  original printed matrix/report fields. All 17,217,120 bytes of the original
  raw Green-table payloads match their FNV-1a-64 fingerprints. Every reference
  case repeats the original executable with a fresh cache, including a 256×256
  FFT spiral, refined/distinct grounds, capacitor plates, and rotated wires.
- All 681 hardware-x87 instruction/arithmetic cases match their consumed
  binary64 outputs. Nine raw binary80 differences are recorded separately;
  this remains a limit on universal microcode identity.
- All 96 recovered scalar/frequency/grid/angle parser cases match i386 exactly,
  including the original negative-angle normalization behavior.
- Both transformer windings match the unchanged i386 solver. Rejected meshes
  recover, simulation does not mutate source geometry, and 100 repeated solves
  retain a bounded heap. An independent gdstk reader validates 32 GDS exports.

These are bounded tests, not universal parity claims. All listed exact
comparisons are required CI gates. The source translator rejects unknown
assembly. Native Clang AST ranges expose implicit long-double-to-double stores
in creator and layered-potential units; the build retains an expression/offset report. This preserves
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
default same-name reference. Power-of-two FFT sizes 2–256, a 1–512 contact limit,
and a 128 MiB table budget bound computation. Collapsed/out-of-bounds contacts
and native fault cases are rejected before matrix indexing. There is no
persistent browser table cache yet; fine grids can take tens of seconds. The
original nonsquare indexing is preserved with sufficient zero backing storage;
original-executable parity is currently measured on square FFT grids only.
Eddy effects and combined network/port metrics remain outstanding.

The worker protocol is tested with actual WASM startup, original wire matrices,
streamed frequency samples, cancellation and error recovery. Scientific export
checks preserve SI units, signed zeros and negative results. Rendered browser QA
remains outstanding because no browser was available in this session.

`simulate(winding, frequency_hz, length_cell, width_cell, thickness_cell,
subdivision_trigger, filament_limit)` exports the complete impedance matrix,
L/R/X and conductor series Q. Mesh dimensions are micrometres; the subdivision
trigger retains the original CCELL interaction. The original no-eddy driver is
compiled without its unused full-eddy entry so the compiler can eliminate that
unbound branch; no electromagnetic formula is replaced. Counts are produced by
the original routines. A configurable 1–1024 filament budget rejects oversized
requests without silently coarsening the mesh. The original source geometry is
preserved, and via cut counts are retained.

Regenerate the independent original executable fixtures with:

```sh
python3 tools/capture_lmat.py --reference ~/github/asitic-re
python3 tools/capture_creators.py --reference ~/github/asitic-re
python3 tools/capture_capacitance.py --reference ~/github/asitic-re
```

These test-only commands require the original executable's legacy runtime DSOs;
`--library-path` selects them. The executable hash is pinned. Raw paired logs, BSAVE files, CAP cache files and
matrices are retained under `build/original-lmat`, `build/original-creators` and
`build/original-capacitance`.

Creator input is bounded to native fixed buffers, 64 turns, 256 sides/via rows
or columns, and a 32 MiB creation allocation budget. Invalid commands recover
without terminating the worker. Admission guards reject native fault cases;
valid descriptor values are passed unchanged to the original builders. Native
unwritten parser/history and mesh words use explicit zero initial state, rather
than claiming process-independent parity for uninitialized bytes.

Next required work includes broader original-executable creator/input coverage, original eddy and
combined circuit/network solvers, coupled-winding and multiport metrics, and original-executable end-to-end
parity for every supported path. All recovered source is already pinned here for
that work. Browser visual QA remains outstanding when a browser is available.

GitHub Actions rebuilds the module, runs these checks, retains the numerical
difference report and GDS artifacts, then deploys `dist/` to GitHub Pages.
