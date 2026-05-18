"""Shunt capacitance from metal traces to substrate ground.

For each polygon on metal layer ``m`` we model the capacitance to
the substrate's bulk ground as a parallel-plate cap

.. math::

    C_p = \\varepsilon_0 \\varepsilon_r \\, A / h

with ``A`` the polygon's footprint area, ``h`` the vertical distance
from the metal centreline to the bottom of the layer stack, and
``ε_r`` the layer-stack-averaged relative permittivity. This is a
textbook approximation; the binary's full Green's-function path
captures lateral coupling that this stub ignores.

The fringe correction adds the standard 0.5·ε₀·(perimeter) term
(Yuan & Trick 1982).

Mirrors the simpler half of ``coupled_microstrip_caps_hj``
(``asitic_kernel.c:0x0804df6c``).
"""

from __future__ import annotations

import math

from reasitic.geometry import Point, Shape
from reasitic.tech import Tech
from reasitic.units import EPS_0, MU_0, UM_TO_M


def parallel_plate_cap_per_area(eps_r: float, h_um: float) -> float:
    """C/A in F/μm² for a parallel-plate cap of dielectric ``eps_r``
    and thickness ``h`` (μm)."""
    if h_um <= 0:
        return float("inf")
    return EPS_0 * eps_r / (h_um * UM_TO_M)


def _polygon_signed_area(vertices: list[Point]) -> float:
    """Shoelace area of a 2D polygon (xy plane) in μm²."""
    n = len(vertices)
    if n < 3:
        return 0.0
    area = 0.0
    for i in range(n - 1):
        v_i = vertices[i]
        v_j = vertices[i + 1]
        area += v_i.x * v_j.y - v_j.x * v_i.y
    return 0.5 * area


def _polygon_perimeter(vertices: list[Point]) -> float:
    if len(vertices) < 2:
        return 0.0
    total = 0.0
    for i in range(len(vertices) - 1):
        total += vertices[i].distance_to(vertices[i + 1])
    return total


