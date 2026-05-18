"""DC resistance per polygon / shape.

Per-segment formula:

.. math::

    R = \\rho_\\text{sh} \\cdot \\frac{L}{W}

where ``ρ_sh`` is the metal layer's sheet resistance (Ω/sq), ``L`` is
the segment length and ``W`` its width. The total DC resistance of
a shape is the sum over its segments. Vias contribute the
per-via-cell resistance from the tech file.

This mirrors the simplest path through ``compute_dc_resistance_per_polygon``
(``asitic_kernel.c:267``) — that decompiled function additionally
splits the sum into "primary" / "secondary" buckets at a tap point and
computes microstrip capacitances; the bare resistance summation is
all we need for the standard ``Res`` REPL command.
"""

from __future__ import annotations

from reasitic.geometry import Segment, Shape
from reasitic.tech import Tech


def segment_dc_resistance(segment: Segment, tech: Tech) -> float:
    """Return the DC resistance of one segment in Ω.

    Resolves the metal layer from ``tech`` to read its sheet
    resistance. Zero-length or zero-width segments contribute 0.
    """
    if segment.length <= 0 or segment.width <= 0:
        return 0.0
    if segment.metal < 0 or segment.metal >= len(tech.metals):
        # Out-of-range metal index: treat as zero contribution rather
        # than silently using rsh from a wrong layer.
        return 0.0
    rsh = tech.metals[segment.metal].rsh
    return rsh * segment.length / segment.width


def compute_dc_resistance(shape: Shape, tech: Tech) -> float:
    """Return the total DC resistance of ``shape`` in Ω.

    Mirrors ``cmd_resis_compute`` (``asitic_repl.c:0x0804eedc``) — the
    DC branch of ``compute_inductance_inner_kernel`` (decomp
    ``0x0804d1e4``) sums ``rsh · length / width`` per ribbon record.
    The function walks one record per ribbon (matching the C linked
    list at ``shape + 0xa8``); for filled chamfered ribbons (closed
    polygons stored verbatim from ``layout_polygons``) the endpoint
    distance equals the average centerline length, so we derive that
    from the shoelace area divided by width to stay consistent with
    :func:`reasitic.info.metal_area`.

    Via filaments (``poly.metal >= len(tech.metals)``) get the C's
    per-cluster treatment from
    ``compute_inductance_inner_kernel:0x0804d1e4``::

        if iVar2 >= g_num_metal_layers:
            return R_per_via / (cells_x * cells_y)

    i.e. the per-via resistance divided by the cell-array size. In
    the Python layout each via cell is a separate polygon, so the
    cluster's parallel combination is ``R_per_via / N_cells``.
    Vias of the same via index are grouped together — every via
    cluster contributes one ``R_per_via / N`` term.
    """
    total = 0.0
    n_metals = len(tech.metals)
    via_counts: dict[int, int] = {}
    # Build a pad-pair lookup so we can detect via-contact pads
    # (paired across two metal layers at the same xy). Standalone
    # W×W polygons (e.g. spiral end-caps in symsq) keep contributing.
    pad_centroids: dict[tuple[float, float, int], int] = {}
    for poly in shape.polygons:
        if poly.metal < 0 or poly.metal >= n_metals:
            continue
        verts = poly.vertices
        if len(verts) != 5 or poly.width <= 0:
            continue
        corners = verts[:-1]
        xs = [v.x for v in corners]
        ys = [v.y for v in corners]
        if (
            abs(max(xs) - min(xs) - poly.width) < 1e-6
            and abs(max(ys) - min(ys) - poly.width) < 1e-6
        ):
            cx = round((max(xs) + min(xs)) * 0.5, 4)
            cy = round((max(ys) + min(ys)) * 0.5, 4)
            key = (cx, cy, poly.metal)
            pad_centroids[key] = pad_centroids.get(key, 0) + 1
    paired_pads: set[tuple[float, float, int]] = set()
    for (cx, cy, m), _ in pad_centroids.items():
        for m2 in range(n_metals):
            if m2 != m and (cx, cy, m2) in pad_centroids:
                paired_pads.add((cx, cy, m))
                break

    for poly in shape.polygons:
        verts = poly.vertices
        if len(verts) < 2:
            continue
        metal = poly.metal
        if metal < 0:
            continue
        if metal >= n_metals:
            via_idx = metal - n_metals
            via_counts[via_idx] = via_counts.get(via_idx, 0) + 1
            continue
        rsh = tech.metals[metal].rsh
        if poly.width <= 0:
            continue
        first, last = verts[0], verts[-1]
        closed = (
            abs(first.x - last.x) < 1e-9
            and abs(first.y - last.y) < 1e-9
            and abs(first.z - last.z) < 1e-9
        )
        # Skip paired W×W via-contact pads only. Standalone W×W
        # polygons (spiral end-caps) still contribute.
        if closed and len(verts) == 5:
            corners = verts[:-1]
            xs = [v.x for v in corners]
            ys = [v.y for v in corners]
            W = poly.width
            if (
                abs(max(xs) - min(xs) - W) < 1e-6
                and abs(max(ys) - min(ys) - W) < 1e-6
            ):
                cx = round((max(xs) + min(xs)) * 0.5, 4)
                cy = round((max(ys) + min(ys)) * 0.5, 4)
                if (cx, cy, metal) in paired_pads:
                    continue
        if closed and len(verts) >= 4:
            # Centerline length = area / width (for a trapezoidal ribbon).
            area = 0.0
            for i in range(len(verts) - 1):
                v_i = verts[i]
                v_j = verts[i + 1]
                area += v_i.x * v_j.y - v_j.x * v_i.y
            L_eff = abs(area) * 0.5 / poly.width
            total += rsh * L_eff / poly.width
        else:
            for i in range(len(verts) - 1):
                L = verts[i].distance_to(verts[i + 1])
                if L > 1e-10:
                    total += rsh * L / poly.width
    # Sum the per-cluster via resistance (R_via / N_cells in parallel).
    for via_idx, n_cells in via_counts.items():
        if 0 <= via_idx < len(tech.vias) and n_cells > 0:
            total += tech.vias[via_idx].r / n_cells
    return float(total)
