import os
import numpy as np
import open3d as o3d
from matplotlib.path import Path
from projection import get_patch_axes, world_to_local2d


def generate_fill_points(patch_state, pcd, fill_resolution, projection_threshold):
    obox = patch_state.obox
    u_axis, v_axis, normal, u_extent, v_extent, _ = get_patch_axes(obox)
    center = np.asarray(obox.center)

    half_u = u_extent / 2.0
    half_v = v_extent / 2.0
    u_steps = np.arange(-half_u, half_u, fill_resolution)
    v_steps = np.arange(-half_v, half_v, fill_resolution)
    uu, vv = np.meshgrid(u_steps, v_steps)
    grid_2d = np.column_stack([uu.ravel(), vv.ravel()])

    for rect in patch_state.cutout_rects:
        quad_path = Path(rect.local_2d)
        inside = quad_path.contains_points(grid_2d)
        grid_2d = grid_2d[~inside]

    if len(grid_2d) == 0:
        return np.empty((0, 3)), np.empty((0, 3))

    world_points = center + grid_2d[:, 0:1] * u_axis + grid_2d[:, 1:2] * v_axis

    points = np.asarray(pcd.points)
    colors = np.asarray(pcd.colors)
    dists = (points - center) @ normal
    mask = np.abs(dists) < projection_threshold
    near_colors = colors[mask] if len(colors) > 0 else np.full((mask.sum(), 3), 0.5)

    if len(near_colors) > 0:
        avg_color = near_colors.mean(axis=0)
    else:
        avg_color = np.array([0.5, 0.5, 0.5])

    fill_colors = np.tile(avg_color, (len(world_points), 1))
    return world_points, fill_colors


def compute_orientation_matrix(patch_states):
    normals = []
    for ps in patch_states:
        if ps.status in ("floor", "ceiling"):
            _, _, normal, _, _, _ = get_patch_axes(ps.obox)
            n = normal / np.linalg.norm(normal)
            normals.append(n)

    if not normals:
        return None

    avg_normal = np.mean(normals, axis=0)
    avg_normal = avg_normal / np.linalg.norm(avg_normal)

    z_target = np.array([0.0, 0.0, 1.0])

    if np.abs(np.dot(avg_normal, z_target)) > 0.999:
        if np.dot(avg_normal, z_target) < 0:
            return np.diag([1.0, 1.0, -1.0])
        return None

    v = np.cross(avg_normal, z_target)
    s = np.linalg.norm(v)
    c = np.dot(avg_normal, z_target)

    vx = np.array([
        [0, -v[2], v[1]],
        [v[2], 0, -v[0]],
        [-v[1], v[0], 0]
    ])

    R = np.eye(3) + vx + vx @ vx * ((1 - c) / (s * s))
    return R


def compute_mesh(pcd, triangle_threshold):
    center = pcd.get_center()
    pcd.estimate_normals()
    pcd.orient_normals_towards_camera_location(center)


    with o3d.utility.VerbosityContextManager(
            o3d.utility.VerbosityLevel.Debug) as cm:
        mesh, densities = o3d.geometry.TriangleMesh.create_from_point_cloud_poisson(
            pcd, depth=8)

    # vertices_to_remove = densities < np.quantile(densities, 0.2)
    # mesh.remove_vertices_by_mask(vertices_to_remove)

    if pcd.has_colors():
        pcd_tree = o3d.geometry.KDTreeFlann(pcd)
        mesh_colors = []
        for vertex in mesh.vertices:
            _, idx, dists = pcd_tree.search_radius_vector_3d(vertex, 0.01)  # tune radius
            if len(idx) == 0:
                # fallback to nearest neighbor if no points in radius
                _, idx, dists = pcd_tree.search_knn_vector_3d(vertex, 1)
                dists = list(dists)
            colors = np.asarray(pcd.colors)[idx]
            weights = 1.0 / (np.array(dists) + 1e-6)
            weights /= weights.sum()
            mesh_colors.append((colors * weights[:, np.newaxis]).sum(axis=0))
        mesh.vertex_colors = o3d.utility.Vector3dVector(np.array(mesh_colors))

    triangle_clusters, cluster_n_triangles, _ = mesh.cluster_connected_triangles()
    triangle_clusters = np.asarray(triangle_clusters)
    cluster_n_triangles = np.asarray(cluster_n_triangles)

    min_tris = len(mesh.triangles) * triangle_threshold

    remove_mask = cluster_n_triangles[triangle_clusters] < min_tris
    mesh.remove_triangles_by_mask(remove_mask)
    mesh.remove_unreferenced_vertices()

    return mesh



def export_augmented(pcd, patch_states, project_name, fill_resolution, projection_threshold, triangle_threshold):
    all_synth_points = []
    all_synth_colors = []
    accepted = 0
    rejected = 0
    floors = 0
    ceilings = 0


    for ps in patch_states:
        if ps.status in ("accepted", "floor", "ceiling"):
            if ps.status == "floor":
                floors += 1
            elif ps.status == "ceiling":
                ceilings += 1
            else:
                accepted += 1
            pts, cols = generate_fill_points(ps, pcd, fill_resolution, projection_threshold)
            if len(pts) > 0:
                all_synth_points.append(pts)
                all_synth_colors.append(cols)
        elif ps.status == "rejected":
            rejected += 1

    # pcd = pcd.voxel_down_sample(voxel_size=0.01)

    orig_points = np.asarray(pcd.points)
    orig_colors = np.asarray(pcd.colors) if pcd.has_colors() else np.full((len(orig_points), 3), 0.5)

    if all_synth_points:
        synth_points = np.vstack(all_synth_points)
        synth_colors = np.vstack(all_synth_colors)
        merged_points = np.vstack([orig_points, synth_points])
        merged_colors = np.vstack([orig_colors, synth_colors])
        num_synth = len(synth_points)
    else:
        merged_points = orig_points
        merged_colors = orig_colors
        num_synth = 0

    R = compute_orientation_matrix(patch_states)
    if R is not None:
        centroid = merged_points.mean(axis=0)
        merged_points = (merged_points - centroid) @ R.T + centroid
        print(f"Oriented point cloud so Z is parallel to floor/ceiling")

    merged = o3d.geometry.PointCloud()
    merged.points = o3d.utility.Vector3dVector(merged_points)
    merged.colors = o3d.utility.Vector3dVector(merged_colors)

    out_dir = os.path.join(".", "points", "cleaned", project_name)
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "cleaned.ply")
    if os.path.exists(out_path):
        os.remove(out_path)
    o3d.io.write_point_cloud(out_path, merged)

    print(f"{accepted} walls, {floors} floors, {ceilings} ceilings accepted, {rejected} rejected, {num_synth} synthetic points added")
    print(f"Augmented point cloud saved to {out_path}")

    mesh_dir = os.path.join(".", "mesh", project_name)
    os.makedirs(mesh_dir, exist_ok=True)

    mesh_path = os.path.join(mesh_dir, "mesh.ply")
    if os.path.exists(mesh_path):
        os.remove(mesh_path)
    o3d.io.write_triangle_mesh(mesh_path, compute_mesh(merged, triangle_threshold))
    print(f"Possion mesh saved to {mesh_path}")

    
    return out_path
