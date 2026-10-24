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

The first browser milestone includes square/polygon/symmetric/multi-metal
windings, planar and 3D transformer builders, rings, wires and a 2×2 via-array
example. Parameters use micrometres. The original `RESIS` DC resistance, `INDUCTANCE` conductor estimate and `RESISHF`
high-frequency resistance paths run inside the C++ kernel. These are the original
conductor estimates; the full filament/substrate/network simulation is still
being integrated. GDSII uses native ribbon boundaries, real via cuts and
landing pads, with 1 µm user units and a 1 nm database grid.

```sh
# Emscripten 6.0.6, Python 3, Node 24
python3 tools/build.py                    # or --emxx /path/to/em++
python3 tools/build_native.py             # GCC i386 + libc.so.6 comparison
node tests/parity.mjs
node tests/original-geometry.mjs
node tests/estimates.mjs
node tests/connected-analysis.mjs
node tests/worker.mjs
python3 -m pip install gdstk==1.0.2
python3 tests/gds_reader.py
python3 -m http.server 8766 --directory dist
```

The native build deliberately keeps x87 instructions. It needs i386 glibc at
`/lib/i386-linux-gnu/libc.so.6` and `/lib/ld-linux.so.2`. Production uses only
WebAssembly, without Python, the native executable, or a remote simulation API.

Current measured evidence: 19 of 20 i386/WASM construction cases match all numeric
fields exactly; the unrotated symmetric polygon has 24 near-zero coordinate
differences (about 1e-19 µm). Seven original-executable SAVE fixtures pass 955
coordinate/resistance comparisons at printed precision. An independent gdstk
reader checks 20 exported libraries, including units, cuts and pads.
These are bounded tests, not universal parity claims. `node tests/parity.mjs
--strict` remains a failing final-fidelity gate until every difference is fixed.

The source translator rejects unknown assembly. Each retained arithmetic
operation and square root uses Berkeley SoftFloat binary80; trig/log/exp
instructions currently use Emscripten libm and remain a fidelity gap. The
SoftFloat license is distributed alongside the site.

The conductor-estimate gate checks 120 numeric fields over 50 frequency cases
against unchanged recovered i386 bodies: 36 differ, with maximum relative error
9.1e-15. Eleven connected-process report fixtures additionally pass 121 comparisons
at their printed precision. `node tests/estimates.mjs --strict` exposes the
remaining bitwise differences. Native technology initialization retains both
height-conversion stores and sheet-resistance unit conversion in C++.

Next required work includes capacitor/balun command composition, arbitrary via
arrays, command parameter normalization, original filament/eddy and CAP/network
solvers, frequency metrics/sweeps, and original-executable end-to-end
parity for every supported path. All recovered source is already pinned here for
that work. Browser visual QA remains outstanding when a browser is available.

GitHub Actions rebuilds the module, runs these checks, retains the numerical
difference report and GDS artifacts, then deploys `dist/` to GitHub Pages.
