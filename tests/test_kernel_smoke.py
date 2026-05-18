"""Smoke tests for the Cython-wrapped C math kernels.

These verify the wrapped kernels link, accept Python-native arguments,
and return finite numerical values. Numerical-faithfulness vs the
original ASITIC binary is checked separately in
``test_validation_compare.py``.
"""
from __future__ import annotations

import math

import pytest

from reasitic import kernel


def test_grover_segment_positive():
    L = kernel.grover_segment_self_inductance(1000.0, 0.5)
    assert math.isfinite(L)
    assert L > 0


def test_grover_zero_length():
    # The Grover closed form returns 0 for vanishing length.
    assert kernel.grover_segment_self_inductance(0.0, 1.0) == 0.0


def test_coupled_microstrip_returns_five_components():
    Cp, Cf, Cfp, Cga, Cgd = kernel.coupled_microstrip_caps(
        W=10.0, s=3.0, h=5.0, eps_r=4.0
    )
    for x in (Cp, Cf, Cfp, Cga, Cgd):
        assert math.isfinite(x)
    # Parallel-plate cap dominates over edge fringing in this geometry.
    assert Cp > 0


@pytest.mark.parametrize(
    "a, b, expected",
    [
        ([3.0, 4.0, 0.0], [0.0, 0.0, 0.0], 5.0),
        ([1.0, 0.0, 0.0], [0.0, 1.0, 0.0], math.sqrt(2.0)),
    ],
)
def test_dist3d(a, b, expected):
    assert kernel.dist3d(a, b) == pytest.approx(expected, rel=1e-12)


def test_vec3_cross_and_dot():
    cross = kernel.vec3_cross([1.0, 0.0, 0.0], [0.0, 1.0, 0.0])
    assert cross == pytest.approx((0.0, 0.0, 1.0), abs=1e-12)
    dot = kernel.vec3_dot([1.0, 2.0, 3.0], [4.0, -5.0, 6.0])
    assert dot == pytest.approx(4 - 10 + 18, rel=1e-12)


def test_complex_sqrt_principal_branch():
    # sqrt(-1) -> (0, 1)
    re, im = kernel.complex_sqrt((-1.0, 0.0))
    assert (re, im) == pytest.approx((0.0, 1.0), abs=1e-12)


def test_safe_divide_clipped_no_overflow():
    # safe_divide(1, 0) should not raise; the kernel clips the
    # denominator before division.
    out = kernel.safe_divide(1.0, 0.0)
    assert math.isfinite(out)


def test_kernel_module_reexport():
    # The package-level alias should match the module-level handle.
    from reasitic import kernel as alias_a
    import reasitic
    assert reasitic.kernel is alias_a
