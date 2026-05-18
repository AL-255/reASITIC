"""Layered substrate Green's function — Niknejad-Gharpurey-Meyer formulation.

Python port of the spectral kernel at ``asitic_kernel.c:0x0808cc90``
(``green_function_kernel_a``) plus its helpers. Refactored from the
x87-laden disassembly into readable C in
``../../../decomp_readable/`` and then translated line-by-line here.

Implements the math from:

    Niknejad, Gharpurey, Meyer (1998), "Numerically Stable Green
    Function for Modeling and Analysis of Substrate Coupling in
    Integrated Circuits", IEEE TCAD 17(4), 305–315.
    https://ieeexplore.ieee.org/document/703820

Specifically the numerically stable continued-fraction form from
the paper's Eqs. 40-41 and 47, evaluated against the multilayer
geometry of paper §III (Fig. 1).

**Equation-to-code mapping** (see also ``decomp_readable/MATH.md``):

* Eq. 6 (complex conductivity ``σ_c = σ + jωε``) → :func:`build_layer_stack`
* Eq. 21 (β, Γ recurrence) → :func:`green_kernel_shared_helper_a` /
  :func:`green_kernel_shared_helper_b` (the C uses the equivalent
  ``r_k`` numerator/denominator form from Eqs. 40-41)
* Eq. 23 (Green's function double sum) → :func:`compute_green_function`
* Eqs. 33-34 (DCT acceleration) → :func:`fft_setup_inplace`
* Eq. 47 (numerically stable F_k^u form) → :func:`green_function_kernel_a`
  (3 tanh boundary factors + cosh-product chain)

**Status**: structural port — the algorithmic skeleton mirrors the
paper's equations and the C's disassembly, but value-for-value
verification against gold awaits the full pipeline wire-up
(``capacitance_segment_integral`` + ``solve_node_equations``).
Currently not used by ``spiral_y_at_freq``; sits alongside the
empirical series-Yue substrate model in
:mod:`reasitic.substrate.green` for head-to-head comparison.

Decoding of the x87 sequences in the original C disassembly is
documented in ``decomp_readable/README.md``. The key decoding rules:

* ``f2xm1 / fscale`` sequence with ``-ABS`` prefactor → ``tanh(arg)``
* same sequence without ``-ABS`` → ``cosh(arg)`` (with overflow
  guard at ``|arg| > 500``)
* ``f2xm1 / fscale`` followed by ``(1/u + u) * 0.5`` → ``cosh(arg)``
"""

from __future__ import annotations

import cmath
import math
from dataclasses import dataclass

import numpy as np
import scipy.fft

from reasitic.tech import Tech
from reasitic.units import EPS_0, UM_TO_M


@dataclass
class LayerStack:
    """Decoded substrate layer geometry + complex permittivity table.

    Mirrors the binary's globals ``g_capacitance_options`` and
    ``g_substrate_layer_table``. Verified against
    ``asitic_repl.c:7600-7619`` (the layer-thickness accumulator
    in ``cmd_load_tech_apply``) and ``:19858-19872`` (the
    ``g_capacitance_options`` initialisation).

    Convention (matches the C binary exactly):

    * Substrate layers are indexed in tek-file order: index ``0`` is
      the DEEPEST layer, index ``N-1`` is the topmost insulator.
    * ``z_boundaries[i]`` for ``i < N`` is the **depth from chip
      surface to the bottom of layer i**, in metres. For BiCMOS
      ``[layer 0 = p−, layer 1 = p+, layer 2 = oxide]`` this gives
      ``[852μm, 52μm, 50μm]`` — i.e. the values DECREASE with index.
    * ``z_boundaries[N] = 0`` is the sentinel (= depth at chip top).
    * ``eps_complex[i]`` is ``ε₀·ε_r,i − jσ_i/ω`` for layer i in the
      same order. (At ``ω = 0`` σ term vanishes.)
    """

    z_boundaries: np.ndarray
    eps_complex: np.ndarray


