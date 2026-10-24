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
all twelve creator commands, 41 original creator spellings and native dimensions
such as ILEN / IWID. Aliases include SQ/SQR/RECT, SP/SPI/S, W, CONTACT/V,
TRANSFORMER/TR/T, CAPTOR/CCAP, BALSQ/CENTERSQ, BALPOLY/CENTERPOLY,
MMSQ/SQMM/RECTMM and HALO/CIRCLE/CIRC. Command matching uses ASCII case
normalization and native comma/tab delimiters. The original catalog's mixed-case
3DTrans/Trans3D/TransMIM entries cannot be reached after its own normalization;
use 3DMIRRORTRANS for that creator. Add devices
to a shared layout to construct independent ground conductors and coil assemblies;
new devices require unique names and the same technology. Rejected additions,
including late export failures, preserve the previous layout and solver state. Parameters use micrometres. The original `RESIS` DC resistance, `INDUCTANCE` conductor estimate, `RESISHF`
high-frequency resistance, low-frequency `Q` and `METALAREA` paths run inside the C++ kernel. These are the original
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
The loaded-input panel adds the original `2PZIN` metric after PI2, PIX, PI4 or
CALCTRANS: select either port, a complex load and optional shunt capacitors.
It uses the selected result's frequency and exports the input impedance as CSV.
PI2, PIX, PI4 and CALCTRANS also support stepped port frequency sweeps, streamed
results, cancellation, combined native records and sweep metrics/S CSV exports.
Original 3PORT provides three-winding models and S/Y/Z conversion. GDSII uses native ribbon boundaries, real via cuts and
landing pads, with 1 µm user units and a 1 nm database grid.

The layout editor runs the original geometry command parser and all 21 recovered
editing/assembly commands in C++: COPY, RENAME, ERASE, PHASE, MOVE/MOVEX/MOVEY,
MOVETO, ROTATE, FLIPV/FLIPH, FLIP, JOIN, JOINSHUNT, SPLIT, BEFRIEND/UNFRIEND,
SPTOWIRE, HIDE and SELECT/UNSELECT. Native aliases and the selected-shape `.`
reference work. Enter multiple commands in the editor to apply them as a batch.
Editing clears stale simulation results; subsequent solves use the committed
layout. Hidden shapes remain in physical GDS export and simulation.

Each edit batch clones the complete shape/segment graph, including paired
windings, parallel branches and their shared next-record links. Failed commands,
invalid geometry and late JSON export failures release the candidate and retain
the live layout. Destructive edits clear references to removed records.
JOINSHUNT's original fallthrough reads a deleted source pointer as a phase;
production explicitly selects positive phase, keeping allocator address bits out
of the mathematical model. Historical arithmetic mode retains the original
fallthrough for diagnostic comparisons.

Edit batches are limited to 32 KiB, 128 commands and a 32 MiB allocation budget;
shape names are printable ASCII up to 79 characters (65 before SPTOWIRE).
Admission guards reject native empty-tail, zero-side, self-join, duplicate-name,
invalid-partner and excessive shunt-source cases. All twelve creator layouts
survive graph cloning unchanged. Eight repeated original edit batches cover
1,813 exact physical binary64 fields, 19 original LMAT matrices, byte-identical
GDS generation and independent GDS-reader checks for 291 polygons. Transaction
checks include partial edits, export-buffer exhaustion, lifetime recovery,
selection, paired movements, parallel branches, empty layouts and subsequent
simulation. Browser visual QA remains unverified.

Creator alias verification uses 176 fresh unmodified original processes: four
case/delimiter variants for each of 44 catalog entries. All 41 accepted aliases
match the canonical original physical binary64 fields; the three unreachable
entries retain their original unknown-command rejection. The actual WASM and
worker paths reproduce all 164 accepted variants and byte-identical GDS files.
`tests/creator-aliases.mjs` also checks the adapter against the pinned native
catalog and rejects non-ASCII case folding that the original does not perform.

Creator parameter coverage includes all twelve families on CMOS and BiCMOS,
plus rotated, asymmetric, fractional-turn and inner-dimension variants. Thirty-seven
cases compare 11,600 physical fields exactly with repeated original BSAVE records,
DC resistance and 43 original LMAT matrices. Direct WASM and worker results agree;
native C++ also checks the CMOS cases. An independent reader verifies all 646 GDS
ribbons, via cuts and landing pads.

The creator form and C++ bridge also accept windings above 64 turns. Seventeen
additional original-executable cases cover fractional square/polygon turns,
symmetric windings, planar transformers, a 131-turn balun, multi-metal windings,
3D mirror transformers and prompted square creation on both process stacks.
Two fresh creators and two fresh reloads per case verify physical records and
DC resistance; selected high-turn LMAT matrices are checked in WASM and the
worker. GDS boundaries are independently checked against the original records.
The original turn-fit calculations remain authoritative. A winding's minimum
record requirement is checked against the 32 MiB creation allocation budget,
and actual construction/export limits still recover transactionally. The older
low-level generator now uses the same allocation recovery. SWEEP's native and
WASM enumerators also retain candidates beyond 64 turns.