def shape_shunt_capacitance(shape: Shape, tech: Tech) -> float:
    """Total shunt capacitance from ``shape`` to substrate ground, in F.

    For each polygon we model the path from its metal-layer
    centreline down to the substrate ground as a stack of series
    parallel-plate caps, one per dielectric layer between the metal
    and the ground:

    .. math::

        \\frac{1}{C_\\text{path}} = \\sum_k \\frac{h_k}{\\varepsilon_0
                                                   \\varepsilon_{r,k} A}

    so the equivalent ``ε_eff = h_total / Σ (h_k / ε_{r,k})`` only
    averages layers actually in the path to ground (rather than
    every substrate layer in the tech file as the previous version
    did). The fringe term keeps the same Yuan–Trick form on
    ``ε_eff``.

    Mirrors the simpler half of ``coupled_microstrip_caps_hj``
    (``asitic_kernel.c:0x0804df6c``).
    """
    if not tech.layers:
        return 0.0
    total_C = 0.0
    # Sum across every ribbon record (matching ``cmd_metalarea_print``
    # semantics): each ribbon contributes its xy-footprint as the
    # parallel-plate "area" and 2·long-axis as the fringe perimeter.
    # See :func:`reasitic.info.metal_area` for the closed-vs-polyline
    # ribbon-record convention used in Python.
    #
    # Ground-plane convention: ASITIC's microstrip model treats the
    # topmost heavily-doped (low-ρ) substrate layer as the cap ground.
    # For a metal in an insulator layer (oxide), the cap path is the
    # metal's own ``d`` (distance from the layer's bottom — which sits
    # against the ground plane if the next layer below is conductor)
    # plus any all-insulator layers between metal and ground.
    # Reverse-engineered from coupled_microstrip_caps_hj / Yuan–Trick:
    # the C uses ``h`` = depth-to-ground and ``eps`` = pure dielectric
    # value of the path, NOT a weighted average over the lossy stack.
    GROUND_RHO = 1.0  # Ω·cm; layers with ρ ≤ this are treated as conductors.
    eps_eff_cache: dict[int, tuple[float, float]] = {}
    # MMSQ multi-metal-stack handling: the C's
    # ``cmd_mmsquare_build_geometry`` emits flipped copies of the
    # base spiral on each lower metal, connected via vias at the
    # turn boundaries. All copies are at the same net voltage. For
    # the substrate cap, the dominant contribution comes from the
    # BOTTOMMOST metal (lowest h-to-ground); upper metals' caps to
    # substrate are short-circuited via the via stack into the
    # bottom metal's cap path. So restrict the per-polygon walk to
    # the bottom metal's polygons.
    polys_for_cap = list(shape.polygons)
    if shape.kind in ("multi_metal_square", "mmsquare", "capacitor"):
        # MMSQ and CAPACITOR stack multiple metal copies at the same
        # net voltage (via stitching vias or terminal connections).
        # Only the bottom-most metal sees the substrate directly; the
        # upper metal copies are shielded. Restrict the cap walk to the
        # bottom metal's polygons to avoid double-counting.
        valid_metals = [
            p.metal for p in shape.polygons
            if 0 <= p.metal < len(tech.metals)
        ]
        if valid_metals:
            bottom = min(valid_metals)
            polys_for_cap = [
                p for p in shape.polygons if p.metal == bottom
            ]

    for poly in polys_for_cap:
        verts = poly.vertices
        if len(verts) < 2:
            continue
        metal = poly.metal
        if metal < 0 or metal >= len(tech.metals):
            continue
        if metal in eps_eff_cache:
            eps_eff, h_total = eps_eff_cache[metal]
        else:
            m = tech.metals[metal]
            if m.layer >= len(tech.layers):
                continue
            own_layer = tech.layers[m.layer]
            # Walk downward from metal's layer to find the first
            # conductor layer. Insulator layers between contribute
            # their thickness; the conductor terminates the path.
            ground_idx: int | None = None
            for j in range(m.layer - 1, -1, -1):
                if tech.layers[j].rho <= GROUND_RHO:
                    ground_idx = j
                    break
            insulator_below = (
                tech.layers[ground_idx + 1 : m.layer]
                if ground_idx is not None else tech.layers[: m.layer]
            )
            own_below = max(0.0, m.d)  # path through own layer
            h_total = sum(layer.t for layer in insulator_below) + own_below
            if h_total <= 0:
                # Metal sits in (or directly on) a conductor — the
                # parallel-plate cap collapses; the Sommerfeld Green's
                # function model would dominate here. We fall back to
                # the legacy stack-average to avoid divide-by-zero.
                below = tech.layers[: m.layer]
                h_total = sum(layer.t for layer in below) + max(0.0, m.d)
                inv_eps_total = (
                    sum((layer.t / layer.eps) for layer in below if layer.eps > 0)
                    + max(0.0, m.d) / max(own_layer.eps, 1.0)
                )
                eps_eff = h_total / inv_eps_total if inv_eps_total > 0 else 1.0
            else:
                # Series cap through insulators only.
                inv_eps = sum(
                    (layer.t / layer.eps)
                    for layer in insulator_below if layer.eps > 0
                ) + own_below / max(own_layer.eps, 1.0)
                eps_eff = h_total / inv_eps if inv_eps > 0 else 1.0
            eps_eff_cache[metal] = (eps_eff, h_total)
        C_per_area = parallel_plate_cap_per_area(eps_eff, h_total)
        first, last = verts[0], verts[-1]
        closed = (
            abs(first.x - last.x) < 1e-9
            and abs(first.y - last.y) < 1e-9
            and abs(first.z - last.z) < 1e-9
        )
        if closed and len(verts) >= 4:
            A_um2 = abs(_polygon_signed_area(verts))
            per_um = _polygon_perimeter(verts)
        else:
            W = poly.width
            edge_sum = 0.0
            for i in range(len(verts) - 1):
                edge_sum += verts[i].distance_to(verts[i + 1])
            A_um2 = edge_sum * W
            per_um = 2.0 * edge_sum
        total_C += C_per_area * A_um2 * (UM_TO_M**2)
        total_C += 0.5 * EPS_0 * eps_eff * per_um * UM_TO_M
    return total_C


