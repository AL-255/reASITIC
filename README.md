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
windings, planar and 3D transformer builders, rings, wires and a 2×2 via-array
example. Parameters use micrometres. The original `RESIS` DC resistance, `INDUCTANCE` conductor estimate and `RESISHF`
high-frequency resistance paths run inside the C++ kernel. These are the original
conductor estimates; the original no-eddy LMAT filament solver is now available in the WASM API.
Substrate and distributed-network simulation are still being integrated. GDSII uses native ribbon boundaries, real via cuts and
landing pads, with 1 µm user units and a 1 nm database grid.

```sh
# Emscripten 6.0.6, Python 3, Node 24
python3 tools/build.py                    # or --emxx /path/to/em++
python3 tools/build_native.py             # GCC i386 + libc.so.6 comparison
node tests/parity.mjs
node tests/original-geometry.mjs
node tests/estimates.mjs
node tests/connected-analysis.mjs
node tests/lmat.mjs
node tests/original-lmat.mjs
node tests/lmat-lifecycle.mjs
node tests/worker.mjs
python3 -m pip install gdstk==1.0.2
python3 tests/gds_reader.py
python3 -m http.server 8766 --directory dist
```

The native build deliberately keeps x87 instructions. It needs i386 glibc at
`/lib/i386-linux-gnu/libc.so.6` and `/lib/ld-linux.so.2`. Production uses only
WebAssembly, without Python, the native executable, or a remote simulation API.

Current measured evidence:

- All 6,972 numeric fields in 20 i386/WASM construction cases match exactly.
- All 120 conductor-estimate fields across 50 frequency cases match exactly.
- All 33,836 LMAT fields across 33 native/WASM cases match exactly, including
  coarse, cross-section and unequal longitudinal meshes, phase and rotation.
- Two fresh unmodified original-executable runs per winding case produced
  identical physical outputs. WASM matches all 2,277 raw BSAVE geometry values
  and 33,704 matrix components at the original LMAT 20-decimal output boundary.
- Seven additional original SAVE fixtures pass 955 printed-precision checks;
  eleven connected command reports pass 121 L / DC / R(f) checks.
- Both transformer windings match the unchanged i386 solver. Rejected meshes
  recover, simulation does not mutate source geometry, and 100 repeated solves
  retain a bounded heap. An independent gdstk reader validates 20 GDS exports.

These are bounded tests, not universal parity claims. All listed exact
comparisons are required CI gates. The source translator rejects unknown
assembly. Arithmetic and square root use Berkeley SoftFloat binary80;
logarithms, trig and exponent fractions use binary128 guard evaluation with
native x87 constants, followed by the original binary80 stores. This avoids
Emscripten's binary64 `logl` fallback. The SoftFloat license ships with the site.
Raw binary80 microcode equality beyond the consumed/tested stores remains
unproven. Native comparison builds use the reference's
`-fexcess-precision=standard` rule.

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
```

This test-only command requires the original executable's legacy runtime DSOs;
`--library-path` selects them. Its executable hash is pinned. Raw paired logs,
BSAVE files and LMAT outputs are retained under `build/original-lmat`.

Next required work includes capacitor/balun command composition, arbitrary via
arrays, command parameter normalization, original eddy and CAP/network
solvers, frequency sweeps, and original-executable end-to-end
parity for every supported path. All recovered source is already pinned here for
that work. Browser visual QA remains outstanding when a browser is available.

GitHub Actions rebuilds the module, runs these checks, retains the numerical
difference report and GDS artifacts, then deploys `dist/` to GitHub Pages.