def build_layer_stack(tech: Tech, omega_rad: float) -> LayerStack:
    """Set up the ``LayerStack`` for a given tech file and ω.

    Mirrors:

    * :func:`analyze_capacitance_driver` per-layer
      ``1 / (σ + jωε)`` table (asitic_kernel.c:1799-1820).
    * ``cmd_load_tech_apply`` z-accumulator
      (asitic_repl.c:7600-7619) where each layer's thickness
      increments the depth value of itself AND all deeper-indexed
      layers — yielding ``z[i] = sum_{j≥i} t_j``.
    * ``g_capacitance_options`` init (asitic_repl.c:19858-19872)
      which copies the depth values 1:1 from the substrate-height
      table and appends a ``0`` sentinel at index ``N``.
    """
    layers = tech.layers
    if not layers:
        return LayerStack(
            z_boundaries=np.array([0.0]),
            eps_complex=np.array([], dtype=complex),
        )
    n = len(layers)
    # z_boundaries[i] = depth from chip top to bottom of layer i, in
    # tek-file order (deepest first). z_boundaries[N] = 0 sentinel.
    # Layer i's bottom is at depth `sum_{j >= i} t_j` since deeper
    # layers sit below shallower ones in tek-file ordering.
    boundaries = np.zeros(n + 1)
    for i in range(n):
        boundaries[i] = sum(
            layers[j].t * UM_TO_M for j in range(i, n)
        )
    boundaries[n] = 0.0

    eps_list: list[complex] = []
    for layer in layers:
        sigma_S_per_m = 100.0 / layer.rho if layer.rho > 0 else 0.0
        eps_r = layer.eps if layer.eps > 0 else 1.0
        if omega_rad > 0:
            eps_c = EPS_0 * eps_r - 1j * sigma_S_per_m / omega_rad
        else:
            eps_c = EPS_0 * eps_r + 0j
        eps_list.append(eps_c)
    return LayerStack(
        z_boundaries=boundaries,
        eps_complex=np.asarray(eps_list, dtype=complex),
    )


def metal_z_from_top(tech: Tech, metal_idx: int) -> float:
    """Compute the metal's depth from chip surface, in metres.

    Mirrors the C binary's metal-z calculation in
    ``cmd_load_tech_apply`` (asitic_repl.c:19830-19838)::

        metal.z = subheight.z[metal.layer] - metal.d

    For BiCMOS m3 (layer=2 oxide, d=5μm) this gives
    ``50μm − 5μm = 45μm`` (i.e. m3 sits 5μm above the bottom of the
    oxide layer, hence at depth 45μm from chip top).
    """
    if not (0 <= metal_idx < len(tech.metals)):
        return 0.0
    m = tech.metals[metal_idx]
    layer = m.layer
    if not (0 <= layer < len(tech.layers)):
        return 0.0
    # subheight.z[layer] = depth to bottom of that layer
    subheight_z = sum(
        tech.layers[j].t * UM_TO_M for j in range(layer, len(tech.layers))
    )
    return subheight_z - m.d * UM_TO_M


# ---- Helpers (mirror decomp_readable/green_kernel_helpers.c) ----


def green_kernel_shared_helper_a(
    stack: LayerStack,
    layer_lo: int,
    layer_hi: int,
    k_rho: float,
) -> complex:
    """Downward accumulator across substrate layers.

    Mirrors ``green_kernel_shared_helper_a`` at
    ``asitic_kernel.c:0x0808f80c``, verified line-by-line against
    the FPU-stack disassembly (lines 11163-11247).

    The C maintains two complex state variables:

    * ``A`` — accumulator (initial value ``T_init + 0j`` from the
      pre-loop tanh on the source layer's own boundary).
    * ``B`` — running product (initial value ``1 + 0j``).

    Per iteration over ``n`` from ``layer_hi - 1`` down to
    ``layer_lo``::

        eps_ratio_n = ε_(n+1) / ε_n                (complex divide)
        T_n         = tanh(k_ρ · (g[n+1] - g[n]))   (from f2xm1)
        B_new       = eps_ratio_n · B               (running update)
        A_new       = A + T_n · B_new

    The pre-loop tanh uses the source layer's own bottom-to-top
    distance::

        T_init = tanh(k_ρ · (g[layer_hi+1] - g[layer_hi]))

    Returns the final ``A`` (the accumulator).
    """
    # Pre-loop initial tanh on source layer's bottom-to-top.
    if (
        layer_hi < 0
        or layer_hi + 1 >= len(stack.z_boundaries)
    ):
        T_init = 0.0
    else:
        T_init = math.tanh(
            k_rho * (stack.z_boundaries[layer_hi + 1]
                     - stack.z_boundaries[layer_hi])
        )
    A: complex = complex(T_init, 0.0)
    B: complex = complex(1.0, 0.0)

    # Walk down from layer_hi - 1 to layer_lo (inclusive).
    for n in range(layer_hi - 1, layer_lo - 1, -1):
        if n < 0 or n + 1 >= len(stack.z_boundaries):
            continue
        if n + 1 >= len(stack.eps_complex):
            continue
        eps_ratio = stack.eps_complex[n + 1] / stack.eps_complex[n]
        T_n = math.tanh(
            k_rho * (stack.z_boundaries[n + 1]
                     - stack.z_boundaries[n])
        )
        B = eps_ratio * B
        A = A + T_n * B
    return A