Polygon sides and via rows/columns likewise follow the original count domain,
without the former 256 ceiling. Seventeen further original cases cover 512-side
spirals and rings, odd-sided symmetric polygons, 300×300 via grids, rotated and
reflected arrays, prompted counts, and a two-million-column via row. Their physical
records and DC resistance repeat in fresh original creation/reload processes.
VIA references use its working original prompted path; the original dispatcher
rejects keyword arguments. The form adapter produces the same physical records.
A complete 640×640 original LMAT reference also checks the larger polygon in
WASM and the worker. The native/WASM SWEEP planners retain 512-side candidates.

Large via previews use repeated SVG tiles instead of one DOM element per cut;
two million cuts need eight DOM nodes. Small previews retain their former
polygons and styling. Independent SVG raster checks verify native cut centers
and gaps after rotation/reflection. GDS still exports every cut as a flat
boundary, writing directly into one preflighted buffer. Seventy-eight streams
match the previous encoder byte for byte. A separate bounded-memory reader
checks every cut, pad, layer and signed coordinate in the 128 MB large-grid
stream; gdstk independently parses its header, pads and farthest cut.
The download's Export limit defaults to 64 MiB and can be increased to 512 MiB.
Oversized exports reject before expansion or buffer allocation, preserving the
layout for another export or further editing.

The multi-metal creator allocates one pointer per layer, including both endpoints:
`delta + 1` entries. The original allocates `delta` but writes through `delta`,
which corrupts the heap for a three-layer difference. The port fixes the count
directly and removes the bridge's blanket allocation padding. Tests span every
supported layer difference. For the original case that faults on exit, captures
retain that status and verify completed geometry and DC outputs twice. Separate
original processes reload those unmodified records, verify the geometry survives
the round trip, and produce repeatable LMAT references.

Imported metal `SMIN` and `WMIN` values reach the original C++ symmetric-creator
prompts. They select its transition-clearance loop and width warnings; they do
not impose a new blanket clamp on creator dimensions. The C++ boundary keeps
full binary64 process values without grid snapping, preserves signed zero,
maps the original `-271` sentinel to zero and other negative minima to `0.1`.
Worker technology metadata reports the effective values. Reconfiguring or
resetting a process clears old minima.

Twelve repeated original process-rule captures check raw normalized metal words,
4,584 physical fields and the exact clearance/width/turn-retry diagnostics. In
one captured case, `SMIN=4` makes the original retry a 10 µm transition and accept
24 µm; the previous import silently discarded that rule and generated 10 µm.
Native C++, WASM and actual worker geometry now agree with the executable.
An independent GDS reader checks all resulting ribbons, via cuts and landing
pads. Non-grid minima, negative/default/signed-zero rules, keyword behavior and
request/reset recovery are covered separately.

The conductor estimates include the original low-frequency extrapolated `Q`,
computed from the unrounded extended-precision inductance and DC resistance.
It is distinct from the distributed network's high-frequency Q. Undefined Q,
including a zero-resistance via, is returned as `null` and displayed as undefined.
`METALAREA` sums three-dimensional segment length times width in µm², following
the primary chain and its original length threshold. Secondary shunts are
excluded, and overlapping ribbons are counted separately. The estimate panel
exports all winding metrics and the actual normalized frequency as CSV.
Thirty-nine original geometry cases in both process stacks supply 528 captured
binary64 metric words from two repeated original runs per case. Historical
arithmetic reproduces these words; production retains the stable geometry
integral and agrees with the original L/Q to better than `1e-12` relative error.
DC resistance, high-frequency resistance and area match the original words.
An independent 80-digit physical DC/area calculation and Q equation, two shunt
controls, 71 retained-intermediate witnesses, worker parity and recovery checks
are included in `tests/conductor-metrics.mjs`.

The original `COUPLING` conductor estimate is available alongside L, R(f) and Q.
Select two windings to report the mutual inductance in nH and coupling coefficient.
It uses the original segment-pair estimate and self-inductance normalization;
the full transformer/network solvers remain available for distributed substrate
effects. The C++ boundary copies both main segment chains before calling the
original estimator, whose temporary endpoint translations can change geometry
even after a successful call. Live geometry, GDS and following solves remain
unchanged. Its default budget of 250,000 pair evaluations includes each winding's
self pairs; the worker accepts an explicit budget up to 1,000,000.

Twenty-six repeated original cases cover CMOS/BiCMOS wires, rotations, large
origins, symmetric coils, planar/3D transformers, baluns, via-only lists and parallel
branch assemblies.
Read-only original entry/return observations check 52 binary64 values exactly
in historical mode. Production reports match the original and differ by at most
`5.4e-14` relatively on these cases. Checks also cover an independent 60-digit
Neumann integral, reciprocity, original zero limits, request/budget recovery,
source immutability and bounded heap use. Host x87 rounding is recorded separately
with a `1e-12` bound; `--strict-native` requests exact host instruction identity.

