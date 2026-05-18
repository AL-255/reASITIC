reASITIC documentation
======================

reASITIC is a modern Python interface to the ASITIC numerical kernels
(*Analysis and Simulation of Inductors and Transformers for ICs*,
Niknejad / Meyer, UC Berkeley, 1999-2000), reverse-engineered from the
original i386 ELF binary and re-implemented in self-contained C +
Python.

The numerical kernels (Grover / Greenhouse partial inductance,
Hammerstad-Jensen coupled-microstrip capacitance, Sommerfeld
substrate Green's function, 2/3-port Y/Z/S conversion, LAPACK-style
solver shims) live in :file:`vendor/recomp/` as plain C and are
exposed to Python through a thin Cython wrapper.  Geometry, network
analysis, sweeps, and optimisation are implemented in pure Python on
top of those kernels.

.. toctree::
   :maxdepth: 2
   :caption: User guide

   getting_started
   concepts
   api/index

.. toctree::
   :maxdepth: 1
   :caption: Project

   changelog


At a glance
-----------

.. code-block:: python

   import reasitic as ra

   # 1. Load a technology file (metal stack, substrate, oxide
   #    constants).
   tech = ra.parse_tech_file("BiCMOS.tek")

   # 2. Build a square-spiral inductor.
   spiral = ra.SquareSpiral(
       name="L1", metal="m3", length=200.0, width=8.0,
       spacing=2.0, turns=3.0, tech=tech,
   )

   # 3. Compute lumped figures of merit.
   L  = ra.compute_self_inductance(spiral, tech)        # H
   R  = ra.compute_dc_resistance(spiral, tech)          # Ω
   Q  = ra.metal_only_q(spiral, tech, freq_ghz=2.4)

   # 4. Pi-equivalent at one frequency (full substrate model).
   pi = ra.pi_model_at_freq(spiral, tech, freq_ghz=2.4)
   print(pi.L_eff_nH, pi.Q)

   # 5. Frequency sweep -> Touchstone S-parameter file.
   sweep = ra.two_port_sweep(spiral, tech,
                             freqs_ghz=[0.5, 1.0, 2.0, 5.0])
   ra.write_touchstone_file("L1.s2p", sweep)