def green_kernel_shared_helper_b(
    stack: LayerStack,
    layer_idx: int,
    k_rho: float,
) -> complex:
    """Upward accumulator (sister of _shared_helper_a).

    Mirrors ``green_kernel_shared_helper_b`` at
    ``asitic_kernel.c:0x0808f004``, verified line-by-line against
    lines 10804-10905.

    The C computes ``T_top = tanh(k_ρ · (g[0] - g[1]))`` BEFORE
    checking ``if (0 < p2)``. The conditional just skips the loop
    body; the pre-loop tanh is always evaluated. For ``layer_idx
    < 1`` (including the ``-1`` sentinel from the main kernel) the
    function returns just ``T_top + 0j``.

    Loop over ``n`` from ``1`` up to ``layer_idx`` inclusive::

        eps_ratio_n = ε_(n-1) / ε_n                (complex divide)
        T_n         = tanh(k_ρ · (g[n] - g[n+1]))   (from f2xm1)
        B_new       = eps_ratio_n · B
        A_new       = A + T_n · B_new
    """
    if len(stack.z_boundaries) < 2:
        return 0.0 + 0j
    T_top = math.tanh(
        k_rho * (stack.z_boundaries[0] - stack.z_boundaries[1])
    )
    A: complex = complex(T_top, 0.0)
    if layer_idx < 1:
        return A
    B: complex = complex(1.0, 0.0)
    for n in range(1, layer_idx + 1):
        if n >= len(stack.eps_complex):
            break
        if n + 1 >= len(stack.z_boundaries):
            break
        eps_ratio = stack.eps_complex[n - 1] / stack.eps_complex[n]
        T_n = math.tanh(
            k_rho * (stack.z_boundaries[n]
                     - stack.z_boundaries[n + 1])
        )
        B = eps_ratio * B
        A = A + T_n * B
    return A


def green_kernel_a_helper(
    stack: LayerStack,
    src_layer: int,
    obs_layer: int,
    k_rho: float,
    z_obs: float,
) -> complex:
    """Propagation-weighted downward accumulator.

    Mirrors ``green_kernel_a_helper`` at
    ``asitic_kernel.c:0x0808fc04``. Same additive A/B-product
    structure as ``_shared_helper_a`` but with a per-step
    propagation factor ``exp(-k_ρ · |z_obs − z_layer_top|)``
    multiplying the running product B between iterations.

    Early-returns ``1 + 0j`` when ``src_layer == obs_layer``.
    """
    if src_layer == obs_layer:
        return 1.0 + 0j
    A: complex = 0.0 + 0j
    B: complex = 1.0 + 0j
    step = -1 if obs_layer < src_layer else +1
    n = src_layer
    safety = abs(obs_layer - src_layer) + 1
    while n != obs_layer and safety > 0:
        if n < 0 or n + 1 >= len(stack.z_boundaries):
            break
        if n + 1 >= len(stack.eps_complex):
            break
        z_layer_top = stack.z_boundaries[n + 1]
        T_n = math.tanh(
            k_rho * (z_layer_top - stack.z_boundaries[n])
        )
        eps_ratio = stack.eps_complex[n + 1] / stack.eps_complex[n]
        prop_arg = k_rho * abs(z_obs - z_layer_top)
        prop = math.exp(-prop_arg) if prop_arg < 500.0 else 0.0
        B = prop * eps_ratio * B
        A = A + T_n * B
        n += step
        safety -= 1
    return A