The numerical `SWEEP` coil-design search is available for square and polygon
spirals. Its C++ planner extracts the original range loops, maximum-turn routine,
binary32 keyword normalization, fractional turn stepping and acceptance predicate.
Candidate descriptors go directly into the C++ constructors, avoiding a second
parse or grid snap. A private worker kernel streams candidate metrics and failures
without replacing the live layout. Stop retains completed candidates; applying a
candidate makes it available for the normal simulation and GDS controls. Geometry
can be applied even when its current contact mesh fails. CSV export includes all
completed candidates and their pi metrics.

The original acceptance predicate compares LMIN in nH, LMAX in µH and first-port
Q; this unit asymmetry is displayed explicitly. SIMTYPE=2 uses PI2. SIMTYPE=3
validates TAP but the original grid still analyzes the candidate against itself;
that behavior is retained. The original FAST conductor model faults and is
rejected. The web path uses the supplied NAME instead of reading the original
batch loop's unwritten constructor-name buffer. Candidate and range-iteration
budgets reject a search without silently truncating its parameter grid.

Ten repeated original searches cover both process stacks, square/polygon grids,
fractional ranges, quality/inductance filtering and TAP validation. Captures stop
read-only after original keyword normalization, then feed those exact normalized
values to fresh original prompted searches, bypassing the native batch-name hang.
They retain 54 descriptors/geometries, 38 accepted rows and 12 nonfinite models.
Historical metrics are compared with native captures; production metrics are
checked against an independent 80-digit circuit reduction. Small shunt-conductance
cancellation can change full-precision legacy resistance values, so these are
recorded separately from production accuracy. The largest relative metric error
against that reduction is below `2.5e-12` in production, versus about `2.1e-8` in the
original on these cases. An independent GDS reader checks all 54 libraries and
1,811 polygons. Browser visual QA remains unverified.

Search kernels enable the original Green-table cache file protocol using bounded
RAM files. Exact frequency bits, full substrate/metal records, FFT/chip dimensions and
arithmetic mode select entries, preventing the native rounded filename from
aliasing different frequencies. Two entries use at most 32 MiB total. Ordinary
solver requests keep their existing cache policy. Nonsquare grids and larger
tables still recompute. Fresh/cached matrices and hashes are checked exactly,
including process changes, frequency aliases, arithmetic isolation and release.

`SWEEPMM` extends design searches to the original multi-metal square constructor.
Select top METAL and a lower EXIT to span the layers between them. The planner
extracts this command's own original iteration and origin rules; construction
uses the corrected `delta + 1` pointer workspace. Temporary copy names are
checked before construction, so appending a conflicting candidate preserves the
live geometry and selection. NAME is used explicitly rather than as unwritten
batch-frame data.

The original non-four-sided SWEEPMM path is kind 20 and constructs a single-metal
polygon. That behavior is retained and identified in the interface. SWEEPMM
always uses numerical PI2, including SIMTYPE=1, despite its original FAST header.
Its AUTOCELL update is skipped for SIMTYPE=1; use SIMTYPE=2 or 3 for automatic
cells. Candidate application, CSV, cancellation and private-kernel isolation use
the same workbench controls as SWEEP.

Nine repeated unmodified original SWEEPMM searches capture 43 candidates across
all four supported CMOS metal spans and BiCMOS, fractional ranges, polygon paths
and SIMTYPE=1 at a different frequency. All descriptors, 10,586 physical fields
and 42 accepted printed rows match; the one original nonfinite model remains a
rejection. Independent 80-digit circuit checks bound production metric error
below `1.1e-12` on these cases, versus about `9.9e-7` for original extraction.
An independent GDS reader verifies 43 libraries and 500 polygons. Worker checks
also cover applying candidates, following simulation, temporary-name recovery,
AUTOCELL/manual equivalence for a coarse mesh and SIMTYPE=1's bypass.

The original numerical `SELFRES` search is available separately from the
pi-model resonance estimate. It repeatedly analyzes the selected device and
ground with the original PI2 SLOW driver and searches single-ended or
differential input reactance. The C++ routine retains its extended-precision
midpoints, optional native fast-step quantization, stop tests and ignored native
iteration argument. A replay boundary requests one missing admittance at a time;
JavaScript coordinates the C++ solves and yields for cancellation without doing
search arithmetic. Automatic mesh limits are recalculated at each saved query.

The search reports the native returned frequency, the last frequency actually
analyzed, its stopping criterion and whether sampled reactance changed sign.
These are distinct: the original can return an untested frequency after interval
convergence, including in an unbracketed range. Analysis budgets stop with an
explicit incomplete result. The native hard limit executes up to 101 analyses
but consumes only 100 of them; CSV marks whether each analysis was used and
retains exact saved Hz and raw model admittances. The original capacitor creator
becomes a generic joined structure, so its numerical SELFRES route also uses
PI2. FAST requests for those structures fault in the pinned original and are
rejected by the worker; the original kind-4 capacitive conversion and search
quantization remain implemented for that model class.

Seven repeated unmodified SELFRES captures include 60 complete finite model
queries/returns and one reproducible FAST fault. `tests/self-resonance-plan.mjs`
checks those saved frequencies and returns and 185 exact native/WASM replay
plans, including the 101-analysis edge and budget recovery. Actual worker tests
compare every saved model against the executable, preserve returned-versus-
analyzed frequencies, and cover automatic cells, cancellation and recovery.

