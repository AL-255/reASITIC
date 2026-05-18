/*
 * asitic_kernel.h -- human-readable rewrite of decomp/output/asitic_kernel.c
 *
 * This header declares the numerical kernel of ASITIC: Grover /
 * Greenhouse inductance formulas, Hammerstad-Jensen coupled-
 * microstrip capacitance, DC-resistance, 2-port / 3-port Y<->Z<->S
 * conversion, and the substrate Green's-function machinery.
 *
 * The implementation in asitic_kernel.c is a clean reconstruction
 * of the Ghidra-decompiled binary at decomp/output/asitic_kernel.c.
 * Numerical structure (factor order, intermediate variable count)
 * matches the original; presentation is conventional C.
 *
 * Naming conventions follow the cleaned Ghidra rename pass:
 *   g_*          -- BSS / data global, named in globals.tsv
 *   _c_const_*   -- rodata constant whose numeric value still has
 *                   to be read out of run/asitic.linux.2.2
 *   _DAT_*       -- bss slot Ghidra was unable to name
 *
 * Geometry & filaments: filament records are 0xec (236) bytes per
 * entry; their interior layout is reproduced through byte-offset
 * accessors below.  See decomp/output/globals_log.tsv for the
 * authoritative field offsets.
 */

#ifndef ASITIC_RECOMP_KERNEL_H
#define ASITIC_RECOMP_KERNEL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- External helpers from other translation units --------------------- */

extern long double safe_log_minus_x_clipped(double a, double b);
extern void        print_error(const char *fmt, ...);
/* Binary symbol misnamed by Ghidra -- the body reads one line from
 * stdin (or the active script) into the caller-supplied buffer.
 * The `add_to_history` flag controls whether the line is appended
 * to readline history when the binary was started with --readline. */
extern void        print_warning(char *buf, char add_to_history);
extern void        print_fatal_and_exit(const char *msg);

/* Forward declarations for the capacitance pipeline. */
int  capacitance_setup(const char *p1, const char *p2);
void build_segment_node_list(int shape, int ctx);
int  filament_list_setup(int *ctx);
int  build_filament_list(int spiral_p);
int  fill_impedance_matrix_triangular(int *Z_out, int ctx);
void fill_inductance_diagonal(int *Z_full, int *Z_reduced, int ctx);
int  fill_inductance_offdiag(int *Z_out, int *filaments);

/* Mutual-inductance dispatch helpers (segment classifier +
 * three closed-form branches). */
int  check_segments_intersect(uint32_t *seg_a, uint32_t *seg_b, double *out_M);
int  mutual_inductance_3d_segments(double *seg_a, double *seg_b);
int  mutual_inductance_filament_general(double *seg_a, double *seg_b,
                                        double *out_M, double sep);
void mutual_inductance_4corner_grover(int seg_a, int seg_b, double *out_M);
void mutual_inductance_orthogonal_segments(int seg_a, int seg_b, double *out_M);
int  filament_pair_4corner_integration(double *corner_a, double *corner_b,
                                       double *corner_c, double *corner_d,
                                       double length, double separation);
void mutual_inductance_assemble_pair(int segment, double *out_tile,
                                     double *origin_xy,
                                     int x_idx, int y_idx, int z_idx,
                                     int nx, int ny, int nz);

/* 3-port reductions / Z->S / pi-equiv extraction. */
void reduce_3port_z_to_2port_y(void);
void z_to_s_3port_50ohm(void);
void extract_pi_equivalent(double freq_GHz);
void clear_yzs_globals(void);

/* Substrate Green's-function machinery (Niknejad-Gharpurey-Meyer
 * 1998, IEEE TCAD 17(4)).  Signatures decoded from the binary +
 * reASITIC/decomp_readable refactors.  All complex<double>
 * results are returned via an out[0..1] pair (re, im) so the file
 * can stay pure-C without pulling in <complex.h>. */
void compute_green_function(int src_metal, int obs_metal,
                            double *green_grid,
                            double *x_axis_fft, double *y_axis_fft);

void green_function_kernel_a(double *out,
                             int last_layer, int src_layer, int obs_layer,
                             double k_rho, double z_obs2, double z_obs1);
