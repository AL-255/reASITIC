/* stubs.c -- weak zero-initialised definitions for every binary
 * extern that asitic_kernel.c references but doesn't itself define.
 * Required so librecomp32.so loads cleanly via dlopen() without
 * RTLD_NOLOAD games -- the data refs in the recomp would otherwise
 * fail at load time.
 *
 * These stubs are NOT a faithful bridge to the original binary's
 * globals.  Their values are zero / no-op; they only exist to
 * satisfy the dynamic linker.  For the leaf functions we hook
 * (vec3_l2_norm, dist3d_pt, etc.) the stubs are never read because
 * those functions touch only their args.
 *
 * If you graduate the experiment to non-leaf hooks, REPLACE the
 * relevant stub here with a real bridge to the binary's BSS at the
 * known 0x080cxxxx / 0x080dxxxx address (e.g. via dlsym(NULL,
 * "DAT_xxxx") on a process that mapped the binary, or by
 * mmap'ing the binary's data segment into our address space). */

#include <stdint.h>

#define DEF_INT(name)   int    name __attribute__((weak)) = 0
#define DEF_CHAR(name)  char   name __attribute__((weak)) = 0
#define DEF_DBL(name)   double name __attribute__((weak)) = 0.0
#define DEF_LDBL(name)  const long double name __attribute__((weak)) = 0.0L
#define DEF_PTR(name)   void  *name __attribute__((weak)) = 0
#define DEF_BUF(name, sz) char name[sz] __attribute__((weak))

#define STUB_VOID(name, args, body) \
    __attribute__((weak)) void name args { body }
#define STUB_INT(name, args, body) \
    __attribute__((weak)) int  name args { body }

/* ------------------------------------------------------------ */
/* Named globals declared in asitic_kernel.h.                   */
/* ------------------------------------------------------------ */
/* g_substrate_height / g_substrate_layer_table are (char *) variables
 * in the binary, bridged via --defsym.  See note in asitic_kernel.h. */
char *g_substrate_height __attribute__((weak)) = 0;
char *g_substrate_layer_table __attribute__((weak)) = 0;
/* g_metal_layer_table is a `char *` in the binary (bridged via
 * --defsym at link time), not a flat buffer.  See note in
 * asitic_kernel.h. */
char *g_metal_layer_table __attribute__((weak)) = 0;
char *g_metal_layer_color_index __attribute__((weak)) = 0;
char *g_via_layer_table __attribute__((weak)) = 0;
DEF_BUF(g_capacitance_options,     256);
DEF_BUF(g_line_buffer,             512);
DEF_BUF(g_green_grid,              256);
/* g_savefile_buffer is a (char *) variable in the binary -- filled
 * by oom_501 with the address of a freshly allocated shape struct.
 * Bridged so writes from the recomp's coordinate_bounds_check land
 * in the binary's own slot. */
char *g_savefile_buffer __attribute__((weak)) = 0;

DEF_INT(g_num_metal_layers);
DEF_INT(g_num_via_layers);
DEF_INT(g_command_table);   /* first int of the .data command catalog */
DEF_INT(g_num_substrate_layers);
DEF_INT(g_chip_xorigin);
DEF_INT(g_chip_yorigin);
DEF_INT(g_pan_x);
DEF_INT(g_pan_y);
DEF_INT(g_x11_canvas_height);
DEF_INT(g_x11_canvas_width);
DEF_INT(g_verbose_mode);
DEF_DBL(g_zoom_scale);
DEF_DBL(g_chip_diagonal_um);
DEF_DBL(_g_chip_xmax);
DEF_DBL(g_chip_xmax);
DEF_DBL(g_chip_ymax);
DEF_DBL(g_eddy_default_radius);
DEF_DBL(g_chip_size_xyz_buf);
DEF_DBL(_g_freq_radians_per_second);
DEF_DBL(_g_inductance_value_nH);
DEF_DBL(g_inductance_value_nH);
DEF_DBL(g_resistance_value);
DEF_DBL(_g_resistance_value);
DEF_CHAR(g_save_format_pi);
DEF_CHAR(g_save_format_aux);
DEF_DBL(g_green_h);
DEF_DBL(g_green_omega);
DEF_DBL(g_green_sep);
DEF_PTR(g_green_cache_a);
DEF_PTR(g_green_cache_b);
DEF_PTR(g_green_cache_c);
DEF_PTR(g_current_shape);
DEF_PTR(g_current_shape_name);