def shape_shunt_admittance(
    shape: Shape, tech: Tech, freq_ghz: float
) -> complex:
    """Total shunt admittance from ``shape`` to ground.

    Mirrors the per-substrate-layer complex-admittance loop the C
    binary runs at the head of ``analyze_capacitance_driver``
    (``asitic_kernel.c:0x08052c50``)::

        for each substrate layer k:
            σ_k = 1 / ρ_k
            ωε_k = ω · ε_0 · ε_r_k
            store 1/(σ_k + j·ωε_k) in the per-layer table

    The C then uses these complex resistivities inside a layered
    Sommerfeld Green's function. Without porting the full Green's
    function we use the Yue–Wong equivalent-circuit topology that
    matches the C's *qualitative* substrate behaviour:

        metal ━┳━━ jωC_ox ━━┳━━ G_sub ━━ ground
              port                substrate
                                  node

    i.e. the metal couples capacitively through the oxide stack
    (``C_ox`` from :func:`shape_shunt_capacitance`) to a "substrate
    node," and that node drains to ground via the substrate-loss
    conductance ``G_sub``. The two pathways are in **series**:

    .. math::

        Y(\\omega) = \\frac{j\\omega C_\\text{ox} \\cdot G_\\text{sub}}
                          {G_\\text{sub} + j\\omega C_\\text{ox}}

    This is the canonical CMOS-on-substrate equivalent circuit
    (Yue 1998 eq. 7-8) and is what the C binary's per-layer
    series-admittance chain reduces to in the single-dominant-layer
    limit. The previous parallel-shunt model ``Y = G + jωC`` gave
    25× too much loss for CMOS-class substrates because it
    bypassed the oxide-cap impedance.

    ``G_sub`` is built from the dominant lossy substrate layer
    using a spreading/skin-effect regime:

    * **Spreading regime** (``δ_skin ≥ L_char``): currents fan out
      laterally through the substrate. Yue's flat-disk spreading
      resistance for a circular contact of radius
      ``r_eq = √(A/π)``:

      .. math::

          G_\\text{sub} = 4 \\sigma r_\\text{eq}    \\quad (\\text{Yue eq. 8})

    * **Skin-effect regime** (``δ_skin < L_char``): the substrate
      acts as a thin lossy sheet of surface resistance ``R_s``.
      Image currents in the sheet pick up eddy-current loss
      ``R_eddy = R_s · L_metal / W_avg``.

    Asymptotically the series model collapses to the right limits:

    * ``G_sub → ∞`` (PEC ground):    ``Y → jωC_ox``  (lossless cap)
    * ``G_sub → 0`` (infinite ρ): ``Y → 0``        (open)
    * ``G_sub ≪ ωC_ox`` (high freq): ``Y → G_sub`` (loss-limited)
    """
    if freq_ghz <= 0 or not tech.layers or not shape.polygons:
        return 0j
    omega = 2.0 * math.pi * freq_ghz * 1.0e9  # rad/s
    C_total = shape_shunt_capacitance(shape, tech)  # F

    # Locate the substrate layer that dominates the loss path: the
    # thickest *non-insulator* layer below the metal's layer. The C
    # binary uses every substrate layer's complex permittivity
    # ``1 / (σ + jωε)`` in the Green's function (see
    # ``analyze_capacitance_driver`` setup at asitic_kernel.c:1799);
    # without the spatial integral we collapse this to a single
    # equivalent layer — the dominant lossy bulk. Insulators (oxide,
    # ρ ≳ 1 MΩ·cm) carry no real loss and are skipped. Heavily-doped
    # caps (BiCMOS p+, ρ ≈ 0.1 Ω·cm) are *thin* and so lose out to
    # the deeper p− layer by the thickest-finite-ρ test.
    INSULATOR_RHO = 1.0e6  # Ω·cm; layers above this are treated as lossless
    metal_idx = -1
    for poly in shape.polygons:
        if 0 <= poly.metal < len(tech.metals):
            metal_idx = poly.metal
            break
    if metal_idx < 0:
        return 1j * omega * C_total
    m = tech.metals[metal_idx]
    loss_layer = None
    for k in range(min(m.layer, len(tech.layers))):
        layer = tech.layers[k]
        if layer.rho <= 0 or layer.rho >= INSULATOR_RHO:
            continue
        if loss_layer is None or layer.t > loss_layer.t:
            loss_layer = layer
    if loss_layer is None:
        return 1j * omega * C_total

    # Substrate conductivity in S/m. ``layer.rho`` is in Ω·cm.
    sigma = 100.0 / loss_layer.rho if loss_layer.rho > 0 else 0.0
    if sigma <= 0:
        return 1j * omega * C_total

    # Skin depth in the substrate at this frequency.
    delta_skin_m = math.sqrt(2.0 / (omega * MU_0 * sigma))
    # Substrate thickness in metres.
    t_sub_m = loss_layer.t * UM_TO_M

    # Walk the ribbons once collecting both the footprint area and
    # the total centerline length: the loss model branches on whether
    # the substrate is in the spreading regime (low σ, lateral
    # current flow) or the skin regime (high σ, eddy currents in a
    # thin sheet beneath the trace).
    A_total_um2 = 0.0
    L_total_um = 0.0
    LW_sum_um2 = 0.0  # sum of L × W (= area, but tracked separately
                     # so we can recover an average width below)
    for poly in shape.polygons:
        verts = poly.vertices
        if len(verts) < 2:
            continue
        first = verts[0]
        last = verts[-1]
        closed = (
            len(verts) >= 4
            and abs(first.x - last.x) < 1e-9
            and abs(first.y - last.y) < 1e-9
            and abs(first.z - last.z) < 1e-9
        )
        if closed:
            A_um2 = abs(_polygon_signed_area(verts))
            # For a chamfered ribbon, the long-axis centerline length
            # is ``area / width``.
            if poly.width > 0:
                L_total_um += A_um2 / poly.width
            LW_sum_um2 += A_um2
        else:
            edge_sum = sum(
                verts[i].distance_to(verts[i + 1])
                for i in range(len(verts) - 1)
            )
            A_um2 = edge_sum * poly.width
            L_total_um += edge_sum
            LW_sum_um2 += A_um2
        A_total_um2 += A_um2
    if A_total_um2 <= 0:
        return 1j * omega * C_total
    A_total_m2 = A_total_um2 * (UM_TO_M**2)
    # Effective footprint area for the substrate-spreading radius.
    # For a spiral coil the substrate "sees" the metal traces directly
    # (depth ~ outer-radius, comparable to turn pitch), so A_total
    # gives the right effective radius. For a single-loop ring, the
    # ring has a hole much larger than the conductor width; the
    # substrate-spread current still emanates from the OUTER ring
    # extent, so A_bbox is closer to the right physics.
    xmin, ymin, xmax, ymax = shape.bounding_box()
    A_bbox_um2 = max((xmax - xmin) * (ymax - ymin), A_total_um2)
    if shape.kind == "ring":
        A_spread_m2 = A_bbox_um2 * (UM_TO_M**2)
    else:
        A_spread_m2 = A_total_m2
    L_char_m = math.sqrt(A_spread_m2)
    if t_sub_m <= 0:
        return 1j * omega * C_total

    # Two regimes for the substrate-loss conductance:
    #
    #   • **Spreading regime** (δ_skin ≥ L_char): currents fan out
    #     laterally through the substrate before being attenuated.
    #     Use Yue's flat-disk spreading-resistance result for a
    #     circular contact of radius ``r_eq = √(A_bbox/π)`` on a
    #     semi-infinite half-space of conductivity σ:
    #         G = 4 σ r_eq           (Yue 1998 eq. 8)
    #     The bounding-box A here matches the geometry the substrate
    #     "sees" (outer extent), giving the right spreading G for
    #     rings as well as spirals.
    #
    #   • **Skin-effect regime** (δ_skin < L_char): the substrate
    #     acts as a thin lossy sheet of surface resistance R_s.
    #     Image currents in the sheet pick up loss proportional to
    #         R_eddy = R_s · L_metal / W_avg
    #     where ``L_metal`` is the total centerline length and
    #     ``W_avg = A / L_metal`` is the average ribbon width. This
    #     is the Yue/Wong eddy-current model for a spiral on a
    #     low-resistivity (CMOS-class) substrate; magnitudes are
    #     within ~5–10× of gold but ratios across geometry are
    #     captured correctly.
    if delta_skin_m >= L_char_m:
        if shape.kind == "wire" and L_total_um > 0 and LW_sum_um2 > 0:
            # Long thin wire: use the line-source spreading
            # resistance for a strip of length L and width W on a
            # semi-infinite half-space (Schneider/Kuester):
            #     R_sub ≈ ln(2L/W) / (π · σ · L)
            # which translates to G_sub = π σ L / ln(2L/W). For
            # L ≫ W this gives a much smaller G than the disk-based
            # ``4σr_eq`` formula and matches the C's wire output
            # within ~15 %.
            L_m = L_total_um * UM_TO_M
            W_avg_m = (LW_sum_um2 / L_total_um) * UM_TO_M
            if W_avg_m > 0 and L_m > 2.0 * W_avg_m:
                G_sub = math.pi * sigma * L_m / math.log(2.0 * L_m / W_avg_m)
            else:
                r_eq_m = math.sqrt(A_spread_m2 / math.pi)
                G_sub = 4.0 * sigma * r_eq_m
        else:
            r_eq_m = math.sqrt(A_spread_m2 / math.pi)
            G_sub = 4.0 * sigma * r_eq_m
    else:
        R_s = math.sqrt(omega * MU_0 / (2.0 * sigma))  # Ω / □
        if L_total_um <= 0 or R_s <= 0:
            return 1j * omega * C_total
        W_avg_m = (LW_sum_um2 / L_total_um) * UM_TO_M
        L_metal_m = L_total_um * UM_TO_M
        if W_avg_m <= 0:
            return 1j * omega * C_total
        R_eddy = R_s * L_metal_m / W_avg_m
        G_sub = 1.0 / R_eddy if R_eddy > 0 else 0.0
    if G_sub <= 0 or C_total <= 0:
        # Degenerate: no loss path or no cap path → return whichever
        # is nonzero standalone.
        return (1j * omega * C_total) if C_total > 0 else complex(G_sub, 0.0)
    # Series combination of oxide-cap and substrate-loss path.
    # Y = (jωC · G) / (G + jωC) — Yue-Wong equivalent circuit.
    Yc = 1j * omega * C_total
    return (Yc * G_sub) / (G_sub + Yc)
