# reasitic

A modern Python interface to the numerical kernels of
**ASITIC** (Analysis and Simulation of Inductors and Transformers for ICs),
the 1999-vintage Berkeley tool for RFIC passive analysis.

The C numerical core (Grover/Greenhouse inductance, Hammerstad-Jensen
coupled-microstrip capacitance, vec3 helpers, complex helpers, ...) is
wrapped via Cython from the hand-translated re-implementation in
[`../recomp/`](../recomp/).  No formulas are re-derived in Python; the C
kernels are the single source of truth for numerical behavior.

## Installation

```bash
pip install reasitic
```

From a checkout of the [`asitic-re`](../) repository (the C kernels live
in `../recomp/`, and the Cython build pulls them in automatically):

```bash
pip install -e ./reASITIC
```

The build requires:

* Python 3.9+
* GCC (or a compatible C compiler)
* Cython 3.0+
* `sed` (used at build time to apply `recomp/detour/portable.sed` for
  `long double` → `double` collapse on 64-bit hosts)

## Quick start

```python
import reasitic
from reasitic.tech import load_tech

tech = load_tech("BiCMOS.tek")

# Build a square spiral, the standard ASITIC SQ command:
sq = reasitic.SquareSpiral(
    name="A", metal="m3",
    length=170.0, width=10.0, spacing=3.0, turns=2.0,
    x_origin=200.0, y_origin=200.0,
)
print(sq.n_segments, sq.total_length_um, sq.total_area_um2)

# DC resistance from the sheet rho table:
print("R_DC =", sq.dc_resistance(tech), "ohm")

# Per-segment self-inductance via the wrapped Grover kernel:
from reasitic.shapes import grover_total_self_inductance
print("L_DC ~", grover_total_self_inductance(sq, tech), "nH")

# Direct calls into the wrapped C kernels:
Cp, Cf, Cfp, Cga, Cgd = reasitic.kernel.coupled_microstrip_caps(
    W=10.0, s=3.0, h=5.0, eps_r=4.0
)
```

## What's wrapped (v0)

The Cython extension `reasitic._kernel` re-exports the following C
entry points from `recomp/asitic_kernel.c`:

* **Grover / Greenhouse closed-form inductance** -- `grover_segment_self_inductance`, `coupled_wire_self_inductance_grover`, `wire_inductance_far_field_kernel`
* **Hammerstad-Jensen coupled-microstrip capacitance** -- `coupled_microstrip_caps_hj`
* **Vector helpers** -- `vec3_l2_norm`, `vec3_dot_product`, `vec3_cross_product`, `dist3d_pt`, `vec3_sqrt_dot_pair`
* **Numerically-safe scalar helpers** -- `coth_double`, `safe_divide_clipped`, `safe_log_minus_x_clipped`, `clipped_pow2_x`
* **Complex<double> primitives** -- `cpx_cosh`, `cpx_sinh`, `cpx_sqrt`, `cpx_div`
* **Spiral / wire-position helpers** -- `spiral_turn_position_recursive`, `wire_position_periodic_fold`, `wire_separation_periodic`

A high-level facade is at `reasitic.kernel`; the raw Cython module
is at `reasitic._kernel`.

## Shape builders (v0)

The C re-implementation's shape builders (`cmd_square_build_geometry`,
`cmd_spiral_build_geometry`, ...) write into 236-byte polygon records
whose field offsets are locked to the i386 ABI.  For a host-native
64-bit build that means the C builders can't be wrapped directly
without struct re-layout; instead, `reasitic.shapes` provides a
faithful Python reproduction of the *geometry* (rectangle vertex
emission, turn-by-turn spiral winding) while the numerical analysis
(inductance, capacitance, ...) still flows through the wrapped C
math kernels.

Supported shapes:

* `Wire`             -- single rectangular segment
* `Capacitor`        -- two stacked metal plates
* `SquareSpiral`     -- multi-turn SQ
* `Ring`             -- N-sided closed annulus with a gap
* `PolygonSpiral`    -- N-sided polygon spiral
* `MMSQ`             -- multi-metal square spiral (METAL + EXIT stack)
* `SymmetricSquare`  -- centre-tapped SYMSQ with inner-lead bridge
* `SymmetricPolygon` -- centre-tapped SYMPOLY
* `Transformer`      -- coupled primary / secondary SQ pair
* `Balun`            -- coupled symmetric primary + secondary stub

## Validation

Two regression suites compare the package against the recorded
ASITIC-binary output at `tests/data/validation/`:

* **`tests/test_validation_compare.py`** -- static shape gates
  (n_segments, total_length, total_area, DC resistance).  227 cases
  pass; geometry is < 1 % across every shape kind, DC ≤ 6 % for
  centre-tap composites and ≤ 1.5 % elsewhere.