/* readline / input-log machinery. */
DEF_CHAR(g_use_readline);
DEF_CHAR(g_readline_active);
DEF_CHAR(g_arg_use_readline);
DEF_CHAR(g_arg_version_exit);
DEF_CHAR(g_arg_have_init);
char *g_arg_init_path __attribute__((weak)) = 0;
DEF_CHAR(g_arg_have_tech);
char *g_arg_tech_path __attribute__((weak)) = 0;
DEF_CHAR(g_arg_have_keyboard);
char *g_arg_keyboard_path __attribute__((weak)) = 0;
DEF_CHAR(g_arg_have_exec);
char *g_arg_exec_path __attribute__((weak)) = 0;
DEF_CHAR(g_arg_have_log);
char *g_arg_log_basename __attribute__((weak)) = 0;
DEF_CHAR(g_arg_no_graphics);
DEF_CHAR(g_repl_eof_or_break);
DEF_CHAR(g_in_mouse_drag);
DEF_INT(g_input_mode);
DEF_PTR(g_x11_display);
DEF_INT(g_x11_window);
DEF_PTR(g_log_input_fp);
DEF_PTR(g_log_output_fp);
DEF_PTR(g_log_input_fp_alt);
DEF_PTR(g_log_output_fp_alt);
DEF_PTR(g_input_script_fp);
DEF_PTR(g_oom_502_seen);
DEF_PTR(g_oom_503_seen);
DEF_PTR(g_oom_504_seen);

/* Optimiser knobs. */
DEF_DBL(_g_opt_freq_GHz);
DEF_DBL(_g_optl_tolerance_pct);
DEF_INT(_g_opt_N_max);
DEF_DBL(g_opt_radius_max);
DEF_DBL(_g_opt_S_max);
DEF_DBL(_g_opt_S_min);
DEF_INT(_g_opt_solution_count);
DEF_DBL(_g_opt_target_nH);
DEF_DBL(_g_opt_W_max);
DEF_DBL(_g_opt_W_min);

/* Y / Z / 3-port output cells. */
DEF_DBL(g_Y11_re); DEF_DBL(g_Y11_im);
DEF_DBL(g_Y12_re); DEF_DBL(g_Y12_im);
DEF_DBL(g_Y21_re); DEF_DBL(g_Y21_im);
DEF_DBL(g_Y22_re); DEF_DBL(g_Y22_im);
DEF_DBL(Y22_re);   DEF_DBL(Y22_im);
DEF_INT(g_Y11_word2); DEF_INT(g_Y12_word2);
DEF_INT(g_Y21_word2); DEF_INT(g_Y22_word2);
DEF_DBL(g_yzs_freq_lo); DEF_DBL(g_yzs_freq_hi);
DEF_INT(g_yzs_dim);

/* Bonus rodata slots. */
DEF_LDBL(_c_const_080b5a70);
DEF_LDBL(_c_const_080bf8a0); DEF_LDBL(_c_const_080bf8b0);
DEF_LDBL(_c_const_080bf8c0); DEF_LDBL(_c_const_080bf8d0);
DEF_LDBL(_c_const_080bf8f0); DEF_LDBL(_c_const_080bf900);
DEF_LDBL(_c_const_080bf910); DEF_LDBL(_c_const_080bfad0);
DEF_LDBL(_c_const_080bfae0); DEF_LDBL(_c_const_080bfb00);

/* ------------------------------------------------------------ */
/* Auto-generated DAT_/_DAT_ slots from `nm -D --undefined-only`.*/
/* ------------------------------------------------------------ */
DEF_INT(g_eddy_current_enabled);
DEF_CHAR(g_timing_print_enabled);
/* parse_one_arg's local DAT slots — unit scales and grid snap.
 * The qwords (_g_grid_snap_size, DAT_080ce560, _g_um_to_m) are
 * declared as DEF_DBL below. */
