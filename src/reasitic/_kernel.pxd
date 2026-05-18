# cython: language_level=3
# C declarations for the recomp leaf math kernels we expose to Python.
#
# These prototypes mirror ../recomp/asitic_kernel.h with one twist: the
# host build pre-processes the recomp through portable.sed, so every
# `long double` returns folds to `double`.  Cython sees plain doubles
# either way (C-level cdecl through ST(0) on i386 is invisible at the
# language level), so we declare them as `double` here.

cdef extern from "asitic_kernel.h" nogil:
    # ---- Grover / Greenhouse closed-form inductance --------------------
    double grover_segment_self_inductance(double length, double radius)
    double coupled_wire_self_inductance_grover(double w, double h, double d)
    double wire_inductance_far_field_kernel(double w1, double w2,
                                            double t1, double t2,
                                            double dx, double dy)

    # ---- Hammerstad-Jensen coupled-microstrip capacitance --------------
    void coupled_microstrip_caps_hj(double W, double s, double h, double eps_r,
                                    double *Cp, double *Cf, double *Cf_prime,
                                    double *Cga, double *Cgd)

    # ---- vec3 helpers --------------------------------------------------
    double vec3_sqrt_dot_pair(const double *a, const double *b)
    double vec3_l2_norm(const double *v)
    double dist3d_pt(const double *a, const double *b)
    void   vec3_cross_product(const double *a, const double *b, double *out)
    double vec3_dot_product(const double *a, const double *b)

    # ---- Hyperbolic / numerically-safe helpers ------------------------
    double coth_double(double x)
    double safe_divide_clipped(double numerator, double denominator)
    double safe_log_minus_x_clipped(double a, double b)
    double clipped_pow2_x(double x)

    # ---- Complex<double> primitives -----------------------------------
    void cpx_cosh(double *out, const double *z)
    void cpx_sinh(double *out, const double *z)
    void cpx_sqrt(double *out, const double *z)
    void cpx_div (double *out, const double *num, const double *den)

    # ---- Spiral / wire-position helpers --------------------------------
    double wire_axial_separation(const double *wire)
    double wire_position_periodic_fold(int i, double outer_dim, double width,
                                       double spacing, int fold_size)
    double wire_separation_periodic(int i, int j,
                                    double p3, double p4, double p5,
                                    int fold_size)
    double spiral_turn_position_recursive(int i, double outer_dim,
                                          double width, double spacing,
                                          int fold_size)
    double spiral_radius_for_N(double outer_dim, double spacing, double width,
                               int sides, int spiral_type)
    double spiral_FindMaxN(double outer_dim, double spacing, double width,
                           int sides, int spiral_type)