void green_function_kernel_b(double *out,
                             int last_layer, int src_layer, int obs_layer,
                             double k_rho, double z_obs2, double z_obs1);

/* Decomp p2/p3 are substrate-layer indices used both as the
 * iteration bound and as the index into g_substrate_layer_table.
 * inner_a takes one scalar (p4), inner_b takes two (p4_a is
 * passed by the binary but ignored by the body; p6 is the active
 * scalar). */
void capacitance_integral_inner_a(double *out,
                                  int p2, int p3,
                                  double p4);
void capacitance_integral_inner_b(double *out,
                                  int p2, int p3,
                                  double p4_a, double p6);

void green_kernel_shared_helper_a(double *out, int layer_lo, int layer_hi,
                                  double k_rho);
void green_kernel_shared_helper_b(double *out, int layer_idx, double k_rho);
void green_kernel_a_helper(double *out, int src_layer, int obs_layer,
                           double k_rho, double z_obs);
void green_kernel_b_helper(double *out, int src_layer, int obs_layer,
                           double k_rho, double z_obs);

void capacitance_per_segment(int *ctx, int seg_i, int seg_j, int layer_idx);
void capacitance_segment_integral(double *out_cap,
                                  char *seg_i, char *seg_j,
                                  double *x_green, double *y_green,
                                  double *xy_green);
void  fft_apply_to_green(void *complex_arr, int N);
void  fft_setup(int green_grid, int nx, int ny);
int   analyze_capacitance_polygon(void *out_Y, const char *shape_a,
                                   const char *shape_b, double freq_Hz);
/* Decomp arg order: (shape, matrix, freq.lo, freq.hi).  The shape
 * is the spiral struct (accessed at +0x50, +0x68, +0x98); spiral
 * is the MV_ColMat<complex> matrix into which the eddy stamps land. */
int   gen_eddy_current_matrix(int shape, int *spiral,
                              double freq_GHz, void *mesh_ctx);
void  eddy_matrix_assemble(int *spiral, int shape, double freq_GHz,
                           void *mesh_ctx, void *row_ctx, void *col_ctx);
long double green_oscillating_integrand(double k, uint64_t omega_pair);
long double green_propagation_integrand(double k, uint64_t omega_pair);
void  set_cell_size_normal(int batch_mode);
void  set_cell_size_critical(int batch_mode);
long double green_function_dqawf_wrapper(uint64_t integrand, double omega,
                                         uint64_t lower, uint64_t upper);
long double compute_dqagi_wrapper(uint64_t integrand, double omega,
                                  uint64_t lower, uint64_t upper);

/* Top-level solvers and assemblers. */
int  solve_node_equations(int spiral, void *p2, int p3);
int  solve_3port_equations(int p1, int p2, int p3, int p4, int p5);
int  solve_inductance_matrix(int spiral, void *p2, void *p3, void *p4,
                             char verbose);
void lmat_subblock_assemble(void **lmat);

/* (lapack_lu_* prototypes below near the LAPACK wrappers section.) */

/* Complex<double> primitives.  In the source binary these were
 * libstdc++ pre-ABI templated helpers with deeply mangled names
 * (e.g. `cosh__H1Zd_RCt7complex1ZX01_t7complex1ZX01`); the
 * recomp reimplements them as plain C functions over (re, im)
 * double pairs.  The mangled names are aliased via macros in
 * asitic_kernel.c so the existing call sites continue to parse,
 * but the *bodies* now live in this translation unit. */
void cpx_cosh(double *out, const double *z);
void cpx_sinh(double *out, const double *z);
void cpx_sqrt(double *out, const double *z);
/* (a+bi) / (c+di) -- writes out = num/den. */
void cpx_div (double *out, const double *num, const double *den);
/* in-place divide:  acc /= den. */
void cpx_div_eq(double *acc, const double *den);
/* real-numerator divide:  out = num_real / den, where num_real is
 * reconstructed from two 32-bit halves (the calling convention
 * Ghidra recovered for `__dv__H1Zd_X01RCt7complex1ZX01_t7complex1ZX01`,
 * with the high half typically 0x3ff00000 to denote +1.0). */
void cpx_real_div(double *out, uint32_t num_lo, uint32_t num_hi,
                  const double *den);

/* --- Globals ----------------------------------------------------------- */