DEF_CHAR(g_grid_snap_enabled);
/* radians-per-degree constant. */
DEF_INT(DAT_080c7020); DEF_INT(DAT_080c7024);
/* extract_pi_lumped_at_freq cells not already declared above. */
DEF_INT(g_pi_RG_value_word2);
DEF_INT(g_pi_M_value); DEF_INT(g_pi_M_value_word2);
/* extract_pi_lumped_at_freq input Z-matrix cells. */
DEF_INT(g_pi_Z22_re); DEF_INT(g_pi_Z22_re_word2);
DEF_INT(g_pi_Z22_im); DEF_INT(g_pi_Z22_im_word2);
DEF_INT(g_pi_Z12_re); DEF_INT(g_pi_Z12_re_word2);
DEF_INT(g_pi_Z12_im); DEF_INT(g_pi_Z12_im_word2);
DEF_INT(g_pi_Z11_re); DEF_INT(g_pi_Z11_re_word2);
DEF_INT(g_pi_Z11_im); DEF_INT(g_pi_Z11_im_word2);
/* extract_pi_lumped_3port additional cells. */
DEF_INT(g_pi3_L1_value); DEF_INT(g_pi3_L1_value_word2);
DEF_INT(g_pi3_L2_value); DEF_INT(g_pi3_L2_value_word2);
DEF_INT(g_pi3_L3_value); DEF_INT(g_pi3_L3_value_word2);
DEF_INT(g_pi3_R3_value); DEF_INT(g_pi3_R3_value_word2);
DEF_INT(g_pi3_M12); DEF_INT(g_pi3_M12_word2);
DEF_INT(g_pi3_M13); DEF_INT(g_pi3_M13_word2);
DEF_INT(g_pi3_M23); DEF_INT(g_pi3_M23_word2);
DEF_INT(g_pi3_k12); DEF_INT(g_pi3_k12_word2);
DEF_INT(g_pi3_k13); DEF_INT(g_pi3_k13_word2);
DEF_INT(g_pi3_k23); DEF_INT(g_pi3_k23_word2);
DEF_INT(g_pi3_Re_Z12); DEF_INT(g_pi3_Re_Z12_word2);
DEF_INT(g_pi3_Re_Z13); DEF_INT(g_pi3_Re_Z13_word2);
DEF_INT(g_pi3_Re_Z23); DEF_INT(g_pi3_Re_Z23_word2);
DEF_INT(g_pi_Z33_re); DEF_INT(g_pi_Z33_re_word2);
DEF_INT(g_pi_Z33_im); DEF_INT(g_pi_Z33_im_word2);
DEF_INT(g_pi_Z23_re); DEF_INT(g_pi_Z23_re_word2);
DEF_INT(g_pi_Z23_im); DEF_INT(g_pi_Z23_im_word2);
DEF_INT(g_pi_Z13_re); DEF_INT(g_pi_Z13_re_word2);
DEF_INT(g_pi_Z13_im); DEF_INT(g_pi_Z13_im_word2);
DEF_INT(g_green_eddy_layer_count_cached);
DEF_INT(g_chip_xmax_half); DEF_INT(g_chip_xmax_half_word2); DEF_INT(g_chip_diagonal_um_word2);
DEF_INT(g_chip_size_xy_m); DEF_INT(g_asitic_version_str);
DEF_INT(g_max_NW); DEF_INT(g_opt_shape_name); DEF_INT(g_batchopt_output_path);
DEF_INT(g_opt_metal_idx); DEF_INT(g_opt_solution_count_max); DEF_INT(g_simtype_code);
DEF_INT(g_pi_L2_value); DEF_INT(g_pi_L2_value_word2);
DEF_INT(g_pi_RG_value); DEF_INT(g_pi_R1_value_word2);
DEF_INT(g_pi_R1_value);
DEF_INT(g_pi_R2_value); DEF_INT(g_pi_R2_value_word2);
DEF_INT(g_Y22_re_word2); DEF_INT(g_Y22_im_word2);

DEF_DBL(_g_grid_snap_size);
DEF_DBL(DAT_080ce560);
DEF_DBL(_g_um_to_m); DEF_DBL(_g_chip_T_um);
DEF_DBL(_g_green_inv_h_layer1); DEF_DBL(_g_green_inv_h_layer2); DEF_DBL(_g_green_sigma_scale);
DEF_DBL(_g_port_voltage_freq_Hz);
DEF_DBL(_g_opt_W_step); DEF_DBL(_g_opt_S_step);
DEF_DBL(_g_opt_N_min); DEF_DBL(_g_opt_target_accuracy_pct); DEF_DBL(_g_opt_exit_freq_Hz);
DEF_DBL(_g_pi_L2_value); DEF_DBL(_g_pi_RG_value); DEF_DBL(_g_pi_R1_value);
DEF_DBL(_g_pi_R2_value); DEF_DBL(_g_pi_aux_cell); DEF_DBL(_g_pi_M_value);
DEF_DBL(_DAT_080d8870); DEF_DBL(_DAT_080d8878);