Automatic cell sizing (AUTOCELL) applies to LMAT, CAP, PI2, PIX, PI4,
CALCTRANS and 3PORT, including each saved sweep frequency. The recovered C++ rule
uses the last technology metal: longitudinal size is `alpha * c / (4 f)`;
width and thickness are `beta * skin_depth`. Both are converted to micrometres.
The same longitudinal and width limits feed capacitance contacts. Alpha and beta
use the original scalar parser; calculated limits bypass dimension parsing and
grid snapping. Manual values are retained when switching modes. Results show the
actual automatic cell sizes. Invalid meshes, FFT contact collapse and exceeded
budgets are rejected without changing the requested discretization.

`tests/automatic-cells.mjs` checks fourteen raw five-limit updates and two complete
S sweep files captured twice from the unmodified executable. It also compares
164 native/WASM evaluations and independent 150-digit wavelength/skin-depth
references across technology changes and wide frequency scales. Exact historical
S records use the explicit reference arithmetic mode; production S results retain
eight significant digits on these cases. Worker checks cover all solver paths,
per-frequency updates, direct-double equivalence, an independent nodal reduction
of a subdivided three-port mesh, and recovery after invalid cells.
`tools/capture_automatic_cells.py` regenerates the native captures using read-only
entry/return observations; `tools/reference_automatic_cells.py` is the independent
physics oracle.

The original `MAXNW` width-subdivision limit is configurable for LMAT, eddy,
PI2/PIX/PI4, CALCTRANS and 3PORT, including frequency sweeps and automatic cells.
Its default is 500. Both preflight filament counts and the actual filament
placement use this value, retaining the original nonuniform edge grading and
3x3 incoming-history override. The worker sets the policy on every solve, so a
nondefault value cannot leak into a subsequent request using the default. CAP
is independent of this inductive subdivision limit. Nondefault limits appear in
result metadata and the displayed mesh description.

The worker validates integer solver controls before WASM can truncate fractions
or wrap large values into another winding, eddy mode or budget. Winding and
substrate indices must refer to the current layout and technology. The C++
`solver_limits()` export supplies the storage and work limits to JavaScript;
the native and WASM builds share the same dense-matrix storage policy.
`tests/worker-integer-controls.mjs` rejects 523 malformed controls across twelve
solver, resonance and sweep paths, checks exact results after rejection, and
verifies that requests preserve the source geometry.

`tests/maxnw.mjs` checks nine repeated original LMAT matrices, including grading
thresholds, rotated geometry and thickness subdivision. `tests/maxnw-ports.mjs`
checks eleven full original S/Y/Z files across PI2, PIX, PI4, CALCTRANS and 3PORT.
Production uses the stable integral and retains ten-digit agreement by matrix
norm on these cases. An independent 80-digit PIX reduction eliminates the chain
currents before solving a smaller system and checks the admittance, currents and
voltages. Original PIX C/Z inputs are captured separately with read-only GDB
observations. The retained-arithmetic WASM audit matches nine files exactly and
records two PIX low-bit differences; these observations do not select production
arithmetic. `--strict-historical` and `--strict-native` separately request exact
instruction-based file identity, which can depend on a host's x87 implementation. Worker checks cover all inductive paths, port sweeps,
automatic cells, budget admission and request isolation. The original zero-edge
case (active grading with fewer than three width filaments) is rejected before
its division by zero; a small limit with uniform placement remains valid.

Production numerics use deterministic software arithmetic. The rectangular
cross-section integral is normalized by geometry scale, keeps guard precision
through its cancelling terms, and rounds to binary64 once. It starts at binary128
and increases precision when the conditioning or rounding interval requires it.
QUADPACK's error-power term uses the algebraically equivalent `x * sqrt(x)` with
software guard precision instead of a logarithm/exponential sequence. The
portable fallback is pinned LibBF C with explicit rounding and its SIMD path
disabled; the vendored MIT license is included in deployment.

Folded networks (PI2, PI4 and CALCTRANS) stamp the physical nodal admittance
`C + D^T*Y*D` from the raw capacitance and branch-admittance inputs. Assembly
uses exact dyadic sums and a power-of-two normalization. With the endpoint
voltages fixed, the interior system has `n-1` unknowns. Both drives share a 128-bit software
factorization. The port matrix and node-capacitance snapshot round to binary64
only when published. PI4 also subtracts its original first-two-pad model before
rounding the final port matrix, preserving small deembedded components that
would disappear in subtraction of the published with-pad matrix. Historical
audit mode retains the original mixed current/voltage solve and pad subtraction.

PI2 and PI4 compute their small shunt admittances by a common-mode drive rather
than summing large, nearly opposite port Y entries. Writing every node voltage
as `1 + delta` makes the forcing term the raw capacitance diagonal. Internal
voltage changes are solved with 128 bits of software precision; the endpoint
currents directly give the two shunts. For PI4, each pad's raw self-capacitance
is subtracted before storing the shunt. The original pi extraction formulas
then consume these stable branches. Returned `pi_shunts` records the values
actually used: the rounded port matrix can lose conductances that the shunt
calculation retains. Historical arithmetic mode keeps the original extraction.

