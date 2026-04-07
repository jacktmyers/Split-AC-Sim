import open3d as o3d
import sys

mesh = o3d.io.read_triangle_mesh(sys.argv[1])
center = mesh.get_center()
o3d.visualization.draw_geometries([mesh],
                                zoom=0.664,
                                front=[0.0, 0.0, -1.0],
                                lookat=center.tolist(),
                                up=[0.0, 1.0, 0.0])