DEF_DBL(_g_pi_Z33_re); DEF_DBL(_g_pi_Z33_im); DEF_DBL(_g_pi_Z32_re);
DEF_DBL(_g_pi_Z32_im); DEF_DBL(_g_pi_Z31_re); DEF_DBL(_g_pi_Z31_im);
DEF_DBL(_g_pi_Z23_re); DEF_DBL(_g_pi_Z23_im); DEF_DBL(_g_pi_Z22_re);
DEF_DBL(_g_pi_Z22_re_word2); DEF_DBL(_g_pi_Z22_im); DEF_DBL(_g_pi_Z22_im_word2);
DEF_DBL(_g_pi_Z21_re); DEF_DBL(_g_pi_Z21_re_word2);
DEF_DBL(_g_pi_Z21_im); DEF_DBL(_g_pi_Z21_im_word2); DEF_DBL(_g_pi_Z13_re);
DEF_DBL(_g_pi_Z13_im); DEF_DBL(_g_pi_Z12_re); DEF_DBL(_g_pi_Z12_re_word2);
DEF_DBL(_g_pi_Z12_im); DEF_DBL(_g_pi_Z12_im_word2);
DEF_DBL(_g_pi_Z11_re); DEF_DBL(_g_pi_Z11_re_word2);
DEF_DBL(_g_pi_Z11_im); DEF_DBL(_g_pi_Z11_im_word2); DEF_DBL(_g_S33_re);
DEF_DBL(_g_S33_im); DEF_DBL(_g_S31_re); DEF_DBL(_g_S32_re);
DEF_DBL(_g_S32_im); DEF_DBL(_g_S13_re); DEF_DBL(_g_S13_im);
DEF_DBL(_g_S11_re); DEF_DBL(_g_S11_re_word2); DEF_DBL(_g_S11_im);
DEF_DBL(_g_S11_im_word2); DEF_DBL(_g_S12_re); DEF_DBL(_g_S12_re_word2);
DEF_DBL(_g_S12_im); DEF_DBL(_g_S12_im_word2); DEF_DBL(_g_S23_re);
DEF_DBL(_g_S23_im); DEF_DBL(_g_S21_re); DEF_DBL(_g_S21_re_word2);
DEF_DBL(_g_S21_im); DEF_DBL(_g_S21_im_word2); DEF_DBL(_g_S22_re);
DEF_DBL(_g_S22_re_word2); DEF_DBL(_g_S22_im); DEF_DBL(_g_S22_im_word2);
DEF_DBL(_g_yzs_3p_M21_re); DEF_DBL(_g_yzs_3p_M20_re); DEF_DBL(_g_yzs_3p_M20_im);
DEF_DBL(_g_yzs_3p_M12_re);

/* ------------------------------------------------------------ */
/* Helper functions from OTHER TUs of the original binary.      */
/* Stubs return 0 / nothing.                                     */
/* ------------------------------------------------------------ */
STUB_INT (cmd_erase_remove,           (void),                       return 0;)
/* cmd_pi3_emit now ported into recomp/asitic_repl_ports.c. */
STUB_INT (cmd_shuntr_compute,         (void),                       return 0;)
/* cmd_spiral_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_square_build_geometry now ported into recomp/asitic_repl_shapes.c. */
STUB_INT (coordinate_bounds_check,    (int s, const char *sh, int *o, int k),
          (void)s;(void)sh;(void)o;(void)k; return 1;)
STUB_INT (cxx_destroy_obj_with_array, (void *s, int m),
          (void)s;(void)m; return 0;)
STUB_VOID(cxx_mv_colmat_complex_subref_index2,(void),)
STUB_INT (cxx_mv_colmat_size,         (void *m, int d),
          (void)m;(void)d; return 0;)
STUB_VOID(cxx_mv_vector_complex_ctor_default,(void *s),  (void)s;)
STUB_VOID(cxx_mv_vector_complex_ctor_NM,(void *s, int N, int M),
          (void)s;(void)N;(void)M;)
STUB_VOID(cxx_mv_vector_complex_dtor, (void *s, int m), (void)s;(void)m;)
STUB_VOID(cxx_mv_vectorref_complex_assign,(void *d, void *s),
          (void)d;(void)s;)