PI2, PI4 and PIX evaluate input impedances and differential Q from these
branches too. With series admittance `s` and endpoint shunts `a,b`, the
differential impedance is `(a+b)/(s*(a+b)+a*b)`. Exact dyadic sums and products
precede guarded division, avoiding cancellation in the determinant of rounded
port entries. When both shunts vanish, the circuit gives `1/s` directly.
The grounded inputs use `1/(s+a)` and `1/(s+b)` in the original port order.
Differential Q cancels the shared denominator magnitude before evaluation.
This uses the reciprocal Pi model already assumed by the original extraction;
the published port matrix and transformer impedance extraction are unchanged.

PIX also computes its shunts from a common-mode drive, using its midpoint
voltage/current averaging model. Current conservation eliminates the chain
currents before solving for internal voltage changes and one endpoint current:
the common-mode system has `n` unknowns instead of the original `2n+2`.
The same 128-bit software arithmetic is used. For a single segment, the midpoint
capacitor splits equally between endpoints; cancelling the impedance
symbolically preserves components even when their magnitudes differ by hundreds
of bits.

PIX port matrices and saved current/voltage states also use current elimination.
Charge conservation gives `I = I0 - T*C*B*V`; substituting this into the voltage
drop equations and fixing both endpoint voltages leaves `n` unknowns instead of
the original mixed-unit `2n+2` system. The remaining current unknown is scaled by
the largest branch-impedance component. This removes arbitrary voltage/current
units from pivot selection without inverting the impedance matrix or changing
the circuit model. Both endpoint drives share one factorization in 128-bit
software arithmetic, and results round to binary64 only when published.
Historical audit mode retains the original node solver and pi extraction.

`tests/portable-pi-shunts.mjs` checks an exact rational RC circuit whose shunt
conductance disappears entirely when rounded port entries are added, plus
one-segment and zero-capacitance limits, invalid/singular inputs and recovery.
`tests/portable-pi-impedance.mjs` checks 14 circuits against an exact rational
2x2 nodal inverse, including tiny shunts, an 800-bit component range, zero-shunt
and resonance limits, subnormal inputs and an 1800-bit admittance scale range.
Native and WASM values agree bit for bit; all four native rounding modes give
identical raw results. Physical PI2/PI4/PIX tests also verify this same evaluator
is used for the returned input impedances and differential Q.

The two-port S conversion also retains exact dyadic sums and products through
the complex quotient, rounding directly to binary64. A large floating branch
previously lost the 0.02 S reference terms during cancellation and returned NaNs
at an admittance of `2^64` S. Exact coefficient evaluation preserves these terms
without an arithmetic cutoff or hardware-specific case. The original formulas
and port order remain authoritative, including the first diagonal numerator
`(g-y22)*(g+y22)+y21*y12`, where `g` is the original stored 0.02 S constant.
Numerical records and raw S matrices use the same conversion.
`tests/portable-scattering.mjs` checks 90 circuits: exact-rational expanded
polynomials, independent traveling-wave boundaries for floating series branches,
six captured original physical matrices, asymmetric off-diagonals, true poles,
subnormal inputs and the largest finite binary64 branch. Native/WASM results
match bit for bit in all four host rounding modes; historical mode retains the
original cancellation failure and original record comparisons.

Three-port S and Y conversion likewise keeps its 3x3 cofactors and determinant
exact through the final binary64 quotient. S uses the equivalent form
`I - 2*(I + gZ)^-1`, with the subtraction performed in the exact numerator.
This preserves the identity term for large, nearly rank-one impedances and
avoids multiplying rounded inverse and identity matrices. The original port
swap and native record formatting remain unchanged. A singular impedance has
undefined Y entries; S can still be finite. Historical mode retains the
original ignored inversion failures for diagnostic comparison.
`tests/portable-three-conversion.mjs` checks 114 S/Y reductions of 57 circuits
against exact rational Gaussian elimination, including traveling-wave
boundaries and exact lossless-network unitarity. Cases include four captured
original impedance matrices, asymmetric ports, rank-one matrices, subnormal
values and extreme component scales. Native/WASM bits agree in all four host
rounding modes. The nine physical cases in `tests/portable-three-port.mjs`
also compare their converted matrices with an independent 80-digit reference
at a `1e-14` matrix-norm tolerance.

Loaded input impedance (`2PZIN`) uses the same two-port circuit and original port
order: port 1 drives `y22`, capacitor 1 shunts `y22`, and capacitor 2 shunts `y11`.
For drive/other diagonal entries `a,b`, cross entries `c,d` and load `Z`,
the C++ kernel evaluates `(1 + b*Z) / (a + (a*b-c*d)*Z)`.
Capacitor stamps, products and sums retain exact dyadic coefficients, and the
complex quotient rounds directly to IEEE binary64, including subnormals.
Multiplying through by the load removes `1/Z`; a short circuit therefore gives
`1/a`. The original instead produces NaNs for a zero-ohm load. This observation
is kept in the fixture, while production uses the physical circuit limit.
Poles, indeterminate circuits and output overflow have explicit result statuses.