extern int     g_num_substrate_layers;     /* 0x080d8900 */
extern int     g_num_metal_layers;         /* 0x080d8904 */
extern int     g_num_via_layers;           /* 0x080d8908 */
/* IMPORTANT: the *_layer_table globals are NOT flat arrays.  In the
 * binary they are (char *) variables that point to heap-allocated
 * descriptor blocks populated by the tech-file parser; see e.g.
 * 0x80564f2: `add 0x080d8910,%eax` in lookup_metal_layer_by_name,
 * which loads the pointer value, not the address of the symbol.
 * Declaring them as `char[]` would make recomp arithmetic
 * miss-target by 4 bytes into BSS.  Entry stride / field layout:
 *   g_metal_layer_table : stride 0xec; name @+0, sub_idx (int) @+0xa0,
 *                          thickness (double) @+0xb0, sigma @+0xb8.
 *   g_via_layer_table   : stride 0xf0; name @+0, depth fields @+0xa0/+0xa8,
 *                          rho (double) @+0xc0. */
extern char   *g_metal_layer_table;        /* 0x080d8910 -> heap, stride 0xec */
extern char   *g_via_layer_table;          /* 0x080d8914 -> heap, stride 0xf0 */
/* Same pointer-vs-array trap as the layer-table globals: the binary
 * loads them as 4-byte pointer values (`mov 0x80d8XXX, %eax` /
 * `mov 0x80d8XXX, %edx`).  Declaring as `char[]` makes recomp
 * arithmetic miss the indirection. */
extern char   *g_substrate_layer_table;    /* 0x080d8ac8 -> heap, stride 0x10 */
extern char   *g_substrate_height;         /* 0x080d890c -> heap, stride 0x28 */
extern char   *g_metal_layer_color_index;  /* 0x080d891c -> heap, stride 8 */

extern void   *g_current_shape;            /* 0x080ce558, head of shape linked list */
extern void   *g_current_shape_name;       /* 0x080ce55c, cached "." alias */
extern char    g_line_buffer[];            /* 0x080d7e40, sized 0xae */
extern char   *g_savefile_buffer;          /* 0x080ce9a4 */
extern char    g_capacitance_options[];
extern int     g_verbose_mode;

extern double  g_chip_diagonal_um;
/* g_chip_xorigin / g_chip_yorigin are int4 in the binary (.data
 * at 0x080ce990 / 0x080ce994; adjacent slots are 4 bytes apart --
 * proves the field width).  Declaring as `double` would slurp the
 * neighbouring int as the high half of a fake 8-byte value. */
extern int     g_chip_xorigin;
extern int     g_chip_yorigin;
extern double  g_chip_size_xy_m;
extern double  g_chip_xmax;                /* 0x080ce980 (.data) */
extern double  g_chip_ymax;                /* 0x080ce988 (.data) */
extern double  g_eddy_default_radius;      /* 0x080ce598 (.data) */
extern double  g_chip_size_xyz_buf;        /* 0x080ce5a0 (.data) */

extern double  g_pan_x;
extern double  g_pan_y;
extern double  g_zoom_scale;
extern int     g_x11_canvas_width;
extern int     g_x11_canvas_height;

extern double  g_opt_radius_max;

extern char    g_save_format_pi;
extern char    g_save_format_aux;

extern void   *g_green_cache_a;
extern void   *g_green_cache_b;
extern void   *g_green_cache_c;
extern double  g_green_omega;
extern double  g_green_h;
extern double  g_green_sep;
extern double  g_green_z_offset;

extern double  g_Y11_re, g_Y11_im;
extern double  g_Y12_re, g_Y12_im;
extern double  g_Y21_re, g_Y21_im;
extern double  g_Y22_re, g_Y22_im;
extern double  Y22_re,   Y22_im;  /* aliased slot at 0x080d8de8/0x080d8df0 */

extern uint32_t g_Y11_word2, g_Y12_word2, g_Y21_word2, g_Y22_word2;

extern double  g_resistance_value;
extern double  g_inductance_value_nH;

extern double  g_yzs_freq_lo, g_yzs_freq_hi;
extern int     g_yzs_dim;

