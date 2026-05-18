/* _extra_stubs.c -- additional weak stubs for the recomp.
 *
 * asitic_kernel.c references a few entry points that normally come
 * from asitic_repl.c / asitic_repl_shapes.c / asitic_repl_ports.c.
 * For the math-kernel-only build, those translation units are not
 * compiled in, so we need no-op stubs to satisfy the dynamic linker
 * at load time.  None of the paths we expose from Python call these.
 */

void cmd_square_build_geometry(void *args, int color_idx)
{
    (void)args;
    (void)color_idx;
}

void cmd_spiral_build_geometry(void *args)
{
    (void)args;
}

void cmd_pi3_emit(void)
{
}

void narrowband_pi_qs_print(void)
{
}
