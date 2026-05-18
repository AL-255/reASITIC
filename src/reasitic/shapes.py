"""Shape builder facade for ASITIC primitives.

This module provides class-based wrappers (Wire, SquareSpiral, ...)
on top of the verified vertex-for-vertex geometry kernel at
:mod:`reasitic._geometry`.  The geometry kernel produces polygon
vertices that match the ASITIC binary's CIF/GDS emit byte-for-byte;
each ``Shape`` class also carries its own ``dc_resistance`` walker
that mirrors the binary's ``cmd_resis_compute`` chain (including
the via-cluster grouping and parallelogram long-edge CL recovered
from the qemu ListSegs dump).

The numerical analysis (inductance, capacitance) still flows through
the Cython-wrapped C math kernels in :mod:`reasitic.kernel`.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable, List, Tuple

from . import _geometry, kernel
from .tech import Metal, Tech


Vertex = Tuple[float, float]
Polygon = Tuple[Vertex, ...]


# ---------------------------------------------------------------------
# Base class
# ---------------------------------------------------------------------

@dataclass
class _ShapeBase:
    """Common shape facade.

    Concrete subclasses populate ``_shape`` (a
    :class:`reasitic._geometry.Shape`) in ``__post_init__``; everything
    else here derives from that.
    """

    name: str
    metal: str
    x_origin: float = 0.0
    y_origin: float = 0.0
    tech: Tech | None = None
    _shape: _geometry.Shape | None = field(default=None, init=False, repr=False)

    @property
    def shape(self) -> _geometry.Shape:
        if self._shape is None:
            raise RuntimeError("shape was not initialised")
        return self._shape

    def _metal_polygons(self) -> list:
        """Walk the CIF-presentation polygons, skipping vias and
        paired W×W via-contact pads.

        The shape's internal ``polygons`` list mixes centerline
        polylines (one connected polygon per spiral) with closed
        chamfered ribbons.  The validation goldens record polygon
        counts and lengths against the *binary's CIF emit* -- i.e.
        the trapezoidal ribbon-per-side layout produced by
        :func:`reasitic._geometry.layout_polygons`.  Walking that
        layout (and dropping vias + paired pads) reproduces the
        binary's ``listsegs`` set.
        """
        if self.tech is None:
            return [p for p in self.shape.polygons if p.width > 0]
        polys = _geometry.layout_polygons(self.shape, self.tech)
        n_m = len(self.tech.metals)
        # Paired-pad filter is only meaningful when via clusters are
        # present: the via-contact pads pair the top/bottom metal at
        # the via cluster's centroid, and ``listsegs`` counts the pair
        # once.  For shapes without vias (e.g. capacitors with two
        # large coincident plates), each plate is a real conductor
        # and must be counted independently.
        has_vias = any(p.metal >= n_m for p in polys)
        if not has_vias:
            return [p for p in polys
                    if 0 <= p.metal < n_m and p.width > 0]

        # Detect "thin" coincident metal-polygon pairs (same xy
        # footprint on two metals, one axis ≤ W) -- those are
        # always via-coupled pads that listsegs counts once.  We
        # exclude beefier coincident pairs (e.g. SYMSQ centre-tap
        # parallelograms with both axes > W) because the binary
        # keeps them as two distinct segments.
        footprints: dict[tuple, set[int]] = {}
        def _full_footprint(p):
            verts = p.vertices
            xs = [v.x for v in verts]
            ys = [v.y for v in verts]
            cx = round((max(xs) + min(xs)) * 0.5, 4)
            cy = round((max(ys) + min(ys)) * 0.5, 4)
            dx = round(max(xs) - min(xs), 4)
            dy = round(max(ys) - min(ys), 4)
            return (cx, cy, dx, dy)
        for p in polys:
            if not (0 <= p.metal < n_m) or p.width <= 0:
                continue
            verts = p.vertices
            xs = [v.x for v in verts]
            ys = [v.y for v in verts]
            dx = max(xs) - min(xs)
            dy = max(ys) - min(ys)
            if min(dx, dy) > p.width * 1.1 + 1e-6:
                continue
            key = _full_footprint(p)
            footprints.setdefault(key, set()).add(p.metal)
        coincident_pairs = {
            k for k, m_set in footprints.items() if len(m_set) > 1
        }
        # Mirror the binary's listsegs collapsing: a W×W via-overlap
        # pad pair (two polys with the same xy centroid on different
        # metals, both with footprint ≈ width × width) is counted
        # once.  Longer paired polygons (exit leads that happen to
        # share a centroid) are kept as separate segments because
        # the binary's listsegs walks the per-metal trace network.
        # Collect via cluster centroids so we can tell apart "pads
        # at a via cluster" (collapsed by listsegs) from "mid-spiral
        # crossovers" (kept as two segments).
        via_centroids: list[tuple[float, float, float]] = []
        for p in polys:
            if p.metal < n_m:
                continue
            verts = p.vertices
            xs = [v.x for v in verts]
            ys = [v.y for v in verts]
            cx = (max(xs) + min(xs)) * 0.5
            cy = (max(ys) + min(ys)) * 0.5
            half = max(max(xs) - min(xs), max(ys) - min(ys)) * 0.5
            via_centroids.append((cx, cy, half))

        def _is_pad(p) -> tuple | None:
            """Return ``(cx, cy)`` if ``p`` is a via-overlap pad.

            A via-overlap pad satisfies both:
              * a via cell sits inside its bounding box,
              * BOTH axes are ≤ ~W (so the polygon is a roughly
                square tip pad, not a long lead extending past the
                via cluster).

            This distinguishes pads (collapsed by ``listsegs``) from
            access leads (which also cross the via cluster but
            extend out toward the chip edge).
            """
            verts = p.vertices
            if len(verts) < 2:
                return None
            xs = [v.x for v in verts]
            ys = [v.y for v in verts]
            dx = max(xs) - min(xs)
            dy = max(ys) - min(ys)
            W = p.width
            # BOTH axes must be ≤ W (with chamfer overhang).  This
            # rejects long access-lead rectangles whose narrow axis
            # is W but whose long axis ≫ W.
            if max(dx, dy) > W * 1.5 + 1e-6:
                return None
            x0, x1 = min(xs), max(xs)
            y0, y1 = min(ys), max(ys)
            for vx, vy, _ in via_centroids:
                if x0 - 1e-6 <= vx <= x1 + 1e-6 and y0 - 1e-6 <= vy <= y1 + 1e-6:
                    cx = (max(xs) + min(xs)) * 0.5
                    cy = (max(ys) + min(ys)) * 0.5
                    return (round(cx, 4), round(cy, 4))
            return None

        pad_centroids: dict[tuple, set[int]] = {}
        for p in polys:
            if not (0 <= p.metal < n_m) or p.width <= 0:
                continue
            key = _is_pad(p)
            if key is not None:
                pad_centroids.setdefault(key, set()).add(p.metal)
        paired_pads = {
            key for key, m_set in pad_centroids.items() if len(m_set) > 1
        }

        out: list = []
        consumed: set[tuple] = set()
        for p in polys:
            if not (0 <= p.metal < n_m) or p.width <= 0:
                continue
            full_key = _full_footprint(p)
            is_coincident_pair = full_key in coincident_pairs
            pad_key = _is_pad(p)
            if is_coincident_pair:
                # Coincident metals at the same xy footprint are
                # always paired by listsegs (collapsed to one).
                if full_key in consumed:
                    continue
                consumed.add(full_key)
                p.is_via_overlap_pad = True  # type: ignore[attr-defined]
            # When the shape includes via clusters, a clean rectangle
            # whose long axis is longer than 2·W and that is NOT
            # part of a coincident pair is an access lead with a
            # chamfered tail at its outer end.  Tag it for
            # ``_binary_centerline_length`` so metal_area accounting
            # matches the binary's stored CL = L − W/2.
            if has_vias and not is_coincident_pair and pad_key is None:
                corners = p.vertices[:-1] if len(p.vertices) == 5 else list(p.vertices)
                if _is_clean_rectangle(corners, p.width):
                    xs = [v.x for v in corners]
                    ys = [v.y for v in corners]
                    long_dim = max(max(xs) - min(xs), max(ys) - min(ys))
                    if long_dim > 2.0 * p.width + 1e-6:
                        p.is_access_lead = True  # type: ignore[attr-defined]
            out.append(p)
        return out

    @property
    def polygons(self) -> List[Polygon]:
        """The metal-polygon vertex set (microns, 2D, ASITIC-CIF order)."""
        out: List[Polygon] = []
        for p in self._metal_polygons():
            verts = p.vertices
            if (
                len(verts) > 1
                and abs(verts[0].x - verts[-1].x) < 1e-9
                and abs(verts[0].y - verts[-1].y) < 1e-9
            ):
                # Drop the closing duplicate vertex emitted by ASITIC.
                verts = verts[:-1]
            out.append(tuple((v.x, v.y) for v in verts))
        return out

    @property
    def n_segments(self) -> int:
        """Count straight conductor pieces, mirroring the binary's
        ``cmd_listsegs`` output.

        Each closed (chamfered-ribbon) polygon collapses to one
        centerline segment; open polylines contribute ``len(vertices)
        - 1`` segments (one per consecutive vertex pair).  This matches
        the validation goldens' ``n_segments`` field.
        """
        return sum(len(p.edges()) for p in self._metal_polygons())

    @property
    def total_length_um(self) -> float:
        """Sum of conductor lengths over all metal polygons.

        Mirrors the binary's ``cmd_metalarea_print`` (see
        ``recomp/asitic_repl_shapes.c:556``): walks every polygon
        record and uses its stored centerline endpoints to compute
        ``len = |v0 - v1|``.  Pads stored with ``v0 == v1`` (via-
        overlap pads) contribute zero; clean-rectangle access leads
        are stored with one chamfered tail (CL = long_dim − W/2).
        """
        total = 0.0
        for poly in self._metal_polygons():
            total += _binary_centerline_length(poly)
        return total

    @property
    def total_area_um2(self) -> float:
        """Total metal footprint in μm²; mirrors ``cmd_metalarea_print``."""
        total = 0.0
        for poly in self._metal_polygons():
            total += _binary_centerline_length(poly) * poly.width
        return total

    @property
    def location(self) -> Tuple[float, float]:
        return (self.shape.x_origin, self.shape.y_origin)

    def _dc_centerline_length(self, poly) -> float:
        """Per-polygon centerline length the binary uses for R.

        Matches ``_binary_centerline_length`` for everything except
        tilted parallelogram crossovers (SYMPOLY centre-tap
        diagonals), which use the polygon's *long edge length*
        instead of the bbox diagonal.  This decoupling lets us hit
        <1 % geometry (where bbox-diagonal matches metal_area for
        SYMSQ-style axis-aligned parallelograms) while keeping
        SYMPOLY DC accurate (where the bbox diagonal would
        over-count).
        """
        if poly.width <= 0:
            return 0.0
        verts = poly.vertices
        if len(verts) < 4:
            return _binary_centerline_length(poly, count_via_pads=True)
        closed = (
            abs(verts[0].x - verts[-1].x) < 1e-9
            and abs(verts[0].y - verts[-1].y) < 1e-9
        )
        if not closed:
            return _binary_centerline_length(poly, count_via_pads=True)
        corners = verts[:-1] if len(verts) == 5 else list(verts)
        if len(corners) != 4:
            return _binary_centerline_length(poly, count_via_pads=True)
        # Check for parallelogram.
        d01 = corners[0].distance_to(corners[1])
        d12 = corners[1].distance_to(corners[2])
        d23 = corners[2].distance_to(corners[3])
        d30 = corners[3].distance_to(corners[0])
        is_parallelogram = (
            abs(d01 - d23) < 1e-6 and abs(d12 - d30) < 1e-6
        )
        if not is_parallelogram:
            return _binary_centerline_length(poly, count_via_pads=True)
        # Clean axis-aligned rectangles (Wire, Cap plate, simple
        # spiral end-cap pads) fall back to ``_binary_centerline_length``.
        if _is_clean_rectangle(corners, poly.width):
            return _binary_centerline_length(poly, count_via_pads=True)
        short_edge = min(d01, d12)
        long_edge = max(d01, d12)
        # SYMSQ-style axis-aligned parallelogram crossover
        # (short edge = W exactly): bbox-diagonal CL (same as the
        # geometry path -- the binary appears to use this length
        # consistently for axis-aligned chamfered crossovers).
        if abs(short_edge - poly.width) < 1e-6:
            return _binary_centerline_length(poly, count_via_pads=True)
        # SYMPOLY-style tilted parallelogram: short edge slightly
        # wider than W (chamfered polygon-spiral connector).  The
        # binary stores its long-edge length here.
        return long_edge

    def dc_resistance(self, tech: Tech | None = None) -> float:
        """Total DC resistance in Ω, mirroring the binary's
        ``cmd_resis_compute`` (asitic_repl.c:0x0804eedc):

            R = Σ rsh × L_seg / W       over every metal polygon
              + Σ R_via / N_cells        over every via cluster

        Walks the filtered metal-polygon set (the same one used by
        ``total_length_um``).

        Pad CL convention: terminal exit pads (SQ/TRANS) store
        ``v0 == v1`` in the binary's chain and contribute zero to
        both metal_area and DC.  Centre-tap pads (SYMSQ / SYMPOLY)
        store finite CL and contribute to DC but the binary's
        ``cmd_metalarea_print`` ``if (len > 1e-10)`` gate still
        zeros them in metal_area.

        We distinguish the two by whether the shape contains any
        ``is_access_lead`` polygon: terminal-pad shapes always do
        (the access lead lives next to the exit pad), centre-tap
        shapes never do.
        """
        t = tech or self.tech
        if t is None:
            raise ValueError("dc_resistance needs a Tech; pass it or set self.tech")
        polys = self._metal_polygons()
        total = 0.0
        n_metals = len(t.metals)
        for poly in polys:
            if poly.width <= 0:
                continue
            # Via-overlap pads are CIF-emit artifacts that aren't in
            # the binary's polygon chain (verified via ListSegs on
            # SYMSQ + SYMPOLY).  Always skip them for R.
            if getattr(poly, "is_via_overlap_pad", False):
                continue
            L = self._dc_centerline_length(poly)
            if L <= 0:
                continue
            rsh = t.metals[poly.metal].rsh
            total += rsh * L / poly.width
        # Group via cells into clusters by spatial proximity.  The
        # binary's polygon chain stores ONE record per via cluster
        # (not one per cell); each record's
        # ``compute_inductance_inner_kernel`` call returns
        # ``r_via / N_cells_in_cluster``.  Summing per cluster gives
        # 2/9 + 2/9 = 0.444 ohm for SYMSQ L=150's two 3×3 clusters
        # (vs 2/18 = 0.111 ohm if we lumped everything as one),
        # which is what closes the SYMSQ DC gap to <1 %.
        via_cells: dict[int, list[tuple[float, float]]] = {}
        for poly in self.shape.polygons:
            if poly.metal < n_metals:
                continue
            via_idx = poly.metal - n_metals
            xs = [v.x for v in poly.vertices]
            ys = [v.y for v in poly.vertices]
            cx = (max(xs) + min(xs)) * 0.5
            cy = (max(ys) + min(ys)) * 0.5
            via_cells.setdefault(via_idx, []).append((cx, cy))
        for via_idx, centroids in via_cells.items():
            if not (0 <= via_idx < len(t.vias)) or not centroids:
                continue
            via_rec = t.vias[via_idx]
            pitch = via_rec.width + via_rec.space
            threshold = pitch * 1.5 + 1e-6
            # Greedy cluster: walk cells, group with any prior
            # cluster whose centre-distance is within ``threshold``.
            clusters: list[list[tuple[float, float]]] = []
            for c in centroids:
                placed = False
                for cl in clusters:
                    if any((c[0]-x)**2 + (c[1]-y)**2 < threshold**2
                           for (x, y) in cl):
                        cl.append(c)
                        placed = True
                        break
                if not placed:
                    clusters.append([c])
            for cl in clusters:
                total += via_rec.r / len(cl)
        return float(total)


def _binary_centerline_length(
    poly: _geometry.Polygon, *, count_via_pads: bool = False
) -> float:
    """Length the ASITIC binary records for ``poly`` in
    ``cmd_metalarea_print``.

    When ``count_via_pads`` is True, via-overlap pads contribute
    their full ``L_long`` length (the binary's R walker doesn't
    short-circuit them the way ``cmd_metalarea_print`` does); when
    False (the default), via-overlap pads contribute zero, matching
    the binary's metal-area output.

    The binary stores two centerline endpoints per polygon record
    (``face[0].v0`` / ``v1``) and reports ``|v0 − v1| × W`` as the
    metal footprint.  We reproduce that here:

    * **Via-overlap pads** (the W×W contact pads paired across two
      metals at a via cluster) are stored with ``v0 == v1`` so they
      contribute zero -- the binary's ``if (len > 1e-10)`` gate
      skips them.  We mark such pads in :meth:`_metal_polygons` and
      honour the mark here.
    * **Chamfered ribbons** (the spiral sides) store CL as the
      midpoints of their two short edges.  For a trapezoidal ribbon
      with parallel long edges ``L`` and ``L − ΔW`` the midpoint
      distance equals the shoelace area / W.
    * **Clean rectangles** (access leads) store CL with one chamfered
      tail at the outer end -- CL = long_dim − W/2.  This matches
      the binary's exact metal_area for SQ N=1.5 / N=2 / N=3 cases
      (residual < 0.2 %).
    """
    if getattr(poly, "is_via_overlap_pad", False) and not count_via_pads:
        return 0.0
    if poly.width <= 0:
        return 0.0
    verts = poly.vertices
    if len(verts) < 2:
        return 0.0
    closed = (
        len(verts) >= 4
        and abs(verts[0].x - verts[-1].x) < 1e-9
        and abs(verts[0].y - verts[-1].y) < 1e-9
        and abs(verts[0].z - verts[-1].z) < 1e-9
    )
    if closed and len(verts) >= 4:
        corners = verts[:-1] if len(verts) == 5 else verts
        xs = [v.x for v in corners]
        ys = [v.y for v in corners]
        dx = max(xs) - min(xs)
        dy = max(ys) - min(ys)
        # The chamfered-tail rule only applies to access leads
        # generated alongside via clusters; ``_metal_polygons``
        # tags those explicitly via ``is_access_lead``.  Plain
        # standalone rectangles (Wire, Capacitor plates) keep
        # their full ``L × W`` area.
        if getattr(poly, "is_access_lead", False):
            long_dim = max(dx, dy)
            return long_dim - poly.width * 0.5
        is_clean_rect = _is_clean_rectangle(corners, poly.width)
        if is_clean_rect:
            # Clean rectangle (Wire, Cap plate, standalone end-cap):
            # full bbox-long length.
            long_dim = max(dx, dy)
            return long_dim
        # Chamfered ribbon: choose the centerline metric the binary
        # stores for this shape kind.  For trapezoids (standard SQ
        # ribbon: long top edge + shorter chamfered bottom), the
        # binary's ``face[0].v0/v1`` midpoint distance equals
        # shoelace_area / W.  For parallelogram crossovers
        # (SYMSQ / SYMPOLY centre-tap diagonals), the binary stores
        # the *long-edge length* -- verified against ListSegs output
        # for SYMSQ L=150 (parallelogram segments 5 and 14 stored as
        # (146, 90)-(136, 75), length sqrt(100+225) = 18.03).
        if len(corners) == 4:
            d01 = corners[0].distance_to(corners[1])
            d12 = corners[1].distance_to(corners[2])
            d23 = corners[2].distance_to(corners[3])
            d30 = corners[3].distance_to(corners[0])
            if abs(d01 - d23) < 1e-6 and abs(d12 - d30) < 1e-6:
                return max(d01, d12)
        area = _polygon_signed_area(poly)
        return area / poly.width if area > 0 else 0.0
    # Open polyline: sum per-edge length.
    total = 0.0
    for i in range(len(verts) - 1):
        L = verts[i].distance_to(verts[i + 1])
        if L > 1e-10:
            total += L
    return total


def _is_clean_rectangle(corners, width: float) -> bool:
    """Detect a clean (non-chamfered) 4-corner rectangle."""
    if len(corners) != 4:
        return False
    xs = [c.x for c in corners]
    ys = [c.y for c in corners]
    dx = max(xs) - min(xs)
    dy = max(ys) - min(ys)
    # All four corners must sit on the bbox edges.
    for c in corners:
        on_xb = abs(c.x - min(xs)) < 1e-6 or abs(c.x - max(xs)) < 1e-6
        on_yb = abs(c.y - min(ys)) < 1e-6 or abs(c.y - max(ys)) < 1e-6
        if not (on_xb and on_yb):
            return False
    return True


def _polygon_signed_area(poly: _geometry.Polygon) -> float:
    if poly.width <= 0:
        return 0.0
    verts = poly.vertices
    if len(verts) < 3:
        return 0.0
    closed = (
        abs(verts[0].x - verts[-1].x) < 1e-9
        and abs(verts[0].y - verts[-1].y) < 1e-9
    )
    n = len(verts) - 1 if closed else len(verts)
    a = 0.0
    for i in range(n):
        x_i, y_i = verts[i].x, verts[i].y
        x_j, y_j = verts[(i + 1) % n].x, verts[(i + 1) % n].y
        a += x_i * y_j - x_j * y_i
    return abs(a) * 0.5


def _require_tech(tech: Tech | None, kind: str) -> Tech:
    if tech is None:
        raise ValueError(f"{kind} requires a 'tech' argument")
    return tech


# ---------------------------------------------------------------------
# Wire
# ---------------------------------------------------------------------

@dataclass
class Wire(_ShapeBase):
    """Straight metal wire of given ``length`` (μm) and ``width`` (μm).

    The single segment is laid out along +x starting at
    ``(x_origin, y_origin)`` on metal layer ``metal``.
    """

    length: float = 0.0
    width: float = 0.0

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "Wire")
        self._shape = _geometry.wire(
            self.name,
            length=self.length, width=self.width,
            metal=self.metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
        )


# ---------------------------------------------------------------------
# Capacitor
# ---------------------------------------------------------------------

@dataclass
class Capacitor(_ShapeBase):
    """Parallel-plate capacitor (top plate on ``metal``, bottom plate
    on ``metal_bottom``).

    Both plates are rectangles of size ``length × width`` (μm) with
    their lower-left corner at ``(x_origin, y_origin)``.
    """

    length: float = 0.0
    width: float = 0.0
    metal_bottom: str = ""

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "Capacitor")
        self._shape = _geometry.capacitor(
            self.name,
            length=self.length, width=self.width,
            metal_top=self.metal,
            metal_bottom=self.metal_bottom or self.metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
        )


# ---------------------------------------------------------------------
# SquareSpiral
# ---------------------------------------------------------------------

@dataclass
class SquareSpiral(_ShapeBase):
    """Square-spiral inductor (ASITIC ``Sq`` command).

    Parameters
    ----------
    length, width, spacing : float
        Outer side length, trace width, and inter-turn spacing (μm).
    turns : float
        Number of turns; may be fractional (the binary supports
        half-turn-resolution layouts).
    metal : str
        Layer name used for the spiral body.
    exit_metal : str, optional
        Layer name used for the underpass that returns the inner
        terminal to the exterior; defaults to the layer below
        ``metal``.
    include_access : bool, default True
        Include the access lead from the outermost vertex to the
        external port.
    """

    length: float = 0.0
    width: float = 0.0
    spacing: float = 0.0
    turns: float = 0.0
    exit_metal: str | None = None
    include_access: bool = True

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "SquareSpiral")
        self._shape = _geometry.square_spiral(
            self.name,
            length=self.length, width=self.width,
            spacing=self.spacing, turns=self.turns,
            metal=self.metal,
            exit_metal=self.exit_metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            include_access=self.include_access,
            tech=t,
        )


# ---------------------------------------------------------------------
# Ring
# ---------------------------------------------------------------------

@dataclass
class Ring(_ShapeBase):
    """Regular polygonal ring inductor (single turn).

    Parameters
    ----------
    radius, width : float
        Ring radius and trace width (μm).
    gap : float
        Angular gap (μm of arc length) breaking the ring open
        between the two terminals.
    sides : int, default 16
        Number of polygon edges approximating the circle.
    """

    radius: float = 0.0
    width: float = 0.0
    gap: float = 0.0
    sides: int = 16

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "Ring")
        self._shape = _geometry.ring(
            self.name,
            radius=self.radius, width=self.width,
            gap=self.gap, sides=self.sides,
            metal=self.metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
        )


# ---------------------------------------------------------------------
# PolygonSpiral
# ---------------------------------------------------------------------

@dataclass
class PolygonSpiral(_ShapeBase):
    """Regular polygonal spiral (ASITIC ``Spiral`` command).

    Same parameter semantics as :class:`SquareSpiral` but with an
    arbitrary number of polygon ``sides`` (8 = octagonal spiral,
    common in mm-wave designs).
    """

    radius: float = 0.0
    width: float = 0.0
    spacing: float = 0.0
    turns: float = 0.0
    sides: int = 8

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "PolygonSpiral")
        self._shape = _geometry.polygon_spiral(
            self.name,
            radius=self.radius, width=self.width,
            spacing=self.spacing, turns=self.turns,
            sides=self.sides,
            metal=self.metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
        )


# ---------------------------------------------------------------------
# MMSQ
# ---------------------------------------------------------------------

@dataclass
class MMSQ(_ShapeBase):
    """Multi-metal square spiral (ASITIC ``MMSQ`` command).

    A square spiral that stacks the same trace on every metal layer
    from ``metal`` down to ``exit_metal``, tied together with stitch
    vias.  Sheet resistance scales inversely with the number of
    layers, raising Q for thick-metal stacks.
    """

    length: float = 0.0
    width: float = 0.0
    spacing: float = 0.0
    turns: float = 0.0
    exit_metal: str = ""

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "MMSQ")
        self._shape = _geometry.multi_metal_square(
            self.name,
            length=self.length, width=self.width,
            spacing=self.spacing, turns=self.turns,
            metal=self.metal,
            exit_metal=self.exit_metal or self.metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
        )


# ---------------------------------------------------------------------
# SymmetricSquare
# ---------------------------------------------------------------------

@dataclass
class SymmetricSquare(_ShapeBase):
    """Symmetric (centre-tapped) square spiral (ASITIC ``Symm``
    command).

    Two interleaved spirals share a centre tap so the device looks
    identical from either external terminal.  ``ilen`` is the bridge
    length crossing under the tracks; ``metal2`` is the bridge layer.
    """

    length: float = 0.0
    width: float = 0.0
    spacing: float = 0.0
    turns: float = 0.0
    ilen: float = 0.0
    metal2: str = ""
    exit_metal: str | None = None

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "SymmetricSquare")
        self._shape = _geometry.symmetric_square(
            self.name,
            length=self.length, width=self.width,
            spacing=self.spacing, turns=self.turns,
            ilen=self.ilen,
            metal=self.metal,
            primary_metal=self.metal,
            exit_metal=self.exit_metal or self.metal2 or self.metal,
            bridge_metal=self.metal2 or self.metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
        )


# ---------------------------------------------------------------------
# SymmetricPolygon
# ---------------------------------------------------------------------

@dataclass
class SymmetricPolygon(_ShapeBase):
    """Symmetric centre-tapped polygonal spiral (the polygon analogue
    of :class:`SymmetricSquare`)."""

    radius: float = 0.0
    width: float = 0.0
    spacing: float = 0.0
    turns: float = 0.0
    sides: int = 8
    ilen: float = 0.0
    metal2: str = ""
    exit_metal: str | None = None

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "SymmetricPolygon")
        self._shape = _geometry.symmetric_polygon(
            self.name,
            radius=self.radius, width=self.width,
            spacing=self.spacing, turns=self.turns,
            sides=self.sides, ilen=self.ilen,
            metal=self.metal,
            primary_metal=self.metal,
            exit_metal=self.exit_metal or self.metal2 or self.metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
        )


# ---------------------------------------------------------------------
# Transformer
# ---------------------------------------------------------------------

@dataclass
class Transformer(_ShapeBase):
    """Stacked-spiral transformer (ASITIC ``Trans`` command).

    Two co-axial square spirals, one on ``metal`` (primary) and one
    on ``metal2`` (secondary).  Set ``port="primary"`` or
    ``port="secondary"`` to materialise one winding at a time.
    """

    length: float = 0.0
    width: float = 0.0
    spacing: float = 0.0
    turns: float = 0.0
    metal2: str = ""
    exit_metal: str | None = None
    port: str = "primary"

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "Transformer")
        self._shape = _geometry.transformer(
            self.name,
            length=self.length, width=self.width,
            spacing=self.spacing, turns=self.turns,
            metal=self.metal,
            exit_metal=self.exit_metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
            which=self.port,
        )
        # The validation goldens record each port's stored (x_origin,
        # y_origin) directly from the C binary:
        #   * primary :  XORG + (W+S),  YORG + (2W+S)   -- internal LL
        #   * secondary: XORG,           YORG            -- unshifted
        # The geometry kernel's secondary places the internal LL at
        # ``YORG + W`` (the inter-turn offset between primary and
        # secondary tracks); the binary's stored shape origin uses
        # the unshifted XORG/YORG instead.  Reconcile here so the
        # public ``location`` matches the gold's convention.
        if self.port == "secondary":
            self._shape.x_origin = self.x_origin
            self._shape.y_origin = self.y_origin


# ---------------------------------------------------------------------
# Balun
# ---------------------------------------------------------------------

@dataclass
class Balun(_ShapeBase):
    """Interleaved single-ended-to-differential balun (ASITIC ``Balun``
    command).

    Two co-planar (same-layer or stacked) spirals are interleaved so
    that the primary is referenced to ground at its centre tap while
    the secondary delivers a balanced differential signal.
    ``port`` selects which winding to materialise.
    """

    length: float = 0.0
    width: float = 0.0
    spacing: float = 0.0
    turns: float = 0.0
    metal2: str = ""
    exit_metal: str | None = None
    port: str = "primary"

    def __post_init__(self) -> None:
        t = _require_tech(self.tech, "Balun")
        self._shape = _geometry.balun(
            self.name,
            length=self.length, width=self.width,
            spacing=self.spacing, turns=self.turns,
            primary_metal=self.metal,
            secondary_metal=self.metal2 or self.metal,
            exit_metal=self.exit_metal,
            x_origin=self.x_origin, y_origin=self.y_origin,
            tech=t,
            which=self.port,
        )


# ---------------------------------------------------------------------
# Self-inductance helpers built on the wrapped C kernels
# ---------------------------------------------------------------------

def grover_total_self_inductance(shape: _ShapeBase, tech: Tech) -> float:
    """Sum-of-segment Grover self-inductance for a shape, in nH.

    Each segment's length is taken from the polygon's centerline
    (vertex-to-vertex distance for open polylines, area/width for
    closed chamfered ribbons) and fed to the wrapped C
    ``grover_segment_self_inductance`` kernel with
    ``r_eff = 0.2235 * (W + t)`` for rectangular conductors.
    """
    t_metal = 0.0
    for poly in shape.shape.polygons:
        if 0 <= poly.metal < len(tech.metals):
            t_metal = tech.metals[poly.metal].t
            break
    total_nH = 0.0
    for poly in shape.shape.polygons:
        if poly.width <= 0 or poly.metal < 0 or poly.metal >= len(tech.metals):
            continue
        verts = poly.vertices
        if len(verts) < 2:
            continue
        closed = (
            len(verts) >= 4
            and abs(verts[0].x - verts[-1].x) < 1e-9
            and abs(verts[0].y - verts[-1].y) < 1e-9
        )
        r_eff = 0.2235 * (poly.width + t_metal)
        if closed:
            area = _polygon_signed_area(poly)
            if area <= 0:
                continue
            l = area / poly.width
            total_nH += kernel.grover_segment_self_inductance(l, r_eff)
        else:
            for i in range(len(verts) - 1):
                l = verts[i].distance_to(verts[i + 1])
                if l > 1e-10:
                    total_nH += kernel.grover_segment_self_inductance(l, r_eff)
    return total_nH * 2e-4  # μm·cgs -> nH
