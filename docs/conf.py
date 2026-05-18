"""Sphinx configuration for reASITIC documentation.

Run ``sphinx-build -b html docs docs/_build/html`` to render the docs
locally.  Output is what gets deployed to GitHub Pages by the workflow
at ``.github/workflows/docs.yml``.
"""
from __future__ import annotations

import importlib
import importlib.util
import os
import sys
from datetime import datetime
from pathlib import Path

# Prefer the *installed* reasitic over the in-tree source: only the
# installed package has the compiled Cython ``_kernel`` extension next
# to its ``.py`` files, while the ``src/`` checkout has only the
# ``.pyx``/``.pxd`` Cython sources.  If the installed wheel is missing
# (e.g. an offline doc-only checkout), fall back to ``src/`` so the
# docstrings still render even though autodoc may skip the C kernel.
ROOT = Path(__file__).resolve().parent.parent
if importlib.util.find_spec("reasitic") is None:
    sys.path.insert(0, str(ROOT / "src"))

try:
    from reasitic import __version__ as _pkg_version
except Exception:  # pragma: no cover - docs build without compiled ext
    _pkg_version = "0.0.0+unknown"


# -- Project information -----------------------------------------------------

project = "reASITIC"
author = "reASITIC contributors"
copyright = f"{datetime.now():%Y}, {author}"
release = _pkg_version
version = ".".join(_pkg_version.split(".")[:2])


# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.napoleon",
    "sphinx.ext.viewcode",
    "sphinx.ext.intersphinx",
    "sphinx.ext.mathjax",
    "sphinx.ext.todo",
    "myst_parser",
]

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

primary_domain = "py"
default_role = "py:obj"


# -- Autodoc / Napoleon ------------------------------------------------------

autosummary_generate = True
autodoc_default_options = {
    "members": True,
    "undoc-members": False,
    "show-inheritance": True,
    "member-order": "bysource",
}
autodoc_typehints = "description"
autodoc_typehints_format = "short"
autodoc_preserve_defaults = True

napoleon_google_docstring = True
napoleon_numpy_docstring = True
napoleon_include_init_with_doc = False
napoleon_include_private_with_doc = False
napoleon_use_rtype = False
napoleon_use_ivar = True


# -- Intersphinx -------------------------------------------------------------

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "matplotlib": ("https://matplotlib.org/stable/", None),
}


# -- HTML output -------------------------------------------------------------

html_theme = "furo"
html_static_path = ["_static"]
html_title = f"reASITIC {release}"
html_show_sourcelink = False
html_theme_options = {
    "sidebar_hide_name": False,
    "navigation_with_keys": True,
    "source_repository": "https://github.com/AL-255/reASITIC",
    "source_branch": "main",
    "source_directory": "docs/",
}


# -- Misc --------------------------------------------------------------------

todo_include_todos = False
add_module_names = False

# autosummary emits an "import_cycle" warning when the listed targets
# are inside the same package as the current automodule context.  In
# our layout that is intentional (the top-level ``automodule:: reasitic``
# block is just a docstring trampoline above the subpackage list), so
# suppress it.
suppress_warnings = ["autosummary.import_cycle"]
