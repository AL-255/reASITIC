Concepts
========

This page sketches the conceptual model behind reASITIC and points to
the modules that implement each part.

Tech file
---------

A *tech file* (``.tek``) describes the process stack: metal layer
thicknesses and sheet resistances, oxide thicknesses and permittivities,
substrate resistivity, and so on.  The original ASITIC syntax is
preserved exactly; reASITIC parses and re-emits the same files
byte-compatibly.

* :func:`reasitic.parse_tech_file` -- parse a file into a
  :class:`reasitic.Tech`.
* :func:`reasitic.write_tech_file` -- round-trip back to disk.

Geometry
--------

The :mod:`reasitic.geometry` module contains the vertex-for-vertex
port of the binary's geometry kernel.  ``Shape`` objects are immutable
collections of :class:`Polygon` instances on numbered metal layers.

Class-based wrappers (:class:`reasitic.SquareSpiral`,
:class:`reasitic.Balun`, ...) sit on top in :mod:`reasitic.shapes`
and are the recommended entry point for new code.

Numerical pipeline
------------------

The pipeline used by :func:`reasitic.compute_self_inductance` and the
``Ind`` REPL command is:

1. **Filament discretisation.**  Each ribbon polygon is sliced along
   its width and thickness (:func:`reasitic.inductance.filament_grid`).
2. **Mutual-inductance matrix.**  Per-filament-pair partial mutuals
   are computed by the Hoer-Love closed-form / Grover series via
   :mod:`reasitic.inductance.grover` and :mod:`reasitic.inductance.skew`.
3. **MNA solve.**  Constraint equations are added (filaments in a
   parent segment share the same terminal voltage) and the system is
   solved by :func:`reasitic.inductance.solve_inductance_mna`.
4. **Reduction.**  The resulting node voltages give a single
   effective inductance and resistance per shape
   (:func:`reasitic.compute_self_inductance`,
   :func:`reasitic.compute_ac_resistance`).

Substrate model
---------------

Two implementations live in :mod:`reasitic.substrate`:

* :mod:`reasitic.substrate.shunt` -- fast parallel-plate per-metal-
  layer capacitance with edge fringe.  Default for
  :func:`reasitic.spiral_y_at_freq`.
* :mod:`reasitic.substrate.green` -- multi-layer Sommerfeld Green's
  function (quasi-static limit) with Bessel-J0 numerical integration.
  Used when ``substrate="green"`` is requested.

Network analysis
----------------

:mod:`reasitic.network` converts between Y, Z, and S parameters,
extracts Pi-equivalent lumped models from the Y matrix, and supports
2-port and 3-port reductions for centre-tapped / balun structures.

* :func:`reasitic.pi_model_at_freq` -- Pi-equivalent (L_eff, Q, ...)
  at a single frequency.
* :func:`reasitic.two_port_sweep` -- frequency sweep.
* :func:`reasitic.write_touchstone_file` -- export as ``.s2p``.

Optimisation
------------

:mod:`reasitic.optimise` wraps :mod:`scipy.optimize` to maximise Q
over the design space of a topology:

* :func:`reasitic.optimise_square_spiral`
* :func:`reasitic.optimise_polygon_spiral`
* :func:`reasitic.optimise_symmetric_square`
* :func:`reasitic.batch_opt_square` -- multi-target / multi-frequency
  batch.

Exports
-------

Layout writers in :mod:`reasitic.exports` produce the file formats
the original ASITIC supported:

* CIF (Caltech Intermediate Format)
* GDSII
* Sonnet ``.son``
* SPICE sub-circuit / broadband
* FastHenry input deck
* Tek 4014 plot