/* *port command state — see init_port_command_state.
 * g_port_termination_z encodes the impedance basis (1=R, 2=Y, 3=Z).
 * g_port_format_polar is misnamed; it's a 1-byte output-format flag
 * (0 = Cartesian r/i, 1 = polar m/p). */
extern int     g_port_in_setup;             /* 0x080d86c8 */
extern int     g_port_termination_z;        /* 0x080d86cc */
extern char    g_port_format_polar;             /* 0x080d86d0 */

/* Anonymous rodata constants.  All known addresses now have their
 * bit-exact values materialised as `#define _c_const_080cXXXX
 * ((long double)V)` macros in asitic_kernel.c (top-of-file
 * rodata-constants block).  No externs needed; the macros are
 * picked up via the .c file's translation unit before the rest
 * of the body. */

extern double _g_um_to_m, _g_chip_T_um;
extern double _g_green_inv_h_layer1, _g_green_inv_h_layer2, _g_green_sigma_scale;
extern double _g_port_voltage_freq_Hz, _g_opt_W_step, _g_opt_S_step;
extern double _g_opt_N_min, _g_opt_target_accuracy_pct, _g_opt_exit_freq_Hz;
extern double _g_pi_L2_value, _g_pi_RG_value, _g_pi_R1_value, _g_pi_R2_value;
extern double _g_pi_aux_cell, _g_pi_M_value, _DAT_080d8870, _DAT_080d8878;
extern double _g_pi_Z33_re, _g_pi_Z33_im, _g_pi_Z32_re, _g_pi_Z32_im;
extern double _g_pi_Z31_re, _g_pi_Z31_im, _g_pi_Z23_re, _g_pi_Z23_im;
extern double _g_pi_Z22_re, _g_pi_Z22_im, _g_pi_Z21_re, _g_pi_Z21_im;
extern double _g_pi_Z13_re, _g_pi_Z13_im, _g_pi_Z12_re, _g_pi_Z12_im;
extern double _g_pi_Z11_re, _g_pi_Z11_im, _g_S31_re;
extern int    _g_yzs_3p_M21_re;

/* --- Kernel function prototypes --------------------------------------- */

/* Grover / Greenhouse inductance formulas. */
long double coupled_wire_self_inductance_grover(double w, double h, double d);
long double grover_segment_self_inductance(double length, double radius);
long double wire_inductance_far_field_kernel(double w1, double w2,
                                             double t1, double t2,
                                             double dx, double dy);

/* Inner skin-effect/inductance kernel. */
long double compute_inductance_inner_kernel(const double *filament,
                                            double freq_GHz);

/* DC resistance per polygon (two material tables). */
void compute_dc_resistance_per_polygon(int shape,
                                       double *out_res_a,
                                       double *out_res_b,
                                       double *out_coupled_cap);
void compute_dc_resistance_3metal_constants(int shape,
                                            double *out_r_metal_a,
                                            double *out_r_metal_b,
                                            double *out_r_metal_c);

/* Hammerstad-Jensen coupled-microstrip capacitance. */
void coupled_microstrip_caps_hj(double W, double s, double h, double eps_r,
                                double *Cp, double *Cf, double *Cf_prime,
                                double *Cga, double *Cgd);
/* NB: the Ghidra-recovered signature here lists 10 params
 * (mode, W, s, h, eps_r, *out_Cp, *out_Cf, *C_self, *C_mutual,
 * layer_idx) but the only call site in the binary
 * (compute_dc_resistance_per_polygon) passes 7 args:
 * (mode, W, s, eps_r, *C_self, *C_mutual, layer_idx).  The
 * shorter form is what the binary actually used; the unused
 * decomp parameters are Ghidra's signature-recovery artefact.
 * h is looked up internally from the substrate table. */
void coupled_microstrip_to_cap_matrix(int mode,
                                      double W, double s, double eps_r,
                                      double *C_self, double *C_mutual,
                                      int layer_idx);

/* 2-port / 3-port linear-algebra helpers backed by the global Y matrix. */
long double imag_z_2port_from_y(int diff_mode);
void        z_2port_from_y(double *out, char diff_mode, int port);
void        zin_terminated_2port(double *out, double *YL, int port);
long double compute_q_factor_from_globals(void);

