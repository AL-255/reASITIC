"""Public alias for :mod:`reasitic._geometry`.

The underscore-prefixed module is the wholesale port of the
ASITIC-binary-faithful geometry kernel from
``reASITIC-old1/src/reasitic/geometry.py`` (3447 lines).  Many
adjacent modules in this package (inductance / network / substrate
/ resistance) were authored against ``reasitic.geometry`` -- this
file re-exports the public surface under that historical name.
"""
from __future__ import annotations

from ._geometry import *  # noqa: F401,F403
from ._geometry import (  # noqa: F401  -- re-export key symbols
    Point,
    Segment,
    Polygon,
    Shape,
    layout_polygons,
    square_spiral,
    wire,
    capacitor,
    ring,
    polygon_spiral,
    multi_metal_square,
    symmetric_square,
    symmetric_polygon,
    transformer,
    transformer_3d,
    via,
    balun,
)