def green_kernel_b_helper(
    stack: LayerStack,
    src_layer: int,
    obs_layer: int,
    k_rho: float,
    z_obs: float,
) -> complex:
    """Below-source propagation-weighted accumulator.

    Mirrors ``green_kernel_b_helper`` at
    ``asitic_kernel.c:0x0808f3f0``. Same additive A/B-product
    structure as ``_a_helper`` but uses the layer's *bottom* z
    for the propagation factor and inverts the eps_ratio direction
    (``ε[n-1] / ε[n]``).
    """
    if src_layer == obs_layer:
        return 1.0 + 0j
    A: complex = 0.0 + 0j
    B: complex = 1.0 + 0j
    step = -1 if obs_layer < src_layer else +1
    n = src_layer
    safety = abs(obs_layer - src_layer) + 1
    while n != obs_layer and safety > 0:
        if n < 0 or n + 1 >= len(stack.z_boundaries):
            break
        if n < 1 or n >= len(stack.eps_complex):
            break
        z_layer_bot = stack.z_boundaries[n]
        t_layer = stack.z_boundaries[n + 1] - z_layer_bot
        T_n = math.tanh(k_rho * t_layer)
        eps_ratio = stack.eps_complex[n - 1] / stack.eps_complex[n]
        prop_arg = k_rho * abs(z_obs - z_layer_bot)
        prop = math.exp(-prop_arg) if prop_arg < 500.0 else 0.0
        B = prop * eps_ratio * B
        A = A + T_n * B
        n += step
        safety -= 1
    return A


def _safe_cosh(arg: float) -> float:
    """``cosh(arg)`` with the C's overflow guard at ``|arg| > 500``.

    Mirrors the cosh-with-cap pattern at
    ``asitic_kernel.c:9666-9716`` (and similar at 9892-9960). The C
    only guards positive args because its cosh arguments are
    products of positive z-distances; our Python uses a different
    z-coordinate sign convention so we guard the absolute value
    (cosh is even, so the math is unchanged).
    """
    if abs(arg) > 500.0:
        return 1.0e15
    return math.cosh(arg)


# ---- Main kernel (mirror decomp_readable/green_function_kernel_a.c) ----


