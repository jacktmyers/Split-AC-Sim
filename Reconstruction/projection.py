import numpy as np
import open3d as o3d


def get_patch_axes(obox):
    R = np.asarray(obox.R)
    extent = np.asarray(obox.extent)
    thin_axis = np.argmin(extent)
    uv_axes = [i for i in range(3) if i != thin_axis]
    u_axis = R[:, uv_axes[0]]
    v_axis = R[:, uv_axes[1]]
    u_extent = extent[uv_axes[0]]
    v_extent = extent[uv_axes[1]]
    normal = R[:, thin_axis]
    return u_axis, v_axis, normal, u_extent, v_extent, thin_axis


def world_to_local2d(points, obox):
    u_axis, v_axis, _, _, _, _ = get_patch_axes(obox)
    center = np.asarray(obox.center)
    diff = points - center
    u = diff @ u_axis
    v = diff @ v_axis
    return np.column_stack([u, v])


def local2d_to_pixel(pts_2d, obox, tex_w, tex_h):
    _, _, _, u_extent, v_extent, _ = get_patch_axes(obox)
    half_u = u_extent / 2.0
    half_v = v_extent / 2.0
    px = ((pts_2d[:, 0] + half_u) / u_extent * (tex_w - 1)).astype(int)
    py = ((pts_2d[:, 1] + half_v) / v_extent * (tex_h - 1)).astype(int)
    return np.column_stack([px, py])


def pixel_to_local2d(px, py, obox, tex_w, tex_h):
    _, _, _, u_extent, v_extent, _ = get_patch_axes(obox)
    half_u = u_extent / 2.0
    half_v = v_extent / 2.0
    u = (px / (tex_w - 1)) * u_extent - half_u
    v = (py / (tex_h - 1)) * v_extent - half_v
    return u, v


def generate_texture(pcd, obox, projection_threshold, tex_w, tex_h):
    u_axis, v_axis, normal, u_extent, v_extent, _ = get_patch_axes(obox)
    center = np.asarray(obox.center)
    points = np.asarray(pcd.points)
    colors = np.asarray(pcd.colors)

    dists = (points - center) @ normal
    mask = np.abs(dists) < projection_threshold
    near_points = points[mask]
    near_colors = colors[mask] if len(colors) > 0 else np.full((mask.sum(), 3), 0.5)

    if len(near_points) == 0:
        texture = np.full((tex_h, tex_w, 4), 0.15, dtype=np.float32)
        texture[:, :, 3] = 1.0
        return texture

    pts_2d = world_to_local2d(near_points, obox)
    pixels = local2d_to_pixel(pts_2d, obox, tex_w, tex_h)

    px = np.clip(pixels[:, 0], 0, tex_w - 1)
    py = np.clip(pixels[:, 1], 0, tex_h - 1)

    texture = np.full((tex_h, tex_w, 4), 0.15, dtype=np.float32)
    texture[:, :, 3] = 1.0
    color_accum = np.zeros((tex_h, tex_w, 3), dtype=np.float64)
    count = np.zeros((tex_h, tex_w), dtype=np.int32)

    for i in range(len(px)):
        x, y = px[i], py[i]
        color_accum[y, x] += near_colors[i]
        count[y, x] += 1

    occupied = count > 0
    for c in range(3):
        texture[:, :, c][occupied] = (color_accum[:, :, c][occupied] / count[occupied]).astype(np.float32)

    return texture
