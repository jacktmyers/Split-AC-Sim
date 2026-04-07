from dataclasses import dataclass, field
import numpy as np
import open3d as o3d


@dataclass
class Rect:
    pixel: list
    local_2d: list


@dataclass
class PatchState:
    index: int
    obox: o3d.geometry.OrientedBoundingBox
    plane_model: np.ndarray
    surface_area: float
    texture: np.ndarray
    tex_w: int = 512
    tex_h: int = 512
    cutout_rects: list = field(default_factory=list)
    status: str = "pending"