def green_function_kernel_a(
    stack: LayerStack,
    last_layer: int,
    src_layer: int,
    obs_layer: int,
    k_rho: float,
    z_obs2: float,
    z_obs1: float,
) -> complex:
    """Above-source spectral integrand for the layered substrate.

    Mirrors ``green_function_kernel_a`` at
    ``asitic_kernel.c:0x0808cc90`` per the line-by-line refactor
    in ``decomp_readable/green_function_kernel_a.c``.

    Returns the spectral Green's-function value ``G(k_ρ, z_obs1, z_obs2)``
    for the case where the observation point sits above the source
    layer in the stratified insulator + substrate stack.

    Args:
        stack:       Pre-built ``LayerStack`` (use :func:`build_layer_stack`).
        last_layer:  Index of the outermost layer in the cosh-chain.
        src_layer:   Source layer index.
        obs_layer:   Observation layer index.
        k_rho:       Radial wavenumber (1/m).
        z_obs2:      Top-side observation z (m).
        z_obs1:      Bottom-side observation z (m).
    """
    if k_rho <= 0 or not len(stack.eps_complex):
        return 0.0 + 0j

    z_bot_src = stack.z_boundaries[src_layer]
    z_top_src = stack.z_boundaries[src_layer + 1]

    # 1. Three tanh boundary factors at the source layer.
    T1 = math.tanh(k_rho * (z_bot_src - z_obs1))
    T2 = math.tanh(k_rho * (z_top_src - z_obs2))
    T3 = math.tanh(k_rho * (z_top_src - z_bot_src))

    # 2. cosh-product chain across insulator layers above the source.
    z_top_stack = stack.z_boundaries[0]
    accum_cosh = 1.0
    for n in range(src_layer, last_layer + 1):
        if n + 1 >= len(stack.z_boundaries):
            break
        z_lay_bot = stack.z_boundaries[n]
        z_lay_top = stack.z_boundaries[n + 1]
        c_top = _safe_cosh(k_rho * (z_top_stack - z_lay_top))
        c_bot = _safe_cosh(k_rho * (z_top_stack - z_lay_bot))
        c_thk = _safe_cosh(k_rho * (z_lay_top - z_lay_bot))
        denom = c_bot * c_thk
        if denom != 0:
            accum_cosh *= c_top / denom

    # 3. Boundary helpers (branch on source-layer position).
    if src_layer == obs_layer:
        ratio_above = (1.0 / stack.eps_complex[src_layer - 1]
                       if src_layer > 0 else 0.0 + 0j)
        ratio_below: complex = 1.0 + 0j
        helper_a: complex = 1.0 + 0j
        helper_b = green_kernel_shared_helper_b(
            stack, src_layer - 1, k_rho,
        )
    elif src_layer == 0:
        ratio_above = 0.0 + 0j
        ratio_below = (1.0 / stack.eps_complex[1]
                       if len(stack.eps_complex) > 1 else 0.0 + 0j)
        helper_a = green_kernel_shared_helper_a(
            stack, 1, obs_layer, k_rho,
        )
        helper_b = green_kernel_shared_helper_b(stack, -1, k_rho)
    else:
        ratio_above = 1.0 / stack.eps_complex[src_layer - 1]
        ratio_below = (
            stack.eps_complex[src_layer]
            / stack.eps_complex[src_layer + 1]
        ) if src_layer + 1 < len(stack.eps_complex) else 0.0 + 0j
        helper_a = green_kernel_shared_helper_a(
            stack, src_layer + 1, obs_layer, k_rho,
        )
        helper_b = green_kernel_shared_helper_b(
            stack, src_layer - 1, k_rho,
        )

    # 4. Two a-helper invocations.
    helper_a_up = green_kernel_a_helper(
        stack, src_layer + 1, obs_layer, k_rho, z_obs2,
    )
    helper_a_dn = green_kernel_a_helper(
        stack, src_layer, obs_layer, k_rho, z_obs2,
    )

    # 5. Combine into the kernel numerator/denominator.
    prod_above_down = ratio_above * helper_a_dn
    prod_below_up = ratio_below * helper_a_up
    prod_above_below = (helper_b * ratio_above
                        - helper_a * ratio_below)

    term_num = (prod_above_down * T2
                + prod_below_up * T1
                + prod_above_below * (T1 * T2))
    top = term_num + (ratio_above * helper_a
                      - ratio_below * helper_b)
    top_scaled = accum_cosh * top

    prod_up_down = helper_a_up * helper_a_dn
    helper_combo = (prod_up_down
                    + T3 * prod_above_below
                    - T3 * (ratio_above * helper_a
                            + ratio_below * helper_b))
    if helper_combo == 0:
        return 0.0 + 0j
    result_num_per_denom = top_scaled / helper_combo

    # 6. Final cosh ratio correction.
    c_at_src = _safe_cosh(k_rho * (z_bot_src - z_obs1))
    c_above_src = _safe_cosh(k_rho * (z_top_stack - z_bot_src))
    if last_layer + 1 < len(stack.z_boundaries):
        z_top_of_last = stack.z_boundaries[last_layer + 1]
    else:
        z_top_of_last = stack.z_boundaries[-1]
    c_above_top = _safe_cosh(k_rho * (z_top_stack - z_top_of_last))
    c_to_obs = _safe_cosh(k_rho * (z_top_of_last - z_obs2))

    cosh_correction = (
        c_at_src * (c_above_src / c_above_top) * c_to_obs
        if c_above_top != 0 else 0.0
    )

    eps_factor = stack.eps_complex[src_layer]
    return result_num_per_denom * cosh_correction * eps_factor


