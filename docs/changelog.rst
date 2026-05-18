Changelog
=========

All notable changes to reASITIC are recorded here.  The format roughly
follows `Keep a Changelog <https://keepachangelog.com/en/1.1.0/>`_.

Unreleased
----------

Added
~~~~~

* Self-contained packaging: the C kernels live under ``vendor/recomp/``
  and the parent ``../recomp/`` checkout is no longer required at
  build time.
* Public Python API: curated re-exports at the top of
  :mod:`reasitic` plus lazy subpackage loading so cold imports stay
  cheap.
* Docstrings on every exported function, class, and module.
* Sphinx documentation with autodoc-driven API reference, deployed
  to GitHub Pages by the ``docs.yml`` workflow.

0.0.1
-----

Initial alpha release of the Python port.  See the project README for
the historical milestones from the reverse-engineering effort.
