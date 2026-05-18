# cython: language_level=3, boundscheck=False, wraparound=False
"""Cython wrappers for the recomp leaf math kernels.

Every function here is a thin shim over the C function of the same name
in ../recomp/asitic_kernel.c.  No formulas are re-derived in Python; the
goal is to surface the C-binary-faithful numerics with a Pythonic
calling convention (positional doubles in, doubles or tuples out).

For dimensional context, see ../recomp/asitic_kernel.c top-of-file
comments and run/doc/ in the asitic-re repo.
"""

from libc.stdlib cimport malloc, free

from . cimport _kernel as _k


# ---------------------------------------------------------------------
# Grover / Greenhouse closed-form inductance
# ---------------------------------------------------------------------

def grover_segment(double length, double radius):
    """Self-inductance of a single straight segment of length L and
    effective radius r (Grover table 1, closed form).

    Returns 0 for vanishing length.  The C entry point is
    ``grover_segment_self_inductance`` at recomp/asitic_kernel.c:592.
    """
    return _k.grover_segment_self_inductance(length, radius)


def coupled_wire_grover(double w, double h, double d):
    """Grover coupled-wire self-inductance term used by the spiral
    integrator.  C entry: ``coupled_wire_self_inductance_grover``."""
    return _k.coupled_wire_self_inductance_grover(w, h, d)


def wire_far_field(double w1, double w2, double t1, double t2,
                   double dx, double dy):
    """Far-field mutual-inductance kernel between two parallel wire
    segments.  See ``wire_inductance_far_field_kernel``."""
    return _k.wire_inductance_far_field_kernel(w1, w2, t1, t2, dx, dy)


# ---------------------------------------------------------------------
# Hammerstad-Jensen coupled-microstrip capacitance
# ---------------------------------------------------------------------

def coupled_microstrip_caps(double W, double s, double h, double eps_r):
    """Hammerstad-Jensen coupled-microstrip cap matrix.

    Parameters
    ----------
    W : double
        Strip width.
    s : double
        Edge-to-edge spacing.
    h : double
        Substrate height.
    eps_r : double
        Relative permittivity.

    Returns
    -------
    (Cp, Cf, Cf_prime, Cga, Cgd) : tuple of doubles
        ``Cp``     parallel-plate cap to ground
        ``Cf``     outer-edge fringing cap
        ``Cf_prime`` inner-edge fringing cap (coupling-reduced)
        ``Cga``    air-gap inter-strip cap
        ``Cgd``    dielectric-gap inter-strip cap
    """
    cdef double Cp = 0.0, Cf = 0.0, Cfp = 0.0, Cga = 0.0, Cgd = 0.0
    _k.coupled_microstrip_caps_hj(W, s, h, eps_r,
                                   &Cp, &Cf, &Cfp, &Cga, &Cgd)
    return Cp, Cf, Cfp, Cga, Cgd


# ---------------------------------------------------------------------
# vec3 helpers
# ---------------------------------------------------------------------

def _to_vec3(seq):
    cdef double *buf = <double *>malloc(3 * sizeof(double))
    if buf is NULL:
        raise MemoryError()
    cdef int i
    for i in range(3):
        buf[i] = float(seq[i])
    return <Py_ssize_t>buf


def vec3_norm(a):
    """Euclidean norm of a length-3 vector."""
    cdef double tmp[3]
    cdef int i
    for i in range(3):
        tmp[i] = float(a[i])
    return _k.vec3_l2_norm(tmp)


def vec3_dot(a, b):
    """Dot product of two length-3 vectors."""
    cdef double pa[3]
    cdef double pb[3]
    cdef int i
    for i in range(3):
        pa[i] = float(a[i])
        pb[i] = float(b[i])
    return _k.vec3_dot_product(pa, pb)