def compute_green_function(
    tech: Tech,
    src_metal: int,
    obs_metal: int,
    omega_rad: float,
    nx: int,
    ny: int,
    chip_x_m: float,
    chip_y_m: float,
) -> np.ndarray:
    """Build the 2-D spectral Green's function grid.

    Mirrors ``compute_green_function`` at
    ``asitic_kernel.c:0x0808c350`` per the readable refactor in
    ``decomp_readable/compute_green_function.c``.

    Returns a ``(nx+1, ny+1)`` complex array where
    ``grid[i, j]`` is the kernel value at
    ``k_ρ(i,j) = sqrt((i·π/chip_x)² + (j·π/chip_y)²)``, divided
    by ``i² · j²`` (the spectral integral weight from the C's
    four ``operator/=`` calls).

    Boundary cells (``i ∈ {0, nx}`` or ``j ∈ {0, ny}``) are
    zeroed — the DCT-style spectral grid requires zero boundary
    values.

    Args:
        tech:        Tech file.
        src_metal:   Metal-table index of source trace.
        obs_metal:   Metal-table index of observation trace.
        omega_rad:   Angular frequency (rad/s).
        nx, ny:      Grid dimensions (``g_chip_xorigin``,
                     ``g_chip_yorigin`` in the C).
        chip_x_m:    Physical chip x extent (m).
        chip_y_m:    Physical chip y extent (m).
    """
    stack = build_layer_stack(tech, omega_rad)
    if not (0 <= src_metal < len(tech.metals)) or not (0 <= obs_metal < len(tech.metals)):
        return np.zeros((nx + 1, ny + 1), dtype=complex)

    layer_src = tech.metals[src_metal].layer
    layer_obs = tech.metals[obs_metal].layer
    z_src_init = metal_z_from_top(tech, src_metal)
    z_obs_init = metal_z_from_top(tech, obs_metal)
    t_src = tech.metals[src_metal].t * UM_TO_M
    t_obs = tech.metals[obs_metal].t * UM_TO_M

    grid = np.zeros((nx + 1, ny + 1), dtype=complex)
    last_layer_a = len(stack.eps_complex) - 1
    first_layer_b = 0

    for i in range(nx + 1):
        for j in range(ny + 1):
            if i == 0 or j == 0 or i == nx or j == ny:
                grid[i, j] = 0.0 + 0j
                continue
            kx = i * math.pi / chip_x_m
            ky = j * math.pi / chip_y_m
            k_rho = math.sqrt(kx * kx + ky * ky)

            z_src = z_src_init
            z_obs = z_obs_init
            # The C dispatches kernel_a vs kernel_b on z_src < z_obs
            # (i.e. source deeper than obs — kernel_a is the
            # above-source case). For non-same-metal pairs the
            # z's are pushed inward by ±0.5·t.
            if z_src < z_obs:
                if src_metal != obs_metal:
                    z_src += 0.5 * t_src
                    z_obs -= 0.5 * t_obs
                G = green_function_kernel_a(
                    stack, last_layer_a, layer_src, layer_obs,
                    k_rho, z_src, z_obs,
                )
            else:
                if src_metal != obs_metal:
                    z_src -= 0.5 * t_src
                    z_obs += 0.5 * t_obs
                G = green_function_kernel_b(
                    stack, first_layer_b, layer_src, layer_obs,
                    k_rho, z_src, z_obs,
                )
            # Spectral integral weight: /i²/j² (four operator/=
            # calls in the C; equivalent to scalar division).
            grid[i, j] = G / (float(i) ** 2 * float(j) ** 2)

    return grid