/* Generic vector helpers. */
long double vec3_sqrt_dot_pair(const double *a, const double *b);
long double vec3_l2_norm(const double *v);
long double dist3d_pt(const double *a, const double *b);
void        vec3_cross_product(const double *a, const double *b, double *out);
long double vec3_dot_product(const double *a, const double *b);

/* Hyperbolic / numerically-safe helpers. */
long double coth_double(double x);
long double safe_divide_clipped(double numerator, double denominator);

/* Whole-kernel entry points used elsewhere in ASITIC. */
/* compute_mutual_inductance: walks the polygon chain of `shape`
 * (at +0xa8) and forms the cascaded ABCD network matrix of every
 * segment.  Each metal segment contributes a (series Z, shunt Y)
 * Pi-block built from coupled-microstrip cap matrix + skin-effect
 * R/L; each via segment contributes a simple R + L block.  When
 * the chain ends, the cascaded ABCD is inverted to extract the
 * 2-port Y parameters which are scattered into the global Y
 * slots (Y11, Y12, Y21, Y22) and then extract_pi_equivalent is
 * invoked to print the lumped pi-network model.  Returns 1 on
 * success (always succeeds modulo the per-segment integrator).
 * The i386 ABI splits the `freq` double across two uint32_t stack
 * slots; on amd64 we pass it as a single double. */
int  compute_mutual_inductance(intptr_t shape, double freq, char verbose);
int  compute_mutual_inductance_old(int shape, double freq, char verbose);
int  analyze_narrow_band_2port(int spiral, void *p2, double freq_GHz,
                               char verbose);
int  analyze_capacitance_driver(void *p1, void *p2, int p3,
                                double freq_GHz, char verbose);

/* Save / capacitance / list helpers. */
void  list_prepend_15int_node(int **head_pp, const int *src_node);
void  list_destroy_node_chain_at_38(void *head);
char *save_chain_find_by_name(const char *name);
void  capacitance_cleanup(void);
void  spiral_list_reverse_at_84(void);
void  save_chain_unlink(char *node);

/* Polygon-vertex array helpers. */
void  backward_diff_2d_inplace(char *arr_base, int n_pts);
void  forward_diff_2d_inplace(double *arr, int n_pts);
void  shape_emit_vias_at_layer_transitions(int shape);

/* Substrate Green's function leaves. */
long double reflection_coeff_imag(double k, double omega);
void  complex_propagation_constant_a(double *out, double k, double omega);
void  complex_propagation_constant_b(double *out, double k, double omega);
void  cdouble_tanh(double *out, double *z);
int   segment_pair_distance_metric(int segment);

/* Periodic-fold / spiral-position helpers. */
long double wire_axial_separation(const double *wire);
int   eddy_packed_index(int i, int j);
long double wire_position_periodic_fold(int i, double outer_dim, double width,
                                        double spacing, int fold_size);
long double wire_separation_periodic(int i, int j,
                                     double p3, double p4, double p5,
                                     int fold_size);
long double spiral_turn_position_recursive(int i, double outer_dim,
                                           double width, double spacing,
                                           int fold_size);

/* Spiral / prompt helpers. */
long double spiral_radius_for_N(double outer_dim, double spacing, double width,
                                int sides, int spiral_type);
long double spiral_FindMaxN(double outer_dim, double spacing, double width,
                            int sides, int spiral_type);
void  prompt_metal_layer(int *out_layer, const char *prompt);
void  prompt_exit_metal_layer(int *out_layer, int current_layer,
                              const char *prompt);
void  prompt_unique_shape_name(char *out_name);

/* Cell-size / view helpers. */
void  kernel_noop_stub_a(void);
void  kernel_noop_stub_b(void);
void  shape_bbox_scan(int shape,
                      double *out_min_x, double *out_min_y,
                      double *out_max_x, double *out_max_y);
void  compute_overall_bounding_box(double *out_min_x, double *out_min_y,
                                   double *out_max_x, double *out_max_y);
void  view_zoom_to_rectangle(int x0, int y0, int x1, int y1);

/* Filament / inductance helpers. */
void  destroy_filament_record_5char_5ptr(int self, unsigned int in_charge);
int   filament_list_to_index_array(int *ctx);