def vec3_cross(a, b):
    """Cross product of two length-3 vectors -> tuple of 3 floats."""
    cdef double pa[3]
    cdef double pb[3]
    cdef double out[3]
    cdef int i
    for i in range(3):
        pa[i] = float(a[i])
        pb[i] = float(b[i])
    _k.vec3_cross_product(pa, pb, out)
    return out[0], out[1], out[2]


def dist3d(a, b):
    """Euclidean distance between two 3-points."""
    cdef double pa[3]
    cdef double pb[3]
    cdef int i
    for i in range(3):
        pa[i] = float(a[i])
        pb[i] = float(b[i])
    return _k.dist3d_pt(pa, pb)


# ---------------------------------------------------------------------
# Hyperbolic / numerically-safe helpers
# ---------------------------------------------------------------------

def coth_kernel(double x):
    """The recomp's ``coth_double`` -- *named* coth but actually
    ``1 / ((expm1(-2|x|) / (expm1(-2|x|) + 2)) * sign_scale)``.  Kept
    binary-faithful even though it diverges from mathematical coth."""
    return _k.coth_double(x)


def safe_divide(double numerator, double denominator):
    """ASITIC's clipped-denominator divide.  Avoids div-by-zero by
    biasing |denominator| away from 0."""
    return _k.safe_divide_clipped(numerator, denominator)


def safe_log_minus_x(double a, double b):
    """ASITIC's ``safe_log_minus_x_clipped`` -- ``log(a) - b`` with
    NaN-safe clipping at the small-argument boundary."""
    return _k.safe_log_minus_x_clipped(a, b)


def clipped_pow2(double x):
    """Clipped 2**x kernel used by the spiral integrator."""
    return _k.clipped_pow2_x(x)


# ---------------------------------------------------------------------
# Complex<double> primitives
# ---------------------------------------------------------------------

def complex_cosh(z):
    """Complex cosh: z is a (re, im) pair, returns (re, im)."""
    cdef double zin[2]
    cdef double zout[2]
    zin[0] = float(z[0]); zin[1] = float(z[1])
    _k.cpx_cosh(zout, zin)
    return zout[0], zout[1]


def complex_sinh(z):
    """Complex sinh: z is a (re, im) pair, returns (re, im)."""
    cdef double zin[2]
    cdef double zout[2]
    zin[0] = float(z[0]); zin[1] = float(z[1])
    _k.cpx_sinh(zout, zin)
    return zout[0], zout[1]


def complex_sqrt(z):
    """Principal complex sqrt: z is (re, im), returns (re, im)."""
    cdef double zin[2]
    cdef double zout[2]
    zin[0] = float(z[0]); zin[1] = float(z[1])
    _k.cpx_sqrt(zout, zin)
    return zout[0], zout[1]


def complex_div(num, den):
    """Complex divide num/den, each is a (re, im) pair."""
    cdef double n_in[2]
    cdef double d_in[2]
    cdef double out[2]
    n_in[0] = float(num[0]); n_in[1] = float(num[1])
    d_in[0] = float(den[0]); d_in[1] = float(den[1])
    _k.cpx_div(out, n_in, d_in)
    return out[0], out[1]


# ---------------------------------------------------------------------
# Spiral / wire-position helpers
# ---------------------------------------------------------------------

def spiral_turn_position(int i, double outer_dim, double width,
                         double spacing, int fold_size):
    """Recursive turn-position helper used by the square-spiral builder.
    Returns the i-th turn's centerline distance from the spiral edge."""
    return _k.spiral_turn_position_recursive(i, outer_dim, width,
                                             spacing, fold_size)


def wire_periodic_position(int i, double outer_dim, double width,
                           double spacing, int fold_size):
    """Periodic-fold wire position helper."""
    return _k.wire_position_periodic_fold(i, outer_dim, width, spacing,
                                          fold_size)


def wire_periodic_separation(int i, int j, double p3, double p4, double p5,
                             int fold_size):
    """Periodic wire-pair separation helper."""
    return _k.wire_separation_periodic(i, j, p3, p4, p5, fold_size)