def fft_setup_inplace(green_grid: np.ndarray) -> None:
    """In-place 2-D DCT-via-FFT inversion of the spectral Green's grid.

    Mirrors ``fft_setup`` at ``asitic_kernel.c:0x08091548``. The C
    function does a mirror-extension + 1-D FFT in both dimensions
    to compute the inverse DCT, converting the spectral-domain
    Green's grid to the spatial-domain grid that
    ``capacitance_segment_integral`` uses for per-pair lookups.

    Steps (mirroring the C's two-pass row-then-column structure):

    * For each row ``i``: build a length-``2·N_y`` complex buffer
      with ``buf[0..ny] = grid[i, 0..ny]`` and the mirror-extension
      ``buf[ny+1..2*ny-1] = grid[i, ny-1..1]``. FFT in place.
      Copy ``buf[0..ny]`` back to ``grid[i, 0..ny]``.

    * Repeat for each column ``j`` with mirror-extension along
      the row axis.

    The mirror-extension makes the implicit-DCT FFT yield real
    coefficients (the DCT-II/III pair). The C's ``fft_apply_to_green``
    is a generic 1-D FFT; we use ``scipy.fft.fft`` for the
    equivalent.

    Args:
        green_grid: in-place ``(nx+1, ny+1)`` complex grid from
            :func:`compute_green_function`. Modified to spatial
            domain.
    """
    shape = green_grid.shape
    if len(shape) != 2:
        raise ValueError(f"green_grid must be 2-D, got {shape}")
    nxp1, nyp1 = shape
    nx = nxp1 - 1
    ny = nyp1 - 1

    # Pass 1: FFT each row.
    for i in range(nxp1):
        buf = np.zeros(2 * ny, dtype=complex)
        buf[: ny + 1] = green_grid[i, :]
        # Mirror extension: buf[ny+1..2*ny-1] = grid[i, ny-1, ny-2, ..., 1]
        for k in range(ny + 1, 2 * ny):
            buf[k] = green_grid[i, 2 * ny - k]
        out = scipy.fft.fft(buf)
        green_grid[i, : ny + 1] = out[: ny + 1]

    # Pass 2: FFT each column.
    for j in range(nyp1):
        buf = np.zeros(2 * nx, dtype=complex)
        buf[: nx + 1] = green_grid[:, j]
        for k in range(nx + 1, 2 * nx):
            buf[k] = green_grid[2 * nx - k, j]
        out = scipy.fft.fft(buf)
        green_grid[: nx + 1, j] = out[: nx + 1]


