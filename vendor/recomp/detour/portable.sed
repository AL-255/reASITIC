# sed script that converts the x87-targeted recomp sources into a
# platform-independent variant.  Applied per-file by the Makefile
# when PORTABLE=1; produces sources in build/portable/ that are
# compiled into librecomp32_portable.so.
#
# What it does:
#   * `long double` -> `double` everywhere -- collapses 80-bit x87
#     to 64-bit IEEE.  Function signatures that return long double
#     still return double, but on i386 cdecl both go through ST(0),
#     so the LD_PRELOAD detour stays binary-compatible at the cost
#     of precision.
#   * Math-library `*l` variants -> double versions:
#       sqrtl  -> sqrt    sinl   -> sin     cosl   -> cos
#       expl   -> exp     logl   -> log     log10l -> log10
#       exp2l  -> exp2    expm1l -> expm1
#       atan2l -> atan2   atanl  -> atan
#       fabsl  -> fabs    floorl -> floor
#       ceill  -> ceil    truncl -> trunc
#       roundl -> round   rintl  -> rint
#       llrintl-> llrint  lrintl -> lrint
#       powl   -> pow     ldexpl -> ldexp
#       cbrtl  -> cbrt    hypotl -> hypot
#   * Long-double literals `1.0L`, `2.5L`, etc. -> drop the `L`.
#
# Word-boundary safety: each pattern uses an explicit non-identifier
# context on either side ([^a-zA-Z_0-9]) so we don't accidentally
# munge identifiers that happen to contain `sqrtl_helper` etc.  The
# `\(...\)` capture groups keep the surrounding character intact.

# `long double` -> `double` (handles both forward declarations and
# casts).  The keyword has whitespace between the tokens; collapse
# any combination.
s/\([^a-zA-Z_0-9]\)long  *double\([^a-zA-Z_0-9]\)/\1double\2/g
s/^long  *double\([^a-zA-Z_0-9]\)/double\1/g

# Math fn renames.  Match `<name>(` with a non-identifier left
# context so we don't rewrite mid-identifier occurrences.
s/\([^a-zA-Z_0-9]\)sqrtl(/\1sqrt(/g
s/\([^a-zA-Z_0-9]\)sinl(/\1sin(/g
s/\([^a-zA-Z_0-9]\)cosl(/\1cos(/g
s/\([^a-zA-Z_0-9]\)tanl(/\1tan(/g
s/\([^a-zA-Z_0-9]\)asinl(/\1asin(/g
s/\([^a-zA-Z_0-9]\)acosl(/\1acos(/g
s/\([^a-zA-Z_0-9]\)atanl(/\1atan(/g
s/\([^a-zA-Z_0-9]\)atan2l(/\1atan2(/g
s/\([^a-zA-Z_0-9]\)sinhl(/\1sinh(/g
s/\([^a-zA-Z_0-9]\)coshl(/\1cosh(/g
s/\([^a-zA-Z_0-9]\)tanhl(/\1tanh(/g
s/\([^a-zA-Z_0-9]\)expl(/\1exp(/g
s/\([^a-zA-Z_0-9]\)exp2l(/\1exp2(/g
s/\([^a-zA-Z_0-9]\)expm1l(/\1expm1(/g
s/\([^a-zA-Z_0-9]\)logl(/\1log(/g
s/\([^a-zA-Z_0-9]\)log2l(/\1log2(/g
s/\([^a-zA-Z_0-9]\)log10l(/\1log10(/g
s/\([^a-zA-Z_0-9]\)log1pl(/\1log1p(/g
s/\([^a-zA-Z_0-9]\)powl(/\1pow(/g
s/\([^a-zA-Z_0-9]\)cbrtl(/\1cbrt(/g
s/\([^a-zA-Z_0-9]\)hypotl(/\1hypot(/g
s/\([^a-zA-Z_0-9]\)fabsl(/\1fabs(/g
s/\([^a-zA-Z_0-9]\)fmodl(/\1fmod(/g
s/\([^a-zA-Z_0-9]\)floorl(/\1floor(/g
s/\([^a-zA-Z_0-9]\)ceill(/\1ceil(/g
s/\([^a-zA-Z_0-9]\)truncl(/\1trunc(/g
s/\([^a-zA-Z_0-9]\)roundl(/\1round(/g
s/\([^a-zA-Z_0-9]\)rintl(/\1rint(/g
s/\([^a-zA-Z_0-9]\)lrintl(/\1lrint(/g
s/\([^a-zA-Z_0-9]\)llrintl(/\1llrint(/g
s/\([^a-zA-Z_0-9]\)ldexpl(/\1ldexp(/g
s/\([^a-zA-Z_0-9]\)frexpl(/\1frexp(/g
s/\([^a-zA-Z_0-9]\)modfl(/\1modf(/g
s/\([^a-zA-Z_0-9]\)scalbnl(/\1scalbn(/g
s/\([^a-zA-Z_0-9]\)scalblnl(/\1scalbln(/g
s/\([^a-zA-Z_0-9]\)nextafterl(/\1nextafter(/g
s/\([^a-zA-Z_0-9]\)copysignl(/\1copysign(/g
s/\([^a-zA-Z_0-9]\)nanl(/\1nan(/g

# Same set, anchored to start-of-line (sed's `^` doesn't see the
# left-context capture group).
s/^sqrtl(/sqrt(/g
s/^sinl(/sin(/g
s/^cosl(/cos(/g
s/^expl(/exp(/g
s/^exp2l(/exp2(/g
s/^logl(/log(/g
s/^atan2l(/atan2(/g
s/^fabsl(/fabs(/g
s/^truncl(/trunc(/g
s/^rintl(/rint(/g
s/^roundl(/round(/g
s/^llrintl(/llrint(/g
s/^powl(/pow(/g
s/^cbrtl(/cbrt(/g
s/^hypotl(/hypot(/g
s/^ldexpl(/ldexp(/g

# `long double` literals: 1.0L, -2.5L, 3e9L, etc. -> drop the `L`.
# (C allows both `1.0` and `1.0L` as initializers for `double`; the
# value is identical when representable.  Stripping the suffix keeps
# the literal at `double` precision once the type is also `double`.)
s/\([0-9]\)L\([^a-zA-Z_0-9]\)/\1\2/g
s/\([0-9]\)L$/\1/g

# Mark every translation unit so the recomp body can compile-time
# check with `#ifdef RECOMP_PORTABLE` (if needed in the future).
1i\
#define RECOMP_PORTABLE 1\