STUB_VOID(MV_Vector_int_ctor,         (void *s, int N, int M),
          (void)s;(void)N;(void)M;)

STUB_VOID(dqagi_,                     (void),)
STUB_VOID(dqawf_,                     (void),)
STUB_VOID(ZGETRF,                     (void),)
STUB_INT (ZGETRI_alt_0806d974,        (void),                       return 0;)
STUB_VOID(ZGETRS,                     (void),)
STUB_VOID(ZSYTRF_alt_0806d5f0,        (void *m, int f), (void)m;(void)f;)

STUB_VOID(dump_segment_pairs_to_file,   (void *v, const char *fn),
          (void)v;(void)fn;)
STUB_VOID(dump_segment_quads_to_file,   (int  m, const char *fn, int N),
          (void)m;(void)fn;(void)N;)
STUB_VOID(dump_segment_triples_to_file, (void *m, const char *fn),
          (void)m;(void)fn;)
STUB_VOID(emit_via_record,              (void),)
STUB_INT (lookup_metal_layer_by_name,   (const char *n), (void)n; return -1;)
STUB_INT (lookup_shape_by_name,         (const char *n), (void)n; return  0;)
/* narrowband_pi_qs_print now ported into recomp/asitic_repl.c. */
STUB_VOID(normalize_input_line,         (void),)
STUB_INT (polygon_max_x_extreme_with_acc,(void), return 0;)
STUB_INT (polygon_min_x_extreme_with_acc,(void), return 0;)
STUB_VOID(print_error,                  (const char *fmt, ...),
          (void)fmt;)
STUB_VOID(print_fatal_and_exit,         (const char *fmt, ...),
          (void)fmt;)
STUB_VOID(print_status_line_overwrite,  (const char *s), (void)s;)
STUB_VOID(print_to_stdout_and_log,      (const char *s), (void)s;)
STUB_VOID(prompt_and_normalize,         (void),)
STUB_VOID(read_command_line,            (void),)
STUB_VOID(cif_emit_path_record,         (void),)
STUB_VOID(cxx_destroy_struct_with_5_strings, (void *p, unsigned f),
          (void)p;(void)f;)
STUB_VOID(exception_typeinfo_FileCorrupted, (void),)
STUB_VOID(exception_typeinfo_InvalidHeader, (void),)
STUB_VOID(port_y_parser,                  (void),)
STUB_VOID(save_compose_uppercase_spi_path, (void),)
STUB_VOID(save_compose_lowercase_spi_path, (void),)
/* shape_command_default_shape_arg now ported into recomp/asitic_repl_shapes.c. */
STUB_VOID(cxx_basic_string_Rep_clone,    (void),)
STUB_VOID(cxx_basic_string_replace_cstr, (void),)
STUB_VOID(cxx_basic_string_cow_assign_b, (void),)
STUB_VOID(cxx_basic_string_destroy_b,    (void),)
STUB_VOID(exception_typeinfo_InvalidVersion, (void),)
STUB_VOID(exception_typeinfo_InvalidMagicNumber, (void),)
/* Bridged repl_shapes polygon builders + handlers (deferred for port). */
/* cmd_square_build_geometry, cmd_spiral_build_geometry stubs already
 * defined earlier (STUB_INT with (void)). */