def green_function_kernel_b(
    stack: LayerStack,
    first_layer: int,    # p2 in C — note: this is the LOWER bound of chain
    src_layer: int,      # p3
    obs_layer: int,      # p4
    k_rho: float,
    z_obs2: float,
    z_obs1: float,
) -> complex:
    """Below-source spectral integrand (sister of kernel_a).

    Mirrors ``green_function_kernel_b`` at
    ``asitic_kernel.c:0x0808dad4``. Per the readable refactor:

    Differences from ``green_function_kernel_a``:

    * T1 uses ``g[src+1]`` (TOP of source layer) instead of
      ``g[src]`` (bottom): ``T1 = tanh(k · (g[src+1] - z_obs1))``
    * T2 uses ``g[src]`` (bottom) instead of ``g[src+1]`` (top):
      ``T2 = tanh(k · (g[src] - z_obs2))``
    * T3 is unchanged (layer thickness): ``T3 = tanh(k · (g[src+1] - g[src]))``
    * Cosh chain bounds reversed: ``for n in [first_layer, src_layer]``
      instead of ``[src_layer, last_layer]``.

    Implementation note: the boundary helper logic and final
    combination follow the same pattern as ``kernel_a``. The two
    kernels share the same structural framework — the binary
    chooses between them via the obs-vs-src position check in
    ``compute_green_function``.
    """
    if k_rho <= 0 or not len(stack.eps_complex):
        return 0.0 + 0j

    z_bot_src = stack.z_boundaries[src_layer]
    z_top_src = stack.z_boundaries[src_layer + 1]

    # 1. Three tanh boundary factors (note swap of T1/T2 vs kernel_a).
    T1 = math.tanh(k_rho * (z_top_src - z_obs1))
    T2 = math.tanh(k_rho * (z_bot_src - z_obs2))
    T3 = math.tanh(k_rho * (z_top_src - z_bot_src))

    # 2. Cosh chain — bound is [first_layer, src_layer] (reversed).
    z_top_stack = stack.z_boundaries[0]
    accum_cosh = 1.0
    for n in range(first_layer, src_layer + 1):
        if n + 1 >= len(stack.z_boundaries):
            break
        z_lay_bot = stack.z_boundaries[n]
        z_lay_top = stack.z_boundaries[n + 1]
        c_top = _safe_cosh(k_rho * (z_top_stack - z_lay_top))
        c_bot = _safe_cosh(k_rho * (z_top_stack - z_lay_bot))
        c_thk = _safe_cosh(k_rho * (z_lay_top - z_lay_bot))
        denom = c_bot * c_thk
        if denom != 0:
            accum_cosh *= c_top / denom

    # 3. Boundary helpers — same structure as kernel_a.
    if src_layer == obs_layer:
        ratio_above = (1.0 / stack.eps_complex[src_layer - 1]
                       if src_layer > 0 else 0.0 + 0j)
        ratio_below: complex = 1.0 + 0j
        helper_a: complex = 1.0 + 0j
        helper_b = green_kernel_shared_helper_b(
            stack, src_layer - 1, k_rho,
        )
    elif src_layer == 0:
        ratio_above = 0.0 + 0j
        ratio_below = (1.0 / stack.eps_complex[1]
                       if len(stack.eps_complex) > 1 else 0.0 + 0j)
        helper_a = green_kernel_shared_helper_a(
            stack, 1, obs_layer, k_rho,
        )
        helper_b = green_kernel_shared_helper_b(stack, -1, k_rho)
    else:
        ratio_above = 1.0 / stack.eps_complex[src_layer - 1]
        ratio_below = (
            stack.eps_complex[src_layer]
            / stack.eps_complex[src_layer + 1]
        ) if src_layer + 1 < len(stack.eps_complex) else 0.0 + 0j
        helper_a = green_kernel_shared_helper_a(
            stack, src_layer + 1, obs_layer, k_rho,
        )
        helper_b = green_kernel_shared_helper_b(
            stack, src_layer - 1, k_rho,
        )

    # 4. The two b-helper invocations (sister of a-helper in kernel_a).
    helper_b_up = green_kernel_b_helper(
        stack, src_layer + 1, obs_layer, k_rho, z_obs2,
    )
    helper_b_dn = green_kernel_b_helper(
        stack, src_layer, obs_layer, k_rho, z_obs2,
    )

    # 5. Combination — same algebra as kernel_a with the
    # helper_a/_b roles swapped (kernel_b uses b-helpers).
    prod_above_down = ratio_above * helper_b_dn
    prod_below_up = ratio_below * helper_b_up
    prod_above_below = (helper_b * ratio_above
                        - helper_a * ratio_below)

    term_num = (prod_above_down * T2
                + prod_below_up * T1
                + prod_above_below * (T1 * T2))
    top = term_num + (ratio_above * helper_a
                      - ratio_below * helper_b)
    top_scaled = accum_cosh * top

    prod_up_down = helper_b_up * helper_b_dn
    helper_combo = (prod_up_down
                    + T3 * prod_above_below
                    - T3 * (ratio_above * helper_a
                            + ratio_below * helper_b))
    if helper_combo == 0:
        return 0.0 + 0j
    result_num_per_denom = top_scaled / helper_combo

    # 6. Final cosh ratio correction — kernel_b uses g[first_layer]
    # instead of g[last_layer+1] for the propagation through the
    # rest of the stack.
    c_at_src = _safe_cosh(k_rho * (z_top_src - z_obs1))
    c_above_src = _safe_cosh(k_rho * (z_top_stack - z_top_src))
    if first_layer < len(stack.z_boundaries):
        z_lower_ref = stack.z_boundaries[first_layer]
    else:
        z_lower_ref = stack.z_boundaries[-1]
    c_below_top = _safe_cosh(k_rho * (z_top_stack - z_lower_ref))
    c_to_obs = _safe_cosh(k_rho * (z_lower_ref - z_obs2))

    cosh_correction = (
        c_at_src * (c_above_src / c_below_top) * c_to_obs
        if c_below_top != 0 else 0.0
    )

    eps_factor = stack.eps_complex[src_layer]
    return result_num_per_denom * cosh_correction * eps_factor