`tests/loaded-port.mjs` checks 30 circuits against exact rational nodal
elimination, with native/WASM bit identity and all four host rounding modes.
`tests/worker-loaded-port.mjs` checks ten scenarios captured in twenty fresh
original processes after PI2, PIX, PI4 and CALCTRANS, including complex loads,
binary32 input normalization and asymmetric shunt capacitances. All nine finite
original reports match. Calculations preserve the saved model; layout changes
invalidate it, rejected requests preserve it, and sweeps use the last completed
sample's frequency. Only the 2x2 matrix and frequency are retained in the worker.

`tests/portable-two-port.mjs` checks 26 PI2/PI4 cases using an independent
80-digit endpoint Schur complement, including pad subtraction and full-precision
pi, impedance and resonance metrics. All 23 finite cases also compare the
same C++ port solve, pad subtraction, node snapshot and common-mode solve in
native and WASM builds bit for bit; three original
nonfinite cases retain their rejection. The largest circuit-metric relative
error in these cases fell from about `3.7e-7` to below `7e-16`; the final port
matrices match the rounded independent reference exactly.
`tests/portable-folded-ports.mjs` adds 24 analytical and scaled circuits,
including an exact rational two-branch reference, a 400-bit pad-subtraction
limit, minimum-subnormal inputs, asymmetric inputs, zero/singular limits and rejection recovery.
Native/WASM values remain bit-identical over an 1800-bit admittance-unit range
and all four host rounding modes; 400-digit reductions check the scaled circuits.
`tests/portable-transformer.mjs` checks all 16 CALCTRANS cases against independent
vertex elimination and transformer-label extraction, with native/WASM node-solve
identity and matrix/model integration checks.
`tests/portable-extended.mjs` checks another 13 PIX cases, including width
subdivision, against an independent 80-digit current-elimination reference for
the complete port matrix, saved currents/voltages, pi metrics, input impedance
and resonance. Common-mode shunts match bit for bit in native and WASM builds.
The worst metric error in these cases fell from `3.7e-7` to below `2.1e-16`.
`tests/portable-extended-shunts.mjs` adds exact midpoint-circuit limits, a
400-bit component magnitude range, impedance independence and error recovery.
`tests/portable-extended-ports.mjs` checks an exact one-midpoint circuit and
fifteen coupled circuits with impedance factors from `2^-800` through `2^800`.
Voltages are invariant and normalized currents/ports match bit for bit across
these factors and native/WASM builds. An independent 400-digit solve checks all
node states; native states also remain bit-identical under all four host rounding
modes. Ordinary physical cases retain the independent 80-digit reference.
All thirteen physical PIX cases also check native/WASM port-state identity and
require matrix/state norm errors below `1e-14`.

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
# SVG raster checks also need system Cairo (libcairo2 on Debian/Ubuntu).
python3 -m pip install gdstk==1.0.2 mpmath==1.3.0 CairoSVG==2.9.1
python3 tools/build.py                    # or --emxx /path/to/em++
python3 tools/build_native.py             # GCC i386 + libc.so.6 reference audit
node tests/worker-integer-controls.mjs
python3 tools/build_portable_math.py       # native/WASM software-math verification
node tests/portable-gmd.mjs
node tests/portable-error-power.mjs
node tests/portable-pi-shunts.mjs
node tests/portable-scattering.mjs
node tests/portable-three-conversion.mjs
node tests/loaded-port.mjs
node tests/worker-loaded-port.mjs
node tests/portable-extended-shunts.mjs
node tests/portable-extended-ports.mjs
node tests/portable-folded-ports.mjs
node tests/portable-simulation.mjs
node tests/portable-pad.mjs
node tests/portable-two-port.mjs
node tests/portable-transformer.mjs
node tests/portable-extended.mjs
node tests/two-port-output.mjs
node tests/port-schedule.mjs
node tests/self-resonance-plan.mjs
node tests/worker-self-resonance.mjs
node tests/automatic-cells.mjs
node tests/maxnw.mjs
node tests/maxnw-ports.mjs
node tests/worker-maxnw.mjs
node tests/worker-automatic-cells.mjs
node tests/port-sweep.mjs
node tests/port-dispatch.mjs
node tests/worker-port-sweep.mjs
node tests/portable-three-port.mjs
node tests/three-port-sweep.mjs
node tests/worker-three-port-sweep.mjs
node tests/native-creators.mjs
node tests/creator-aliases.mjs
node tests/creator-coverage.mjs
python3 tests/gds_creator_coverage_reader.py
node tests/creator-coverage.mjs --ranges
python3 tests/gds_creator_coverage_reader.py --ranges
node tests/creator-ranges.mjs
node tests/creator-coverage.mjs --counts
python3 tests/gds_creator_coverage_reader.py --counts
node tests/creator-counts.mjs
node tests/gds-stream.mjs
node tests/geometry-preview.mjs
python3 tests/svg_preview_reader.py
node tests/metal-rules.mjs
python tests/gds_metal_rules_reader.py
node tests/geometry-edits.mjs
node tests/worker-geometry-edits.mjs
python3 tests/gds_edits_reader.py
node tests/original-creator-binary.mjs
node tests/parity.mjs
node tests/original-geometry.mjs
node tests/estimates.mjs
node tests/conductor-metrics.mjs
node tests/connected-analysis.mjs
node tests/coupling.mjs
node tests/potential-memo.mjs
node tests/optimization.mjs
node tests/worker-optimization.mjs
python3 tests/gds_optimization_reader.py
node tests/optimization.mjs --multimetal
node tests/worker-optimization.mjs --multimetal
python3 tests/gds_optimization_reader.py --multimetal
node tests/capacitance.mjs
node tests/contact-budget.mjs             # dense CAP/PI2 references; may take minutes
node tests/capacitance-data.mjs
node tests/capacitance-grids.mjs
node tests/fft-budget.mjs                  # original 512-grid thin-wire CAP/PI2
node tests/potential-normalization.mjs
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
- All 300 conductor-estimate fields across 50 frequency cases match the native audit build exactly.
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
default same-name reference. Positive power-of-two FFT sizes, a 16 MiB budget per
dense complex contact matrix (currently 1–1024 contacts), and a 128 MiB table
budget bound computation. The Green budget counts `(nx+1)*(ny+1)+2*nx+2*ny`
complex cells for each packed metal pair, using overflow-safe integer admission.
There is no fixed 256-cell FFT ceiling. Grid squares and contact-index area
products use exact wider integers before binary64 conversion, avoiding the
original 32-bit normalization wrap. Synthetic full-domain and half-domain
first-mode contacts verify the analytic two-layer potential through 1,048,576×1 and 1×1,048,576 using explicit spectral
samples. `potential_grid_fits(nx,ny)` exposes the technology-dependent budget to the worker. FFT controls accept powers of two;
the worker rejects fractional, out-of-range and wrapping dimensions before the
WASM ABI. `max_contact_count()` similarly exposes the derived contact limit.
Collapsed/out-of-bounds contacts and native fault cases are rejected before matrix indexing. There is no
persistent browser table cache yet; fine grids can take several minutes. The
original square-grid results remain identical. Rectangular grids use a consistent
`ny+1` row stride through generation, FFT and contact lookup, with scratch sized
for both axes. The original mixes `nx+1` and `ny+1` strides and undersizes its
scratch buffer: both 8×16 and 16×8 abort in two fresh original runs. Those fault
paths remain isolated diagnostics with bounded backing, not production answers.

