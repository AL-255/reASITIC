"""reasitic -- modern Python interface to the ASITIC numerical kernels.

reasitic is a re-port of `ASITIC
<http://rfic.eecs.berkeley.edu/~niknejad/asitic.html>`_ (Analysis and
Simulation of Inductors and Transformers for ICs, Niknejad / Meyer,
UC Berkeley, 1999-2000).  The numerical kernels are wrapped via
Cython from the hand-translated C re-implementation that lives in
``vendor/recomp/``; geometry, network analysis, and optimisation are
implemented in pure Python on top of those kernels.

Public API at a glance
======================

Subpackages:

* :mod:`reasitic.kernel` -- Cython-wrapped C math kernels (Grover,
  Hammerstad-Jensen, vec3 helpers, complex arithmetic).
* :mod:`reasitic.shapes` -- class-based shape builders
  (:class:`Wire`, :class:`SquareSpiral`, ..., :class:`Balun`).
* :mod:`reasitic.geometry` -- functional builders + the
  :class:`Point` / :class:`Polygon` / :class:`Segment` /
  :class:`Shape` dataclasses.
* :mod:`reasitic.inductance` -- partial inductance, filament MNA,
  eddy correction, Grover / Hoer-Love kernels.
* :mod:`reasitic.resistance` -- DC and frequency-dependent
  (skin effect) resistance.
* :mod:`reasitic.substrate` -- substrate Green's function +
  shunt / coupled-microstrip capacitance.
* :mod:`reasitic.network` -- 2/3-port Y/Z/S conversions,
  Pi-equivalent extraction, frequency sweeps, Touchstone I/O.
* :mod:`reasitic.optimise` -- inductor-geometry optimisation
  driven by :mod:`scipy.optimize`.
* :mod:`reasitic.exports` -- CIF / GDSII / Tek / Sonnet / SPICE /
  FastHenry writers (and readers where round-trip is supported).
* :mod:`reasitic.cli` -- the ASITIC-style REPL driver.

Top-level helper modules:

* :mod:`reasitic.tech` -- tech-file parsing / writing
  (:func:`parse_tech_file`, :class:`Tech`).
* :mod:`reasitic.info` -- shape introspection (segments, areas,
  LR matrices).
* :mod:`reasitic.quality` -- quality-factor calculators.
* :mod:`reasitic.report` -- design-report builders.
* :mod:`reasitic.plot` -- matplotlib helpers (optional).
* :mod:`reasitic.persistence` -- session save / restore.
* :mod:`reasitic.units` -- unit conversions (um, GHz, ...).

Minimal example
===============

.. code-block:: python

    import reasitic as ra

    tech = ra.parse_tech_file("BiCMOS.tek")
    spiral = ra.SquareSpiral(
        tech=tech, layer="m3", length=200, width=8,
        spacing=2, turns=3,
    )
    L = ra.compute_self_inductance(spiral, tech)
    R = ra.compute_dc_resistance(spiral, tech)
    Q = ra.metal_only_q(spiral, tech, freq_ghz=2.4)
    print(f"L = {L*1e9:.2f} nH, R = {R*1e3:.1f} mOhm, Q = {Q:.1f}")
"""

from ._version import __version__

# ---------------------------------------------------------------------------
# Subpackages (always available as attributes of the top-level package).
# ---------------------------------------------------------------------------
# Eager-import the leaf modules and subpackages that do NOT reach back into
# the top-level reasitic namespace.  Subpackages such as ``optimise`` and
# ``cli`` import names like ``Shape`` from the top level and therefore have
# to be loaded lazily (after the top-level ``from .geometry import ...``
# bindings have been established).
from . import (
    geometry as geometry,
    info,
    kernel,
    persistence,
    plot,
    quality,
    report,
    tech,
    units,
)


_LAZY_SUBPACKAGES = frozenset(
    {
        "cli",
        "exports",
        "inductance",
        "network",
        "optimise",
        "resistance",
        "shapes",
        "substrate",
    }
)


def __getattr__(name: str):
    """Lazy-load subpackages that re-import from the top level.

    Several subpackages (``optimise``, ``cli``, ``exports`` ...) import
    names like :class:`Shape` from :mod:`reasitic` itself.  Eagerly
    importing them during package initialisation triggers a circular
    import because those names have not yet been bound.  Returning
    them via :pep:`562` lazy attribute access defers the import until
    after the top-level ``from`` re-exports below have completed.
    """
    if name in _LAZY_SUBPACKAGES:
        import importlib

        module = importlib.import_module(f".{name}", __name__)
        globals()[name] = module
        return module
    raise AttributeError(f"module 'reasitic' has no attribute {name!r}")


def __dir__():
    return sorted(set(globals()) | _LAZY_SUBPACKAGES)

# ---------------------------------------------------------------------------
# Class-based shape builders (recommended entry point for new code).
# ---------------------------------------------------------------------------
from .shapes import (
    Balun,
    Capacitor,
    MMSQ,
    PolygonSpiral,
    Ring,
    SquareSpiral,
    SymmetricPolygon,
    SymmetricSquare,
    Transformer,
    Wire,
)

# ---------------------------------------------------------------------------
# Functional geometry builders + primitives.
# ---------------------------------------------------------------------------
from .geometry import (
    Point,
    Polygon,
    Segment,
    Shape,
    balun,
    capacitor,
    multi_metal_square,
    polygon_spiral,
    ring,
    square_spiral,
    symmetric_polygon,
    symmetric_square,
    transformer,
    transformer_3d,
    via,
    wire,
)

# ---------------------------------------------------------------------------
# Numerical helpers (inductance / resistance / quality).
# ---------------------------------------------------------------------------
from .inductance import (
    compute_mutual_inductance,
    compute_self_inductance,
    coupling_coefficient,
)
from .resistance import (
    compute_ac_resistance,
    compute_dc_resistance,
    three_class_resistance,
)
from .quality import metal_only_q