/* MNA / node-eq pieces. */
void  node_eq_unpack_forward(double *src, double *bias, int out_nodes, int N);
void  node_eq_unpack_backward(double *src, double *bias, int out_nodes, int N);
void  node_eq_back_substitute(int solution, int bias, int out_nodes, int N);
void  node_eq_assemble(int A, double *x, double *out_b, int N);
void  node_eq_setup_rhs(int matrix, int rhs, int n_nodes);

/* Misc leaf math. */
long double cos_or_sin_select(const double *x, const double *arg, int flag);
long double ref_pow_double(const double *x, const double *y);
long double clipped_pow2_x(double x);

/* Y/Z/S small leaves. */
void  y_to_z_2port_invert(void);
void  y_to_s_2port_50ohm(void);

/* Eddy / inductance leaves.
 *
 * The trailing (uint32_t, uint32_t) pair in axial_term, segment_kernel,
 * and inductance_eddy_fold is the i386 ABI splitting of an 8-byte
 * "integrand" pointer (decomp's (p2, p3) for axial_term;
 * (p3, p4) for segment_kernel; (fold_ctx, work) for eddy_fold).
 * All three forward the halves to green_function_select_integrator
 * via its packed `void *integrand` slot. */
void inductance_eddy_fold(int shape, int *matrix,
                          uint32_t fold_ctx, uint32_t work);
long double mutual_inductance_filament_kernel(double *a, double *b,
                                              double *out);
long double mutual_inductance_axial_term(int wire,
                                         uint32_t p2, uint32_t p3);
long double mutual_inductance_segment_kernel(double *a, double *b,
                                             uint32_t p3, uint32_t p4);

/* Green's-function inner kernels. */
long double green_function_kernel_a_oscillating(const double *x);
long double green_function_kernel_b_reflection(const double *x);
long double green_function_select_integrator(void *integrand, double omega,
                                             uint32_t lower_lo, uint32_t lower_hi,
                                             uint32_t upper_lo, uint32_t upper_hi);

/* LAPACK wrappers (real bodies live in asitic_lapack.c; these are the
 * recovered thin shims in asitic_kernel.c -- bodies below). */
int   lapack_lu_factor_matobj(void *A_mat, void *ipiv);
int   lapack_lu_solve_matobj(void *A_mat, void *B_mat, void *ipiv);
int   lapack_lu_factor_raw(void *A, void *ipiv, int N);
int   lapack_lu_solve_raw(void *A, void *B, void *ipiv, int N, int NRHS);

/* Polygon-bound helpers from the REPL TU. */
extern long double polygon_max_x_extreme_with_acc(int shape, double acc);
extern long double polygon_min_x_extreme_with_acc(int shape, double acc);

/* LAPACK / matrix-object helpers from the FORTRAN TU. */
extern int  cxx_mv_colmat_size(void *mat, int dim);
extern void ZGETRF(int *M, int *N, void *A, int *LDA, void *IPIV, int *INFO);
extern void ZGETRS(char *TRANS, int *N, int *NRHS, void *A, int *LDA,
                   void *IPIV, void *B, int *LDB, int *INFO);

/* REPL stdin helper. */
extern void read_command_line(const char *prompt, void *line_buffer, uint32_t flags);

/* REPL name-table lookups (asitic_repl.c).  These functions normalize
 * their input in place via normalize_input_line, so the buffer must
 * be writable. */
extern int   lookup_metal_layer_by_name(char *name);
extern int   lookup_via_layer_by_name(char *name);
extern void *lookup_shape_by_name(char *name);
extern int   lookup_command_id_by_name(char *name);
extern int   lookup_command_id_by_alias(char *name);
extern void  cmd_pause_wait_for_key(void);
extern unsigned char stdin_has_input_select(void);
extern unsigned int  readline_eol_callback(void);

/* Interactive prompt helpers (asitic_repl_prompts.c). */
/* readline / log-I/O machinery (asitic_repl_logio.c) */
extern char  g_use_readline;
extern char  g_readline_active;
extern char  g_arg_use_readline;
extern FILE *g_log_input_fp;
extern FILE *g_log_output_fp;
extern FILE *g_log_input_fp_alt;
extern FILE *g_log_output_fp_alt;
extern FILE *g_input_script_fp;

