"""Build the reasitic._kernel Cython extension by compiling the vendored
recomp C sources alongside the .pyx wrapper.

This package is fully self-contained: the ASITIC kernel C re-implementation
lives under ``vendor/recomp/`` and is the sole source of truth for the
build.  No paths outside the package root are consulted.

The recomp sources target the i386 ASITIC binary's x87 80-bit long-double
ABI; on 64-bit hosts we pre-process them through ``portable.sed`` which
collapses ``long double`` -> ``double``.  The pre-processed sources land in
``build/recomp_native/`` at build time so the wheel ships a host-native
shared extension instead of an i686-only LD_PRELOAD library.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext as _build_ext
from setuptools.command.sdist import sdist as _sdist

try:
    from Cython.Build import cythonize
except ImportError as e:  # pragma: no cover
    raise SystemExit(
        "Cython is required to build reasitic. "
        "Install it with `pip install Cython`."
    ) from e

ROOT = Path(__file__).resolve().parent
VENDOR_RECOMP = ROOT / "vendor" / "recomp"


def _recomp_root() -> Path:
    """Return the vendored recomp source tree.

    The package is self-contained; the only supported source for the
    kernel C is ``vendor/recomp/``.  Missing files indicate a corrupt
    checkout or sdist.
    """
    if VENDOR_RECOMP.exists():
        return VENDOR_RECOMP
    raise SystemExit(
        "vendor/recomp/ is missing -- the reasitic source tree is "
        "incomplete.  Reinstall from a clean checkout or sdist."
    )


RECOMP = _recomp_root()
PORTABLE_SED = RECOMP / "detour" / "portable.sed"
STUBS_C = RECOMP / "detour" / "stubs.c"

KERNEL_SOURCES = [
    RECOMP / "asitic_kernel.c",
    RECOMP / "asitic_kernel.h",
]


class sdist(_sdist):
    """sdist now just delegates -- vendor/recomp/ is the canonical
    source tree and is always present in the repo."""


def _ensure_portable_sources(dest_dir: Path) -> list[Path]:
    """Run portable.sed on every recomp source we need and drop the
    output under dest_dir. Returns the list of generated .c paths the
    extension should compile."""
    dest_dir.mkdir(parents=True, exist_ok=True)
    generated: list[Path] = []
    for src in KERNEL_SOURCES:
        if not src.exists():
            raise SystemExit(
                f"recomp source missing: {src} -- the reasitic source "
                "tree is incomplete.  Reinstall from a clean checkout "
                "or sdist."
            )
        dst = dest_dir / src.name
        # Re-generate when source is newer.
        if (
            not dst.exists()
            or src.stat().st_mtime > dst.stat().st_mtime
            or PORTABLE_SED.stat().st_mtime > dst.stat().st_mtime
        ):
            with dst.open("wb") as out:
                subprocess.check_call(
                    ["sed", "-f", str(PORTABLE_SED), str(src)],
                    stdout=out,
                )
        if dst.suffix == ".c":
            generated.append(dst)
    return generated


class build_ext(_build_ext):
    """Custom build_ext that materialises the portable C sources before
    delegating to setuptools' standard build."""

    def run(self) -> None:
        native_dir = ROOT / "build" / "recomp_native"
        sources = _ensure_portable_sources(native_dir)
        # Also stage stubs.c (no x87 in it, but copy to keep the build
        # tree self-contained for the wheel).
        stubs_dst = native_dir / "stubs.c"
        shutil.copyfile(STUBS_C, stubs_dst)
        extra_stubs = ROOT / "src" / "reasitic" / "_extra_stubs.c"
        # setuptools rejects absolute paths in ext.sources, so make
        # everything relative to setup.py.
        def _rel(p: Path) -> str:
            return str(p.relative_to(ROOT))
        rel_sources = [_rel(s) for s in sources]
        rel_stubs = _rel(stubs_dst)
        rel_extra = _rel(extra_stubs)
        rel_native_inc = _rel(native_dir)
        for ext in self.extensions:
            if ext.name == "reasitic._kernel":
                ext.sources = list(ext.sources) + rel_sources + [rel_stubs, rel_extra]
                ext.include_dirs = list(ext.include_dirs) + [rel_native_inc]
        super().run()


extensions = [
    Extension(
        name="reasitic._kernel",
        sources=["src/reasitic/_kernel.pyx"],
        include_dirs=["src/reasitic"],
        libraries=["m"],
        extra_compile_args=[
            "-O2",
            "-fvisibility=default",
            "-fno-stack-protector",
            "-Wno-format-extra-args",
            "-DRECOMP_PORTABLE",
        ],
        # The recomp references binary-only globals/functions (e.g.
        # cmd_square_build_geometry) that the kernel doesn't actually
        # call from the leaf math paths we expose.  --unresolved-symbols
        # lets the .so link cleanly; calls into those entry points will
        # raise at runtime (and we don't expose them).
        extra_link_args=["-Wl,--unresolved-symbols=ignore-all"],
    )
]


setup(
    ext_modules=cythonize(extensions, language_level=3),
    cmdclass={"build_ext": build_ext, "sdist": sdist},
)