The rectangular calculation keeps the same spectral Green functions, cosine
transform, contact integrals and matrix solve. An independent 80-digit reference
inverts a fresh original 32×32 Green cache with direct cosine sums, truncates its
spectral modes to each requested grid, and analytically integrates the cosine
basis over contacts. It checks potential matrices, inverse/folding and equivalent
impedance in 27 one-/two-/four-contact cases, including both rectangular
orientations and one-cell axes. Full-domain contact integrals also verify exact
cosine orthogonality on 512×8, 8×512, 1024×1 and 1×1024 grids. Native/WASM, worker
and cache round trips match exactly. Four two-segment rectangular PI2 cases also check the contact-to-network
folding, independent nodal reduction and worker solve. This verifies the corrected
mathematical layout; it does not claim numerical parity with an original
rectangular run that aborts.

A 1 µm-wide M4 wire collapses at 256×256 and gives NaN in the original, but
512×512 resolves it and reports 6.218 fF. Two fresh original CAP and PI2 runs
retain their stage matrices, reports and hashes of all 63,652,080 Green bytes.
The fine-grid checks compare native/WASM/worker results, independent PI2 circuit
reduction, unchanged contact meshes and rejection/recovery. Allocation admission
also checks every pair of signed-32-bit power-of-two dimensions for one, two,
five and 32 metals against independent BigInt counts, in both production and
historical layouts. The numerical-budget CI matrix runs these longer checks
separately from the standard audit and dense-contact cases.

Two further original cases use 660 and 661 contacts for CAP and grounded PI2.
Fresh repeated original runs retain the complete 660×660 potential matrix,
folded capacitance/node stages, printed metrics and raw Green-table hashes.
WASM and worker checks preserve the native mesh, validate the original reports,
and verify rejection/recovery at the selected budget. The resource budget does
not coarsen contacts or alter FFT resolution; native CCELL pitch normalization
still applies. Dense refinement can take minutes in WASM. These longer checks
run in a separate parallel CI job, and deployment requires both jobs to pass.
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

Direct PIX and stepped `2PORTX` are different original command paths. Direct
PIX always calls the extended solver. Numerical `2PORTX` selects PIX only for
actual kinds 0, 1, 16, 17 and 18; other conductor records use PI2 and emit the
original compatibility warning. This predicate is extracted from the pinned
C++ dispatcher. Selection follows the constructed shape, not the creator's
label: `MMSQUARE` and `CAPACITOR` join their parts into kind 21 and consequently
use PI2 in a `2PORTX` sweep. Every worker sample retains its selected command,
shape kind and model in `dispatch`; CSV model labels follow the actual solver.
The `2PORTGND` reference is acquired through native argument type 8, including
an implicit device reference in the workbench. Its mesh reset is distinct from
an omitted optional `2PORTX` ground. Explicit self-ground retains its own path.