# ---------------------------------------------------------------------------
# Spiral / wire layout helpers reused by optimisers.
# ---------------------------------------------------------------------------
from .spiral_helpers import (
    segment_pair_distance_metric,
    spiral_max_n,
    spiral_radius_for_n,
    spiral_turn_position,
    wire_position_periodic_fold,
)

# ---------------------------------------------------------------------------
# Tech / persistence / info helpers.
# ---------------------------------------------------------------------------
from .tech import (
    Tech,
    parse_tech,
    parse_tech_file,
    write_tech,
    write_tech_file,
)
from .info import format_lr_matrix, format_segments, list_segments, lr_matrix, metal_area
from .quality import metal_only_q as metal_only_q  # re-export for completeness
from .report import DesignReport, FreqPointReport, design_report
from .persistence import (
    load_session,
    load_viewport,
    save_session,
    shape_from_dict,
    shape_to_dict,
    tech_from_dict,
    tech_to_dict,
)

# ---------------------------------------------------------------------------
# Network analysis (the most-requested high-level surface).
# ---------------------------------------------------------------------------
from .network import (
    NetworkSweep,
    PiModel,
    PiResult,
    TouchstoneFile,
    TouchstonePoint,
    TransformerAnalysis,
    pi_equivalent,
    pi_model_at_freq,
    read_touchstone_file,
    self_resonance,
    spiral_y_at_freq,
    two_port_sweep,
    write_touchstone_file,
    y_to_s,
    y_to_z,
    z_to_y,
)

# ---------------------------------------------------------------------------
# Substrate model.
# ---------------------------------------------------------------------------
from .substrate import (
    integrate_green_kernel,
    parallel_plate_cap_per_area,
    shape_pi_capacitances,
    shape_shunt_admittance,
    shape_shunt_capacitance,
)

# ---------------------------------------------------------------------------
# Optimisation.
# ---------------------------------------------------------------------------
from .optimise import (
    OptResult,
    batch_opt_square,
    optimise_area_square_spiral,
    optimise_polygon_spiral,
    optimise_square_spiral,
    optimise_symmetric_square,
    sweep_square_spiral,
)

# ---------------------------------------------------------------------------
# Exports.
# ---------------------------------------------------------------------------
from .exports import (
    read_cif_file,
    read_gds_file,
    read_sonnet_file,
    write_cif_file,
    write_fasthenry_file,
    write_gds_file,
    write_sonnet_file,
    write_spice_broadband_file,
    write_spice_subckt_file,
    write_tek_file,
)


__all__ = [
    "__version__",
    # Subpackages
    "cli",
    "exports",
    "geometry",
    "inductance",
    "info",
    "kernel",
    "network",
    "optimise",
    "persistence",
    "plot",
    "quality",
    "report",
    "resistance",
    "shapes",
    "substrate",
    "units",
    # Class-based shapes
    "Balun",
    "Capacitor",
    "MMSQ",
    "PolygonSpiral",
    "Ring",
    "SquareSpiral",
    "SymmetricPolygon",
    "SymmetricSquare",
    "Transformer",
    "Wire",
    # Geometry primitives
    "Point",
    "Polygon",
    "Segment",
    "Shape",
    # Functional builders
    "balun",
    "capacitor",
    "multi_metal_square",
    "polygon_spiral",
    "ring",
    "square_spiral",
    "symmetric_polygon",
    "symmetric_square",
    "transformer",
    "transformer_3d",
    "via",
    "wire",
    # Inductance / resistance / quality
    "compute_ac_resistance",
    "compute_dc_resistance",
    "compute_mutual_inductance",
    "compute_self_inductance",
    "coupling_coefficient",
    "metal_only_q",
    "three_class_resistance",
    # Spiral helpers
    "segment_pair_distance_metric",
    "spiral_max_n",
    "spiral_radius_for_n",
    "spiral_turn_position",
    "wire_position_periodic_fold",
    # Tech / info / persistence / reports
    "DesignReport",
    "FreqPointReport",
    "Tech",
    "design_report",
    "format_lr_matrix",
    "format_segments",
    "list_segments",
    "load_session",
    "load_viewport",
    "lr_matrix",
    "metal_area",
    "parse_tech",
    "parse_tech_file",
    "save_session",
    "shape_from_dict",
    "shape_to_dict",
    "tech_from_dict",
    "tech_to_dict",
    "write_tech",
    "write_tech_file",
    # Network analysis
    "NetworkSweep",
    "PiModel",
    "PiResult",
    "TouchstoneFile",
    "TouchstonePoint",
    "TransformerAnalysis",
    "pi_equivalent",
    "pi_model_at_freq",
    "read_touchstone_file",
    "self_resonance",
    "spiral_y_at_freq",
    "two_port_sweep",
    "write_touchstone_file",
    "y_to_s",
    "y_to_z",
    "z_to_y",
    # Substrate
    "integrate_green_kernel",
    "parallel_plate_cap_per_area",
    "shape_pi_capacitances",
    "shape_shunt_admittance",
    "shape_shunt_capacitance",
    # Optimisation
    "OptResult",
    "batch_opt_square",
    "optimise_area_square_spiral",
    "optimise_polygon_spiral",
    "optimise_square_spiral",
    "optimise_symmetric_square",
    "sweep_square_spiral",
    # Exports
    "read_cif_file",
    "read_gds_file",
    "read_sonnet_file",
    "write_cif_file",
    "write_fasthenry_file",
    "write_gds_file",
    "write_sonnet_file",
    "write_spice_broadband_file",
    "write_spice_subckt_file",
    "write_tek_file",
]
