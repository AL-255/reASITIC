"""Pythonic facade over the Cython-wrapped C math kernels.

The bodies live in :mod:`reasitic._kernel` (built by setup.py from
``../recomp/asitic_kernel.c``).  This module re-exports them under
slightly nicer names and adds module-level docstrings; it does *not*
re-derive any numerics.
"""
from __future__ import annotations

from typing import Iterable

from . import _kernel as _k

__all__ = [
    "grover_segment_self_inductance",
    "coupled_wire_self_inductance",
    "wire_inductance_far_field",
    "coupled_microstrip_caps",
    "vec3_norm",
    "vec3_dot",
    "vec3_cross",
    "dist3d",
    "coth_kernel",
    "safe_divide",
    "safe_log_minus_x",
    "clipped_pow2",
    "complex_cosh",
    "complex_sinh",
    "complex_sqrt",
    "complex_div",
    "spiral_turn_position",
    "wire_periodic_position",
    "wire_periodic_separation",
]


def grover_segment_self_inductance(length: float, radius: float) -> float:
    """Grover's closed-form self-inductance for a straight filament of
    length ``length`` (μm) and effective radius ``radius`` (μm).
    Returns 0 for vanishing length."""
    return _k.grover_segment(length, radius)


def coupled_wire_self_inductance(w: float, h: float, d: float) -> float:
    """Grover coupled-wire self-inductance term."""
    return _k.coupled_wire_grover(w, h, d)


def wire_inductance_far_field(
    w1: float, w2: float, t1: float, t2: float, dx: float, dy: float
) -> float:
    """Far-field mutual-inductance kernel between two parallel
    filaments separated by (dx, dy) in plane."""
    return _k.wire_far_field(w1, w2, t1, t2, dx, dy)


def coupled_microstrip_caps(
    W: float, s: float, h: float, eps_r: float
) -> tuple[float, float, float, float, float]:
    """Hammerstad-Jensen coupled-microstrip cap matrix.

    Returns ``(Cp, Cf, Cf', Cga, Cgd)`` -- parallel-plate, outer fringing,
    inner (coupling-reduced) fringing, air-gap, and dielectric-gap
    capacitances in F/cm.
    """
    return _k.coupled_microstrip_caps(W, s, h, eps_r)


def vec3_norm(v: Iterable[float]) -> float:
    """Euclidean norm of a length-3 vector."""
    return _k.vec3_norm(v)


def vec3_dot(a: Iterable[float], b: Iterable[float]) -> float:
    """Dot product of two length-3 vectors."""
    return _k.vec3_dot(a, b)


def vec3_cross(a: Iterable[float], b: Iterable[float]) -> tuple[float, float, float]:
    """Cross product of two length-3 vectors."""
    return _k.vec3_cross(a, b)


def dist3d(a: Iterable[float], b: Iterable[float]) -> float:
    """Euclidean distance between two 3-points."""
    return _k.dist3d(a, b)


def coth_kernel(x: float) -> float:
    """The C kernel's ``coth_double`` -- not mathematical coth; see the
    docstring in :mod:`reasitic._kernel`."""
    return _k.coth_kernel(x)


def safe_divide(numerator: float, denominator: float) -> float:
    """Clipped-denominator divide used throughout the C kernel."""
    return _k.safe_divide(numerator, denominator)


def safe_log_minus_x(a: float, b: float) -> float:
    """ASITIC's NaN-safe ``log(a) - b``."""
    return _k.safe_log_minus_x(a, b)


def clipped_pow2(x: float) -> float:
    """Clipped ``2**x`` kernel."""
    return _k.clipped_pow2(x)


def complex_cosh(z: tuple[float, float]) -> tuple[float, float]:
    """Complex cosh on a (real, imag) tuple."""
    return _k.complex_cosh(z)


def complex_sinh(z: tuple[float, float]) -> tuple[float, float]:
    """Complex sinh on a (real, imag) tuple."""
    return _k.complex_sinh(z)


def complex_sqrt(z: tuple[float, float]) -> tuple[float, float]:
    """Principal complex sqrt on a (real, imag) tuple."""
    return _k.complex_sqrt(z)


def complex_div(num: tuple[float, float], den: tuple[float, float]) -> tuple[float, float]:
    """Complex divide ``num / den`` on (real, imag) tuples."""
    return _k.complex_div(num, den)


def spiral_turn_position(
    i: int, outer_dim: float, width: float, spacing: float, fold_size: int
) -> float:
    """Recursive turn-position helper used by the square-spiral builder."""
    return _k.spiral_turn_position(i, outer_dim, width, spacing, fold_size)


def wire_periodic_position(
    i: int, outer_dim: float, width: float, spacing: float, fold_size: int
) -> float:
    """Periodic-fold wire position helper."""
    return _k.wire_periodic_position(i, outer_dim, width, spacing, fold_size)


def wire_periodic_separation(
    i: int, j: int, p3: float, p4: float, p5: float, fold_size: int
) -> float:
    """Periodic wire-pair separation helper."""
    return _k.wire_periodic_separation(i, j, p3, p4, p5, fold_size)