Fourteen repeated original cases across CMOS/BiCMOS verify these choices,
including wire, ring, joined/stacked shapes, both symmetric coil families,
capacitor construction, and omitted versus explicit self-ground. Read-only
driver/writer observations provide 336 original Y words and 56 complete S/Y
files; independent 80-digit circuit reductions check production matrices.
All 92 model selections over the 37 creator/process variants are checked too.
The separate kind-4 capacitive dispatcher preserves the original 2x2 conversion
and exports defined S/Y records; it does not manufacture a new Pi scalar bank
from the previous command's history. The current original capacitor creator
does not produce kind 4. An analytical 2x2 matrix checks this conversion and its
export/recovery boundaries without relabeling any native geometry.

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
private clone, folded capacitance and reduced inductance admittance retain the
original driver sequence. Production uses the endpoint-voltage reduction and
guarded shunt extraction described above; historical arithmetic mode keeps the
original node stamping, general LU solve and pi extraction.
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
The three-port host audit also measures cancellation residuals relative to the
matrix norm: near-zero entries of an inverted complex matrix need not retain
component-relative accuracy. It records bounded host differences without CPU
or case selection. CAP stages, schema, saved frequency and signed zero remain
strict, and every original rectangular/polar S/Y/Z record is still compared
exactly before the host audit. `tests/three-port.mjs --strict-native` additionally
requires native instruction-bit identity. Production accuracy is checked
separately against an independent 80-digit nodal reduction.

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
python3 tools/capture_self_resonance.py --reference ~/github/asitic-re # requires GDB
python3 tools/capture_maxnw.py --reference ~/github/asitic-re
python3 tools/capture_maxnw_ports.py --reference ~/github/asitic-re
python3 tools/capture_automatic_cells.py --reference ~/github/asitic-re # requires GDB
python3 tools/capture_creators.py --reference ~/github/asitic-re
python3 tools/capture_creator_aliases.py --reference ~/github/asitic-re
python3 tools/capture_metal_rules.py --reference ~/github/asitic-re # requires GDB
python3 tools/capture_geometry_edits.py --reference ~/github/asitic-re
python3 tools/capture_capacitance.py --reference ~/github/asitic-re
python3 tools/capture_eddy.py --reference ~/github/asitic-re
python3 tools/capture_network.py --reference ~/github/asitic-re
python3 tools/capture_loaded_port.py --reference ~/github/asitic-re # requires GDB
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

Creator input is bounded to native fixed buffers and the original signed 32-bit
numeric count domain before conversion. Creation has a 32 MiB allocation budget
and an output-size limit. Turn and side counts follow the original geometric fit
rules and these resource budgets. Via grids remain compact until GDS export,
whose separate byte budget is selectable in the interface. Invalid commands recover
without terminating the worker. Admission guards reject native fault cases;
valid descriptor values are passed unchanged to the original builders. Native
unwritten parser/history and mesh words use explicit zero initial state, rather
than claiming process-independent parity for uninitialized bytes.

Next recreation work includes broader original-executable creator/input coverage,
additional validation of stable full-network metrics, and original-executable
end-to-end parity for every supported path. All recovered source is already pinned here for
that work. Browser visual QA remains outstanding when a browser is available.

GitHub Actions rebuilds the module, runs these checks, retains the numerical
difference report and GDS artifacts, then deploys `dist/` to GitHub Pages.

The public page loads a complete browser release from `releases/<sha256>/`.
The identifier covers the JavaScript modules, WASM bytes, technology files,
styles and bundled notices. Module imports, workers, the WASM loader and process
loads stay within that release, so independently cached root files cannot mix
kernel versions. `dist/build.json` records each input's SHA-256 and byte count;
unversioned root artifacts remain available for Node verification.

If a cached page refers to a release removed by a newer deployment, startup
shows an explicit reload link that requests fresh root HTML. The same recovery
is available after a worker load fails, including older pages without that link.
An incompatible old kernel is rejected before the worker reports ready.
The worker also retains the compiled WASM module after its initial load. A later
optimizer creates a fresh instance from that code without fetching the old
release again. Instances have independent memory, geometry, process data and
solver state; the shared module contains immutable code only. Failed initial
downloads or compilation can be retried. Tests cover concurrent creation, heap
growth isolation and a real three-candidate original SWEEP after removing its
release's WASM file, followed by an unchanged live-layout CAP solve.
A separate controlled interleaving delays one private initializer until a newer
search has completed its first candidate. The obsolete initializer cannot
publish its instance over the active one; the remaining original candidates,
live geometry and applied GDS stay correct. Searches and retained applications
keep explicit references to their own private kernel. The
integration check models two cacheable deployments, verifies the module graph
and stale-page recovery, then runs real packaged WASM/worker CAP and byte-identical
GDS export. It verifies packaging and loading; rendered browser QA remains
unverified when no browser surface is available.

```sh
node tests/kernel-factory.mjs
node tests/optimizer-startup.mjs
node --experimental-vm-modules tests/site-package.mjs
```
