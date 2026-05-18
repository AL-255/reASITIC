"""FFT-accelerated convolution of the substrate Green's function.

Mirrors the binary's full FFT pipeline:

* :func:`compute_green_function`     ↔ ``compute_green_function`` (decomp ``0x0808c350``)
* :func:`fft_apply_to_green`         ↔ ``fft_apply_to_green`` (``0x080912c0``)
* :func:`setup_green_fft_grid`       ↔ ``fft_setup`` (``0x08091548``)
* :func:`rasterize_shape`            — used by ``analyze_capacitance_polygon``
  to map a shape footprint onto the FFT charge grid
* :func:`substrate_cap_matrix`       ↔ ``analyze_capacitance_driver``
  end-to-end pipeline

The static substrate Green's function ``G(ρ, z₁, z₂)`` only depends
on the lateral *separation* ``(Δx, Δy)`` between source and field
points. With the spatial-domain Green's tabulated on an
``(Nₓ, Nᵧ)`` grid, applying it to an arbitrary charge distribution
becomes a 2-D convolution, which the FFT performs in
``O(Nₓ Nᵧ log(Nₓ Nᵧ))``. For an ``M``-shape capacitance-matrix
extraction this gives ``O(M · Nₓ Nᵧ log(Nₓ Nᵧ))`` versus
``O(M² · N_pairs)`` for the per-pair Sommerfeld integration in
:mod:`reasitic.substrate.green`.

The implementation builds the Green's function directly in k-space
(spatial-frequency domain), where the layered-stack reflection
coefficient ``R_eff(k_ρ)`` has its natural definition. Linear
convolution is achieved by zero-padding the charge distribution to
``(2Nₓ, 2Nᵧ)`` so wraparound aliasing is suppressed. This matches
the binary's ``fft_apply_to_green`` which operates on the same
zero-padded layout (the literal `nx*2 * ny*2 * 16` allocation in
the decompilation gives it away).
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
import scipy.fft

from reasitic.geometry import Shape
from reasitic.substrate.green import (
    _path_eff_eps_r,
    _stack_reflection_coefficient,
    layered_reflection_complex,
)
from reasitic.tech import Tech
from reasitic.units import EPS_0, UM_TO_M


@dataclass
class GreenFFTGrid:
    """Pre-computed FFT-domain Green's function for one ``(z₁, z₂)`` pair.

    All arrays are zero-padded to ``(2 N_x, 2 N_y)`` so the
    convolution implemented by :func:`fft_apply_to_green` is linear
    rather than circular.
    """

    nx: int
    ny: int
    chip_x_um: float
    chip_y_um: float
    z1_um: float
    z2_um: float
    g_grid: np.ndarray         # (Nx, Ny) spatial-domain G, real-space cells (μm²)
    g_fft: np.ndarray          # (2Nx, 2Ny) zero-padded FFT of g_grid


def _g_at_k(
    k_rho: float,
    z1_m: float,
    z2_m: float,
    R_eff: float,
) -> float:
    """Static-limit Green's function in k-space for the layered substrate.

    For a charge above a stratified ground plane the spatial-spectral
    Green's function is::

        G̃(k_ρ) = (1 / (2ε₀ k_ρ)) · (e^{-k_ρ |z₁−z₂|}
                                      + R_eff(k_ρ) · e^{-k_ρ (z₁+z₂)})

    The ``1/k_ρ`` factor is regularised at the DC mode by the caller.
    """
    if k_rho <= 0:
        return 0.0
    decay_d = math.exp(-k_rho * abs(z1_m - z2_m))
    decay_s = math.exp(-k_rho * (z1_m + z2_m))
    return (decay_d + R_eff * decay_s) / (2.0 * EPS_0 * k_rho)


def setup_green_fft_grid(
    tech: Tech,
    *,
    z1_um: float,
    z2_um: float,
    nx: int | None = None,
    ny: int | None = None,
    chip_x_um: float | None = None,
    chip_y_um: float | None = None,
    omega_rad: float = 0.0,
) -> GreenFFTGrid:
    """Pre-compute the substrate Green's function on a 2-D FFT grid.

    Mirrors the binary's ``fft_setup`` (decomp ``0x08091548``).

    The grid uses the ``chipx`` / ``chipy`` extents and ``fftx`` /
    ``ffty`` resolution from the tech file by default; pass overrides
    for any of those to deviate. ``nx`` / ``ny`` should be powers of
    2 for the fastest FFT, but any positive integers are accepted.

    The resulting :class:`GreenFFTGrid` can be passed to
    :func:`fft_apply_to_green` for fast batched evaluation.
    """
    if nx is None:
        nx = tech.chip.fftx if tech.chip.fftx > 0 else 64
    if ny is None:
        ny = tech.chip.ffty if tech.chip.ffty > 0 else 64
    if chip_x_um is None:
        chip_x_um = tech.chip.chipx if tech.chip.chipx > 0 else 512.0
    if chip_y_um is None:
        chip_y_um = tech.chip.chipy if tech.chip.chipy > 0 else 512.0
    if nx <= 0 or ny <= 0:
        raise ValueError("nx and ny must be positive")

    dx = chip_x_um / nx
    dy = chip_y_um / ny
    z1_m = z1_um * UM_TO_M
    z2_m = z2_um * UM_TO_M

    # Zero-padded grid for linear convolution
    Nx = 2 * nx
    Ny = 2 * ny

    # k-space coordinates over the padded grid: k_n = 2π n / (N · dx)
    kx = 2.0 * math.pi * scipy.fft.fftfreq(Nx, d=dx * UM_TO_M)
    ky = 2.0 * math.pi * scipy.fft.fftfreq(Ny, d=dy * UM_TO_M)
    KX, KY = np.meshgrid(kx, ky, indexing="ij")
    K_RHO = np.sqrt(KX * KX + KY * KY)

    # Build G̃(k_ρ) by sampling the 1-D function above per cell.
    # Vectorise the reflection coefficient by collecting unique k_ρ
    # values on a logarithmic grid and interpolating onto the 2-D grid.
    k_rho_flat = K_RHO.flatten()
    finite = k_rho_flat[k_rho_flat > 0]
    if finite.size == 0:
        # Pathological all-DC grid
        g_fft_padded = np.zeros((Nx, Ny), dtype=complex)
        g_grid = np.zeros((nx, ny))
        return GreenFFTGrid(
            nx=nx, ny=ny,
            chip_x_um=chip_x_um, chip_y_um=chip_y_um,
            z1_um=z1_um, z2_um=z2_um,
            g_grid=g_grid, g_fft=g_fft_padded,
        )

    # Substrate reflection coefficient. Per the C's
    # ``green_function_kernel_a/_b`` (asitic_kernel.c:0x0808cc90) the
    # ground plane is the topmost ρ ≤ 1 Ω·cm substrate layer below
    # the metal stack — the C builds the layered transfer matrix
    # against that conductor. For the static-limit port we use
    # R = -1 (PEC image reflection) and absorb the insulator stack's
    # series ε_r into the prefactor 1/(2k·ε₀·ε_eff). This matches the
    # spatial-domain image-method form used in
    # :func:`reasitic.substrate.green.green_function_static`.
    #
    # At ``omega_rad > 0`` the reflection becomes complex via
    # :func:`layered_reflection_complex` (single-layer Fresnel with
    # the topmost conductor's complex γ), giving an Im{G_k} that
    # carries the substrate-loss contribution. Im{C_matrix} from
    # the inverted ``P`` then yields ``G_substrate / ω`` for the
    # Pi-network reduction (§7 in TODO.md).
    eps_eff = _path_eff_eps_r(tech)
    if omega_rad > 0:
        # Sample R at unique k_ρ values to amortise the per-cell
        # complex-sqrt cost. Use the grid's actual k_ρ values
        # rounded to a finite log-spaced set.
        k_flat = K_RHO[K_RHO > 0]
        k_unique = np.unique(k_flat)
        R_lookup = np.array([
            layered_reflection_complex(tech, float(k), omega_rad)
            for k in k_unique
        ], dtype=complex)
        R_grid = np.zeros(K_RHO.shape, dtype=complex)
        for k_val, R_val in zip(k_unique, R_lookup, strict=True):
            R_grid[K_RHO == k_val] = R_val
    else:
        R_grid = np.full(K_RHO.shape, -1.0 + 0j)

    decay_d = np.exp(-K_RHO * abs(z1_m - z2_m))
    decay_s = np.exp(-K_RHO * (z1_m + z2_m))
    with np.errstate(divide="ignore", invalid="ignore"):
        G_k = np.where(
            K_RHO > 0,
            (decay_d + R_grid * decay_s) / (
                2.0 * EPS_0 * eps_eff * np.maximum(K_RHO, 1e-30)
            ),
            0.0,
        )
    G_k = G_k.astype(complex)

    # Spatial-domain Green's: inverse FFT on the padded grid, take
    # the unique (Nx, Ny) corner. ifftshift first because the
    # spatial axis we want is centred at 0. When ``omega_rad > 0``
    # the kernel is complex (substrate loss), so we keep complex
    # output here; callers can take the real part if they only
    # need the lossless cap contribution.
    g_padded = scipy.fft.ifft2(G_k)
    if omega_rad > 0:
        g_centered: np.ndarray = scipy.fft.fftshift(g_padded)
    else:
        g_centered = scipy.fft.fftshift(np.real(g_padded))
    cx = Nx // 2
    cy = Ny // 2
    g_grid = g_centered[cx - nx // 2: cx - nx // 2 + nx,
                        cy - ny // 2: cy - ny // 2 + ny].copy()

    return GreenFFTGrid(
        nx=nx, ny=ny,
        chip_x_um=chip_x_um, chip_y_um=chip_y_um,
        z1_um=z1_um, z2_um=z2_um,
        g_grid=g_grid, g_fft=G_k,
    )


def compute_green_function(
    tech: Tech,
    *,
    z1_um: float,
    z2_um: float,
    nx: int | None = None,
    ny: int | None = None,
) -> GreenFFTGrid:
    """Public binary-equivalent entry point for ``compute_green_function``.

    Mirrors the binary's top-level Green's-function builder
    (``decomp/output/asitic_kernel.c:9203``, address ``0x0808c350``).
    Convenience alias of :func:`setup_green_fft_grid` so the API
    matches the C symbol name 1:1.
    """
    return setup_green_fft_grid(tech, z1_um=z1_um, z2_um=z2_um, nx=nx, ny=ny)


def green_apply(grid: GreenFFTGrid, charge: np.ndarray) -> np.ndarray:
    """Backward-compatible alias for :func:`fft_apply_to_green`."""
    return fft_apply_to_green(grid, charge)


def fft_apply_to_green(
    grid: GreenFFTGrid,
    charge: np.ndarray,
) -> np.ndarray:
    """Convolve a charge grid with the precomputed Green's function.

    Mirrors ``fft_apply_to_green`` (decomp ``0x080912c0``). The
    charge grid is zero-padded to ``(2 N_x, 2 N_y)`` before
    multiplication so the convolution stays linear (no wraparound).

    Args:
        grid:    A :class:`GreenFFTGrid` from :func:`setup_green_fft_grid`.
        charge:  An ``(N_x, N_y)`` real array (Coulombs / cell). Must
                 match ``grid.nx`` × ``grid.ny`` exactly.

    Returns:
        The potential ``V`` in Volts, shape ``(N_x, N_y)``.
    """
    if charge.shape != (grid.nx, grid.ny):
        raise ValueError(
            f"charge shape {charge.shape} does not match "
            f"({grid.nx}, {grid.ny})"
        )
    Nx = 2 * grid.nx
    Ny = 2 * grid.ny
    padded = np.zeros((Nx, Ny), dtype=complex if np.iscomplexobj(charge) else float)
    padded[: grid.nx, : grid.ny] = charge
    Q_fft = scipy.fft.fft2(padded)
    V_fft = Q_fft * grid.g_fft
    V_padded = scipy.fft.ifft2(V_fft)
    if np.iscomplexobj(grid.g_grid):
        return np.asarray(V_padded[: grid.nx, : grid.ny])
    return np.asarray(V_padded[: grid.nx, : grid.ny].real)


# ----- Shape rasterisation -----------------------------------------------


def rasterize_shape(
    shape: Shape,
    *,
    nx: int,
    ny: int,
    chip_x_um: float,
    chip_y_um: float,
) -> np.ndarray:
    """Mark grid cells covered by ``shape``'s polygon footprint.

    Returns a boolean ``(N_x, N_y)`` mask. Used by the
    capacitance-matrix pipeline to turn a list of shapes into the
    charge grid that drives :func:`fft_apply_to_green`. Mirrors the
    binary's ``analyze_capacitance_polygon`` rasterisation step.

    Each grid cell is treated as covered if its centre lies inside
    *any* polygon of the shape. Open (line) polygons are ignored.
    """
    if nx <= 0 or ny <= 0:
        raise ValueError("nx and ny must be positive")
    dx = chip_x_um / nx
    dy = chip_y_um / ny
    mask = np.zeros((nx, ny), dtype=bool)
    if not shape.polygons:
        return mask

    # Build cell-centre coordinates
    xs = (np.arange(nx) + 0.5) * dx
    ys = (np.arange(ny) + 0.5) * dy

    for poly in shape.polygons:
        verts = poly.vertices
        if len(verts) < 2:
            continue
        if len(verts) < 3:
            # 2-vertex centerline ribbon (e.g. capacitor plate, wire).
            # Expand it back to a 4-corner rectangle using poly.width
            # so the point-in-polygon test below has a polygon to chase.
            # Vertices are already in absolute (world) coords from the
            # shape builder's ``to_world`` step, so we do NOT add
            # ``shape.x_origin / y_origin`` again.
            a, b = verts[0], verts[1]
            edx = b.x - a.x
            edy = b.y - a.y
            L = math.hypot(edx, edy)
            if L <= 1e-12:
                continue
            nx_v = -edy / L * poly.width * 0.5
            ny_v = edx / L * poly.width * 0.5
            xs_p = [
                a.x + nx_v, b.x + nx_v, b.x - nx_v, a.x - nx_v,
            ]
            ys_p = [
                a.y + ny_v, b.y + ny_v, b.y - ny_v, a.y - ny_v,
            ]
        else:
            xs_p = [v.x for v in verts]
            ys_p = [v.y for v in verts]
        # Fast bbox cull
        xmin, xmax = min(xs_p), max(xs_p)
        ymin, ymax = min(ys_p), max(ys_p)
        ix_lo = max(0, math.floor(xmin / dx))
        ix_hi = min(nx - 1, math.ceil(xmax / dx))
        iy_lo = max(0, math.floor(ymin / dy))
        iy_hi = min(ny - 1, math.ceil(ymax / dy))
        if ix_lo > ix_hi or iy_lo > iy_hi:
            continue
        # Test each cell centre against the polygon
        for ix in range(ix_lo, ix_hi + 1):
            for iy in range(iy_lo, iy_hi + 1):
                if _point_in_polygon(xs[ix], ys[iy], xs_p, ys_p):
                    mask[ix, iy] = True
    return mask


def _point_in_polygon(
    px: float, py: float,
    xs: list[float], ys: list[float],
) -> bool:
    """Ray-casting point-in-polygon test for a closed loop."""
    n = len(xs)
    if n < 3:
        return False
    inside = False
    j = n - 1
    for i in range(n):
        if ((ys[i] > py) != (ys[j] > py)) and (
            px < (xs[j] - xs[i]) * (py - ys[i]) / (ys[j] - ys[i] + 1e-30) + xs[i]
        ):
            inside = not inside
        j = i
    return inside


# ----- End-to-end capacitance-matrix pipeline ---------------------------


def substrate_cap_matrix(
    shapes: list[Shape] | dict[str, Shape],
    tech: Tech,
    *,
    z1_um: float | None = None,
    z2_um: float | None = None,
    nx: int = 64,
    ny: int = 64,
) -> np.ndarray:
    """End-to-end FFT-accelerated substrate capacitance matrix.

    Mirrors the binary's ``analyze_capacitance_driver`` (decomp
    ``0x08052c50``). For ``M`` shapes returns an ``(M, M)`` symmetric
    capacitance matrix in **Farads**:

    1. Rasterise each shape onto the ``(N_x, N_y)`` grid.
    2. Compute the Green's function on the same grid.
    3. For each shape ``i``, place uniform unit-charge on its
       footprint, propagate via :func:`fft_apply_to_green` to get the
       potential everywhere, and integrate over each shape ``j`` to
       form the potential matrix ``P_ij``.
    4. Invert ``P`` to get the cap matrix ``C = P⁻¹``.

    The pipeline is symmetrised at the end to absorb numerical noise.
    """
    items_list: list[Shape] = (
        list(shapes.values()) if isinstance(shapes, dict) else list(shapes)
    )
    n_shapes = len(items_list)
    if n_shapes == 0:
        return np.zeros((0, 0))

    # Pick z heights: use the topmost metal of the shape's metals if
    # the caller didn't override.
    if z1_um is None:
        # Use the average metal-layer z of all shapes
        zs: list[float] = []
        for sh in items_list:
            for poly in sh.polygons:
                m = tech.metals[poly.metal] if 0 <= poly.metal < len(tech.metals) else None
                if m is not None:
                    zs.append(m.d + 0.5 * m.t)
        z1_um = sum(zs) / max(len(zs), 1) if zs else 1.0
    if z2_um is None:
        z2_um = z1_um

    chip_x = tech.chip.chipx if tech.chip.chipx > 0 else 1024.0
    chip_y = tech.chip.chipy if tech.chip.chipy > 0 else 1024.0
    grid = setup_green_fft_grid(
        tech, z1_um=z1_um, z2_um=z2_um, nx=nx, ny=ny,
        chip_x_um=chip_x, chip_y_um=chip_y,
    )

    # Rasterise every shape and compute its area
    masks: list[np.ndarray] = []
    cell_area_m2 = (chip_x * UM_TO_M / nx) * (chip_y * UM_TO_M / ny)
    for sh in items_list:
        mask = rasterize_shape(
            sh, nx=nx, ny=ny, chip_x_um=chip_x, chip_y_um=chip_y,
        )
        masks.append(mask)

    # Build P_ij: place unit charge on shape i, integrate V over shape j
    P = np.zeros((n_shapes, n_shapes))
    for i, mask_i in enumerate(masks):
        n_cells_i = int(mask_i.sum())
        if n_cells_i == 0:
            P[i, i] = 1.0  # singular row; will be dropped by inversion
            continue
        # Unit total charge spread uniformly over shape i
        Q = np.zeros((nx, ny))
        Q[mask_i] = 1.0 / (n_cells_i * cell_area_m2)
        V = fft_apply_to_green(grid, Q)
        for j, mask_j in enumerate(masks):
            n_cells_j = int(mask_j.sum())
            if n_cells_j == 0:
                continue
            P[i, j] = float(V[mask_j].mean())

    # Symmetrise (numerical noise can break exact reciprocity)
    P = 0.5 * (P + P.T)
    # Invert P → cap matrix; guard against singular blocks
    try:
        C = np.linalg.inv(P)
    except np.linalg.LinAlgError:
        C = np.linalg.pinv(P)
    return np.asarray(0.5 * (C + C.T))
