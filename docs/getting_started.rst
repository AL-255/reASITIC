Getting started
===============

Installation
------------

reASITIC builds a small Cython extension that wraps the vendored C
kernels in :file:`vendor/recomp/`.  A C toolchain, Python 3.9+, and
NumPy are required.

.. code-block:: bash

    pip install .

Editable installs work too::

    pip install -e .

The wheel ships everything needed at runtime; there is no dependency
on the upstream ASITIC binary or on the parent
``asitic-re`` monorepo.

Quick tour
----------

The package exposes a small, curated set of names at the top level.
Anything you need for inductor design lives one ``import reasitic``
away.

.. code-block:: python

    import reasitic as ra

    tech = ra.parse_tech_file("BiCMOS.tek")

    spiral = ra.SquareSpiral(
        name="L1", metal="m3",
        length=200.0, width=8.0, spacing=2.0, turns=3.0,
        tech=tech,
    )

    print(ra.compute_self_inductance(spiral, tech),
          ra.compute_dc_resistance(spiral, tech),
          ra.metal_only_q(spiral, tech, freq_ghz=2.4))

Optimising a design
-------------------

``reasitic.optimise`` wraps :mod:`scipy.optimize` so you can hand it a
target inductance and let it find the spiral geometry that maximises
Q.

.. code-block:: python

    from reasitic.optimise import optimise_square_spiral

    result = optimise_square_spiral(
        tech=tech, metal="m3",
        target_inductance_nH=2.0,
        freq_ghz=2.4,
        spacing=2.0,
    )
    print(result.length, result.width, result.turns, result.q)


Sweeps and Touchstone export
----------------------------

.. code-block:: python

    sweep = ra.two_port_sweep(
        spiral, tech,
        freqs_ghz=[0.1, 0.5, 1.0, 2.0, 5.0, 10.0],
    )
    ra.write_touchstone_file("L1.s2p", sweep)

Layout export
-------------

CIF, GDSII, Sonnet, FastHenry, and SPICE writers all live under
:mod:`reasitic.exports`:

.. code-block:: python

    ra.write_gds_file("L1.gds", spiral, tech)
    ra.write_cif_file("L1.cif", spiral, tech)
    ra.write_sonnet_file("L1.son", spiral, tech)

Using the REPL
--------------

If you want the ASITIC command syntax (``Sq NAME=L1 LEN=200 ...``)
the original CLI is available::

    python -m reasitic.cli

or programmatically:

.. code-block:: python

    from reasitic.cli import Repl
    repl = Repl(tech=ra.parse_tech_file("BiCMOS.tek"))
    repl.run_line("Sq NAME=L1 LEN=200 W=8 S=2 N=3 EXIT=m4 M=m5")
    repl.run_line("Ind NAME=L1 FREQ=2.4")