extern void  close_log_files(void);
extern void  reopen_log_files(void);
extern void  init_select_input_routines(void);
/* print_fatal_and_exit is declared variadic at the top of this header
 * (line 43); the port at recomp/asitic_repl.c uses one arg verbatim. */
extern void  dispatch_command(char *line);
extern void  read_one_line_with_log(const char *prompt, char *out_line,
                                    char echo_to_history);
extern void  init_resolve_x11_display(void);
extern void  open_log_files_for_session(const char *prompt, char *out_line,
                                        char echo_to_history);
extern void  print_info_with_prefix(const char *prompt, char *out_buf,
                                    char echo_to_history);
extern void  read_input_line(char *out_buf, char echo_to_history);
extern void  post_command_cleanup(void);
extern void  cmd_scale_apply(void);
extern void  cmd_scale_clamp_view(void *shape);
extern void  cmd_bb_show_bounding_box(void);
extern void  cmd_listsegs_show(void *shape);
extern void  cmd_capacitor_build_geometry(void *shape);
extern void  cmd_balun_build_geometry(void *shape_a, void *shape_b);
extern void  polygon_subdivide_along_segment(double *poly, double *base,
                                             double *dir, int i, int n);
extern void  shape_translate_inplace_xy(void *shape, double dx, double dy);
extern void  init_via_polygon_record_metal(void *dst, void *src);
extern void  shape_property_setter(void *shape, FILE *fp);
extern void  destroy_savefile_chain_at_d4(void);
extern void  destroy_savefile_chain_at_d0(void);
extern void  destroy_savefile_chain_at_24(void);
extern void  init_techlayer_record_a(void *rec);
extern void  init_techlayer_record_b(void *rec);
extern void  init_substrate_corner_record(void *rec);
extern int   parse_command_args(char *line, void *args_buf);
extern void  free_green_function_cache(void);
extern void  print_status_line_overwrite(const char *msg);
extern void  print_to_stdout_and_log(const char *fmt);
extern void  print_to_log_only(const char *fmt, char to_log);
extern void  log_to_input_log_fp(const char *fmt, const char *value);

/* cmd_* dispatch helpers (asitic_repl.c). */
extern void  cmd_options_print(void);
extern void  execute_script_file(char *filename);
extern void  cmd_showldiv_print(void *state);
extern void  destroy_port_command_state(void);
extern void  init_print_banner(void);
extern void  init_check_memory_budget(void);
extern void  techlayer_oom_502_copy(void *src);
extern void  techlayer_oom_503_copy(char *src);
extern void  techlayer_oom_504_copy(char *src);
extern void  init_finalize_initfile_arg(void);
extern void  init_load_techfile(void);
extern void  init_open_keyboard_redirect(void);
extern void  init_install_signal_handlers(void);
extern void  cmd_showldiv_format(void *shape);
extern void  cmd_modifytechlayer_apply(void);

/* Polygon-record utility leaves (asitic_repl_shapes.c). */
extern void  vec3_copy(double *dst, const double *src);
extern void  polygon_record_copy_subblock(void *dst, const void *src);
extern void  polygon_record_copy(void *dst, const void *src);
extern void  vec3_normalize_diff(double *seg, double *out_unit_dir);
extern void  shape_clear_polygon_select_flags(void *shape);
extern int   shape_count_polygons_visible_metal(void *shape);
extern void  cmd_flip_apply(void *shape);
extern void  polygon_translate_to_align_shapes(double *poly,
                                               void *shape_src,
                                               void *shape_dst);
extern void  shape_for_each_polygon_apply(void *shape, void *other_shape);
extern void  shape_record_init_from_args(char *shape, const char *args_buf);
extern void  cmd_rename_apply(char *shape, char *new_name);
extern void  polygon_collapse_endpoints_2d(void *poly);
extern void  shape_polygon_set_metal_layer(void *poly, int layer_idx);
extern long double cmd_metalarea_print(void *shape);
extern void  geometry_record_alloc(void);
extern void  destroy_polygon_chain_recursive(void *head);
extern void  destroy_all_shapes(void);
extern void  cmd_erase_remove(void *shape);
extern void  display_list_append(void **list_head_pp, const void *src_buf);
extern void  cmd_joinshunt_apply(void *shape_a, void *shape_b);
extern void *shape_3d_clone_apply_then_flip(void *src_shape);
extern void  cmd_rotate_apply(void *shape, void *fn_a, void *fn_b);
extern void  cmd_zin_compute(char diff_mode, int port);
extern long double cmd_inductance_compute(void *shape);
extern long double cmd_coupling_compute(void *shape_a, void *shape_b,
                                        double *out_M);
