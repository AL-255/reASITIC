"""Geometry-information commands: metal area, segment listing,
partial-inductance matrix dump.

These mirror ``cmd_metalarea_print`` (``asitic_repl.c:0x0804ee74``),
``cmd_listsegs`` (case 210), and ``cmd_lrmat_dump`` (case 531). The
binary's outputs are textual; we return Python objects and format
them as text in :func:`format_*` helpers.
"""

from __future__ import annotations

from io import StringIO
from typing import Any

import numpy as np

from reasitic.geometry import Shape
from reasitic.inductance.filament import build_inductance_matrix, filament_grid
from reasitic.substrate.shunt import _polygon_signed_area


def metal_area(shape: Shape) -> float:
    """Total xy-projected metal area of ``shape`` in μm².

    Mirrors ``cmd_metalarea_print`` (``asitic_repl.c:0x0804ee74``):
    walks every polygon in the shape and accumulates the per-ribbon
    metal footprint. The C binary stores one polygon record per
    ribbon segment with both a centerline-endpoint pair and a
    trapezoidal 4-corner outline, and reports ``length × width``
    per record.

    Python's Shape stores some kinds as centerline polylines
    (square / polygon spirals, ring, wire, capacitor plates after
    the centerline rewrite) and others as already-filled chamfered
    trapezoid polygons (transformer / balun / symmetric / mmsquare,
    which are produced through ``layout_polygons`` to match the CIF
    exactly). The two representations are distinguished here:

    * **Closed** polygons (first vertex == last vertex) are filled
      trapezoidal ribbons — use the shoelace area directly.
    * **Open** polylines (≥ 2 vertices, first ≠ last) are centerline
      paths — sum ``edge_length × polygon.width`` over consecutive
      vertex pairs.
    """
    total = 0.0
    for poly in shape.polygons:
        verts = poly.vertices
        if len(verts) < 2:
            continue
        first = verts[0]
        last = verts[-1]
        closed = (
            abs(first.x - last.x) < 1e-9
            and abs(first.y - last.y) < 1e-9
            and abs(first.z - last.z) < 1e-9
        )
        if closed and len(verts) >= 4:
            # Shoelace area of the filled outline (trapezoid for a
            # chamfered ribbon segment).
            area = 0.0
            for i in range(len(verts) - 1):
                v_i = verts[i]
                v_j = verts[i + 1]
                area += v_i.x * v_j.y - v_j.x * v_i.y
            total += 0.5 * abs(area)
        else:
            # Open polyline: sum each edge length × polygon width.
            W = poly.width
            for i in range(len(verts) - 1):
                L = verts[i].distance_to(verts[i + 1])
                if L > 1e-10:
                    total += L * W
    return float(total)


def list_segments(shape: Shape) -> list[dict[str, Any]]:
    """Return one dict per segment with position, length, and width."""
    out: list[dict[str, Any]] = []
    for i, s in enumerate(shape.segments()):
        out.append(
            {
                "index": i,
                "x_a": s.a.x,
                "y_a": s.a.y,
                "z_a": s.a.z,
                "x_b": s.b.x,
                "y_b": s.b.y,
                "z_b": s.b.z,
                "length": s.length,
                "width": s.width,
                "thickness": s.thickness,
                "metal": s.metal,
            }
        )
    return out


def format_segments(shape: Shape) -> str:
    """Render :func:`list_segments` as the binary's text format."""
    out = StringIO()
    out.write(f"Shape <{shape.name}> has {len(shape.segments())} segments:\n")
    out.write(
        f"{'#':>3} {'metal':>5} {'L_um':>10} {'a':>30} {'b':>30}\n"
    )
    for s in list_segments(shape):
        a = f"({s['x_a']:.2f},{s['y_a']:.2f},{s['z_a']:.2f})"
        b = f"({s['x_b']:.2f},{s['y_b']:.2f},{s['z_b']:.2f})"
        out.write(
            f"{s['index']:>3} {s['metal']:>5} {s['length']:>10.3f} {a:>30} {b:>30}\n"
        )
    return out.getvalue()


def lr_matrix(shape: Shape) -> np.ndarray:
    """Per-segment partial inductance matrix in nH.

    Diagonal = self-L of each segment; off-diagonal = signed Grover
    parallel-segment mutual. Mirrors the binary's ``LRMAT`` dump
    (case 531).
    """
    fils = []
    for idx, s in enumerate(shape.segments()):
        for f in filament_grid(s, n_w=1, n_t=1):
            f.parent_segment = idx
            fils.append(f)
    return build_inductance_matrix(fils)


def format_lr_matrix(shape: Shape) -> str:
    """Render the partial-L matrix as a text table."""
    M = lr_matrix(shape)
    n = M.shape[0]
    out = StringIO()
    out.write(f"# Partial-inductance matrix (nH) for shape <{shape.name}>: {n}x{n}\n")
    for i in range(n):
        row = " ".join(f"{M[i, j]:10.6f}" for j in range(n))
        out.write(row + "\n")
    return out.getvalue()