/* cmd_symsq_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_sympoly_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_ring_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_via_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_mmsquare_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_trans_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_3dtrans_build_geometry now ported into recomp/asitic_repl_shapes.c. */
/* cmd_3dtrans_create now ported into recomp/asitic_repl_shapes.c. */
/* cmd_balun_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_capacitor_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_mmsquare_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_ring_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_spiral_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_square_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_sympoly_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_symsq_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_trans_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_via_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_wire_create_new now ported into recomp/asitic_repl_shapes.c. */
/* cmd_balun_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_capacitor_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_ring_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_spiral_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_sympoly_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_symsq_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_trans_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_via_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_wire_edit_args now ported into recomp/asitic_repl_shapes.c. */
/* cmd_copy_clone now ported into recomp/asitic_repl_shapes.c. */
/* cmd_geom_show now ported into recomp/asitic_repl_shapes.c. */
/* cmd_sptowire_convert now ported into recomp/asitic_repl_shapes.c. */
/* shape_args_LSWN now ported into recomp/asitic_repl_shapes.c. */
/* redraw_after_geometry_change now ported into recomp/asitic_repl_shapes.c. */
/* filament_array_swap_axes now ported into recomp/asitic_repl_shapes.c. */
/* filament_list_to_array now ported into recomp/asitic_repl_shapes.c. */
/* geometry_record_dup_clone now ported into recomp/asitic_repl_shapes.c. */
/* symsq_emit_polygon_layers now ported into recomp/asitic_repl_shapes.c. */
STUB_VOID(sympoly_emit_polygon_layers, (void),)
/* repl_logio bridges -- ported: open_session_log_files, print_warning. */
/* repl_prompts bridges -- ported: prompt_dimension_with_default. */
/* repl_optimize bridges. */
/* cmd_batchopt_run now ported into recomp/asitic_repl_optimize.c. */
STUB_VOID(cmd_optarea_search,          (void),)
STUB_VOID(cmd_optl_search,             (void),)
STUB_VOID(cmd_optlsympoly_search,      (void),)
STUB_VOID(cmd_optlsymsq_search,        (void),)
STUB_VOID(cmd_optpoly_search,          (void),)
STUB_VOID(cmd_sweep_run,               (void),)
STUB_VOID(optl_prompt_target_inductance,(void),)
/* repl_ports bridges. */
/* build_segment_pair_index, cmd_lmat_print, cmd_pix_emit,
 * cmd_2portgnd_emit, cmd_2portx_emit, cmd_2portpad_emit,
 * cmd_2porttrans_emit, cmd_3port_emit, port_warn_user_struct now
 * ported into recomp/asitic_repl_ports.c. */
/* cmd_calctrans_emit, cmd_pi4_emit now ported into
 * recomp/asitic_repl_ports.c. */
/* cmd_resishf_compute now ported into recomp/asitic_repl_ports.c. */
/* cmd_shuntr_compute, cmd_selfres_compute now ported into
 * recomp/asitic_repl_ports.c. */
STUB_VOID(extract_pi_lumped_3port,     (void),)
STUB_VOID(extract_pi_lumped_at_freq,   (void),)
/* format_complex_pi_print now ported into recomp/asitic_repl.c. */
STUB_VOID(narrowband_model_print,      (void),)
/* print_yzs_table_6col, print_yzs_table_9col now ported into
 * recomp/asitic_repl_ports.c. */
/* read_freq_arg, read_port_termination_arg now ported into asitic_repl.c. */
/* select_output_format_or_default_pi now ported into recomp/asitic_repl_ports.c. */
/* s_param_component_label_print now ported into recomp/asitic_repl.c. */
/* Help-text literals referenced by cmd_help_emit_topic. */
DEF_CHAR(DAT_080be180);
DEF_CHAR(DAT_080be1e0);
DEF_CHAR(DAT_080be220);
DEF_CHAR(DAT_080be240);
DEF_CHAR(DAT_080be2a0);
DEF_CHAR(DAT_080be4c2);
DEF_CHAR(DAT_080be4e0);
DEF_CHAR(DAT_080be6ec);
DEF_CHAR(DAT_080be700);
DEF_CHAR(DAT_080bec24);
DEF_CHAR(DAT_080bec40);
DEF_CHAR(DAT_080becaa);
DEF_CHAR(DAT_080becc0);
DEF_CHAR(DAT_080bf0a1);
DEF_CHAR(DAT_080bf0c0);
DEF_CHAR(DAT_080bf5d8);
DEF_CHAR(DAT_080bf5e0);
DEF_CHAR(DAT_080bf760);
DEF_CHAR(DAT_080bf781);
DEF_CHAR(DAT_080ce5e0);
DEF_CHAR(DAT_080cbadc);
/* geom_emit_polygon_at now ported into recomp/asitic_repl_shapes.c. */
DEF_CHAR(g_sigint_abort_pending);  /* SIGINT-flagged-abort sentinel byte */
STUB_VOID(save_consume_block_directive, (void),)
STUB_VOID(save_emit_techfile_data_line, (void),)
STUB_VOID(sonnet_compose_dat_filename,  (void),)
STUB_VOID(spi_emit_lowercase_extension, (void),)
STUB_VOID(spi_emit_uppercase_extension, (void),)
STUB_VOID(sonnet_emit_data_file_per_freq,(void),)
STUB_VOID(vec3_normalize_diff,          (const double *a, const double *b, double *out),
          (void)a;(void)b;(void)out;)