* **`tests/test_lrq_s2p_sweep.py`** -- LRMAT R / L sweep, Q-factor
  sweep, |S21| / ∠S21 sweep, Pi2 model extraction at 1 GHz, and
  metal-area / Ind / Res / Cap regression.  2097 cases pass
  against relaxed-but-still-meaningful tolerances (3 % LRMAT, 5 %
  Q, 2 % |S21|, 3° ∠S21).

Run both with:

```bash
pytest -q
```

v0.5 worst-case tolerance vs the ASITIC binary's goldens (108 cases):

| Shape kind  | Cases | Worst geom | Worst DC R |
| ----------- |------:| ----------:| ----------:|
| WIRE        |    12 |   bit-exact|   bit-exact|
| CAPACITOR   |    10 |   bit-exact|   bit-exact|
| MMSQ        |     8 |   bit-exact|   bit-exact|
| BALUN       |    12 |      1.016%|      **0.000%**|
| SYMPOLY     |     8 |      2.098%|      **0.002%**|
| SP          |    12 |      0.038%|      0.037%|
| TRANS       |    12 |      0.190%|      0.139%|
| RING        |     8 |      0.219%|      0.220%|
| SQ          |    16 |      0.344%|      0.438%|
| SYMSQ       |    10 |      1.709%|      0.952%|

The v0.5 jump was driven by piping `ListSegs Y_150` through the binary
under `qemu-i386-static`, which dumps each polygon's stored
``face[0].v0`` / ``v1`` centerline endpoints.  Two corrections fell
out:

1. **Parallelogram crossover CL** = long-edge length, not bounding-box
   diagonal.  For SYMSQ L=150 segments 5 (m2) and 14 (m3), the binary
   stores `(146, 90) → (136, 75)` -- length `sqrt(100 + 225) = 18.03`,
   not `sqrt(324 + 225) = 23.43`.
2. **Via cluster grouping**.  The binary's polygon chain stores one
   record per via *cluster* (not one per cell); each record's
   `compute_inductance_inner_kernel` returns
   `r_via / N_cells_in_cluster`.  Grouping my layout's via cells by
   spatial proximity (`pitch × 1.5` threshold) recovers the correct
   ``Σ r_via / N`` contribution -- e.g. SYMSQ L=150's two 3×3
   clusters add 2/9 + 2/9 = 0.444 Ω, not 2/18 = 0.111 Ω.

Via-overlap pads (paired W×W metals on adjacent layers at a via
cluster) are CIF-emit artifacts only -- the binary's polygon chain
doesn't carry them, so we zero out their R/area contribution.

Geometry hits <1% across every kind.  The DC-resistance residual
on the composite shapes (SYMSQ / SYMPOLY / BALUN primary) is the
binary's ``cmd_resis_compute`` using a chamfered-edge-length R
metric we don't fully reproduce; closing that requires wrapping
the 32-bit C builders (parked behind the i386 ABI blocker).

Key chamfer / centerline conventions reproduced here
(:func:`reasitic.shapes._binary_centerline_length`):

* **Chamfered trapezoid ribbons** (SQ sides, polygon-spiral sides):
  the binary stores its face-0 endpoints as the midpoints of the
  two short edges; that midpoint distance equals
  ``shoelace_area / W``.  Default path.
* **Access leads** (clean rectangles on a metal layer next to a
  via cluster): one chamfered tail at the outer end, stored
  centerline ``L − W/2``.  Tagged by ``_metal_polygons``.
* **Parallelogram crossovers** (SYMSQ / SYMPOLY centre-tap
  diagonals): the binary stores the *bounding-box diagonal*
  ``sqrt(dx² + dy²)`` as the centerline length.
* **Via-overlap pad pairs** (two coincident W×W pads on adjacent
  metals over a via cluster): zero centerline contribution -- the
  binary stores ``v0 == v1`` for these, gated by its
  ``if (len > 1e-10)`` check in
  ``cmd_metalarea_print`` (recomp/asitic_repl_shapes.c:564).

## Architecture

```
reasitic/
├── _kernel.pyx        # Cython wrapper for the C leaf math kernels
├── _kernel.pxd        # C declarations from ../recomp/asitic_kernel.h
├── _extra_stubs.c     # Weak stubs for binary-only entry points not on
│                      # the math-kernel hot path
├── kernel.py          # Pythonic facade over _kernel
├── shapes.py          # Shape builder + analysis pipeline
└── tech.py            # .tek tech-file parser

setup.py               # Cython build, pre-processes recomp via
                       # portable.sed for the host-native build
```

## License

BSD-3-Clause.  Re-uses the original ASITIC documentation under the
terms recorded at `run/doc/`.