extern void  cmd_resis_compute(void *shape);
extern unsigned int point_in_polygon_winding(int n, double *vx, double *vy,
                                             double px, double py);
extern unsigned char polygon_contains_point_2d(void *shape, void *poly,
                                               double px, double py);
extern unsigned int shape_contains_point_walk_polygons(void *shape, double px,
                                                       double py);
extern void  polygon_set_metal_color_only(void *poly, int layer_idx);
extern void  shape_polygons_xy_extreme(void *shape, double *out_max,
                                       double *out_min);

extern void  prompt_spiral_phase(int *out_phase);
extern void  prompt_spiral_orient(double *out_orient);
extern void  prompt_origin_xy(double *out_x, double *out_y);
extern void  read_angle_radians(const char *prompt, double *out_radians);
extern void  prompt_metal_width(double *out_W, double max_W);
extern void  prompt_radius(double *out_radius);
extern void  prompt_polygon_sides(int *out_sides);
extern void  prompt_spacing(double *out_spacing, double max_S);

/* Diagnostic segment-tuple dumpers (asitic_repl.c). */
extern void  dump_segment_pairs_to_file(void *shape, const char *filename);
extern void  dump_segment_triples_to_file(void *shape, const char *filename);
extern void  dump_segment_quads_to_file(int shape_a, const char *filename,
                                        int shape_b);

/* Wire-bounds + tile-count validator.
 *
 *   wire     : pointer to wire/segment struct (~0xec bytes; fields at
 *              byte offsets 0/8/.../0x28 are endpoint doubles,
 *              +0xcc width, +0xdc metal-idx).
 *   src      : pointer to a struct whose first bytes are the shape
 *              name (sprintf-able) and whose +0x54 / +0x5c doubles
 *              are the x / y translation applied to wire endpoints.
 *   out_buf  : pointer to a slot that receives the populated shape
 *              struct.  When keep_existing == 0 the function calls
 *              oom_501 to allocate via the binary's chunk allocator
 *              and stores g_savefile_buffer there; otherwise the
 *              caller's existing pointer is reused.
 *   keep_existing : non-zero to skip the oom_501 allocation.
 *
 * Returns the low-byte boolean: 0 on out-of-range (after a
 * `print_error` call), 1 on success.  The high 3 bytes of the
 * return are the wire's metal-idx int -- harmless padding that
 * callers ignore. */
/* `keep_existing` is a CHAR in the binary's ABI: the caller pushes
 * a 16-bit slot at ESP-2 via `pushw $0`/`pushw $1` after first
 * making 2-byte room with `sub $-2,%esp`.  The high 2 bytes of
 * the 4-byte parameter slot are therefore uninitialised garbage --
 * reading as `int` would pick that up.  Declare as `char` so
 * gcc emits a `movzbl` / `movsbl` byte-load. */
extern int   coordinate_bounds_check(void *wire, void *src,
                                     void *out_buf, char keep_existing);

/* Binary helper used by coordinate_bounds_check: allocates a shape
 * struct via the C++ runtime's chunk allocator and stows the new
 * pointer in g_savefile_buffer. */
extern void  oom_501(void);

/* g_command_table is the REPL command catalog at 0x080cbac0.  Entry
 * stride is 0x5c bytes; entry layout (recovered into
 * decomp/output/commands.{md,json}) is:
 *   +0x00 (int)            command id (-2 = end-of-table sentinel)
 *   +0x04..+0x10 (4 ptrs)  command name + 3 aliases (NUL-terminated)
 *   +0x14...               handler / mode / flags
 * Declared as `int` so callers can read entry 0's id directly. */
extern int   g_command_table;              /* 0x080cbac0 (.data) */

extern void prompt_and_normalize(const char *prompt, void *line_buffer);
extern void normalize_input_line(char *buffer, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* ASITIC_RECOMP_KERNEL_H */
