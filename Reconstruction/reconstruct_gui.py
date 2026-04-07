import os
import sys
import random
import numpy as np
import open3d as o3d
import dearpygui.dearpygui as dpg
import cv2

from patch_state import PatchState, Rect
from projection import get_patch_axes, generate_texture, pixel_to_local2d
from export import export_augmented

MIN_SURFACE_AREA = 1.0
PROJECTION_THRESHOLD = 0.05
MAX_TEX_DIM = 512
FILL_RESOLUTION = 0.02

TRIANGLE_THRESHOLD = .01

NORMAL_VARIANCE_THRESHOLD_DEG = 60
COPLANARITY_DEG = 75
OUTLIER_RATIO = 0.75
MIN_PLANE_EDGE_LENGTH = 0
MIN_NUM_POINTS = 0
KNN = 30

tex_iter = 0


def patch_area(obox):
    sides = sorted(obox.extent)
    return sides[1] * sides[2]


def compute_tex_dims(obox):
    _, _, _, u_ext, v_ext, _ = get_patch_axes(obox)
    aspect = u_ext / v_ext
    if aspect >= 1.0:
        tw = MAX_TEX_DIM
        th = max(1, int(MAX_TEX_DIM / aspect))
    else:
        th = MAX_TEX_DIM
        tw = max(1, int(MAX_TEX_DIM * aspect))
    return tw, th


def load_patches(project_name):
    ply_dir = os.path.join(".", "points", "original", project_name)
    ply_file = os.path.join(ply_dir, os.listdir(ply_dir)[0])
    pcd = o3d.io.read_point_cloud(ply_file)
    pcd.estimate_normals()

    oboxes = pcd.detect_planar_patches(
        normal_variance_threshold_deg=NORMAL_VARIANCE_THRESHOLD_DEG,
        coplanarity_deg=COPLANARITY_DEG,
        outlier_ratio=OUTLIER_RATIO,
        min_plane_edge_length=MIN_PLANE_EDGE_LENGTH,
        min_num_points=MIN_NUM_POINTS,
        search_param=o3d.geometry.KDTreeSearchParamKNN(knn=KNN))

    oboxes = sorted(oboxes, key=patch_area, reverse=True)
    oboxes = [ob for ob in oboxes if patch_area(ob) >= MIN_SURFACE_AREA]

    patch_states = []
    for i, obox in enumerate(oboxes):
        u_axis, v_axis, normal, u_ext, v_ext, thin = get_patch_axes(obox)
        n = normal / np.linalg.norm(normal)
        d = -n @ np.asarray(obox.center)
        plane_model = np.array([n[0], n[1], n[2], d])
        area = patch_area(obox)
        tw, th = compute_tex_dims(obox)
        tex = generate_texture(pcd, obox, PROJECTION_THRESHOLD, tw, th)
        ps = PatchState(
            index=i,
            obox=obox,
            plane_model=plane_model,
            surface_area=area,
            texture=tex,
            tex_w=tw,
            tex_h=th,
        )
        patch_states.append(ps)
        print(f"Loaded patch {i}: area={area:.2f} tex={tw}x{th}")

    return pcd, patch_states


def texture_to_bgra(tex):
    img = (tex * 255).astype(np.uint8)
    bgra = cv2.cvtColor(img[:, :, :3], cv2.COLOR_RGB2BGRA)
    bgra[:, :, 3] = 255
    return bgra


def bgra_to_dpg(img):
    rgba = cv2.cvtColor(img, cv2.COLOR_BGRA2RGBA)
    return (rgba.astype(np.float32) / 255.0).flatten()


def rotate_image(img, rotation):
    if rotation == 1:
        return cv2.rotate(img, cv2.ROTATE_90_CLOCKWISE)
    elif rotation == 2:
        return cv2.rotate(img, cv2.ROTATE_180)
    elif rotation == 3:
        return cv2.rotate(img, cv2.ROTATE_90_COUNTERCLOCKWISE)
    return img


def unrotate_point(rx, ry, orig_w, orig_h, rotation):
    if rotation == 0:
        return rx, ry
    elif rotation == 1:
        return ry, orig_h - 1 - rx
    elif rotation == 2:
        return orig_w - 1 - rx, orig_h - 1 - ry
    elif rotation == 3:
        return orig_w - 1 - ry, rx
    return rx, ry


class App:
    def __init__(self, pcd, patch_states, project_name):
        self.pcd = pcd
        self.patches = patch_states
        self.project_name = project_name
        self.current_index = 0
        self.quad_points = []
        self.is_drawing = False
        self.rotation = 0

    def current_patch(self):
        if 0 <= self.current_index < len(self.patches):
            return self.patches[self.current_index]
        return None

    def composite_canvas(self):
        global tex_iter
        ps = self.current_patch()
        if ps is None:
            return

        base = texture_to_bgra(ps.texture).astype(np.float32)
        overlay = np.zeros_like(base, dtype=np.float32)

        for rect in ps.cutout_rects:
            pts = np.array(rect.pixel, dtype=np.int32)
            cv2.fillPoly(overlay, [pts], rect.color)

        alpha = overlay[:, :, 3:4] / 255.0
        base[:, :, :3] = base[:, :, :3] * (1 - alpha) + overlay[:, :, :3] * alpha
        base[:, :, 3] = 255
        result = base.astype(np.uint8)

        result = rotate_image(result, self.rotation)

        if self.quad_points:
            for pt in self.quad_points:
                cv2.circle(result, pt, 4, (0, 255, 0, 255), -1)
            if len(self.quad_points) > 1:
                for j in range(len(self.quad_points) - 1):
                    cv2.line(result, self.quad_points[j], self.quad_points[j + 1], (0, 255, 0, 255), 2)

        h, w = result.shape[:2]
        old_tag = f"__canvas_tex_{tex_iter}"
        if dpg.does_item_exist(old_tag):
            dpg.delete_item(old_tag)
        tex_iter += 1
        new_tag = f"__canvas_tex_{tex_iter}"
        data = bgra_to_dpg(result)
        dpg.add_dynamic_texture(parent="__texture_container", tag=new_tag, width=w, height=h, default_value=data)
        dpg.configure_item("__canvas_image", texture_tag=new_tag, width=w, height=h)

    def displayed_dims(self):
        ps = self.current_patch()
        if ps is None:
            return MAX_TEX_DIM, MAX_TEX_DIM
        w, h = ps.tex_w, ps.tex_h
        if self.rotation in (1, 3):
            return h, w
        return w, h

    def update_info_panel(self):
        ps = self.current_patch()
        if ps is None:
            dpg.set_value("info_text", "No patches loaded.")
            return
        drawing_status = "DRAWING" if self.is_drawing else "IDLE"
        rot_labels = ["0", "90", "180", "270"]
        lines = [
            f"Patch: {ps.index + 1} / {len(self.patches)}",
            f"Area: {ps.surface_area:.3f}",
            f"Status: {ps.status}",
            f"Cutouts: {len(ps.cutout_rects)}",
            f"Rotation: {rot_labels[self.rotation]}",
            f"Mode: {drawing_status}",
            f"Points: {len(self.quad_points)}/4",
        ]
        dpg.set_value("info_text", "\n".join(lines))

    def update_patch_list(self):
        status_chars = {"pending": "-", "accepted": "W", "floor": "F", "ceiling": "C", "rejected": "X"}
        for i, ps in enumerate(self.patches):
            tag = f"plist_{i}"
            sc = status_chars.get(ps.status, "?")
            label = f"#{i} | {ps.surface_area:.2f} | [{sc}]"
            dpg.set_value(tag, label)
            if ps.status in ("accepted", "floor", "ceiling"):
                dpg.bind_item_theme(tag, "theme_accepted")
            elif ps.status == "rejected":
                dpg.bind_item_theme(tag, "theme_rejected")
            else:
                dpg.bind_item_theme(tag, "theme_pending")

    def navigate_to(self, idx):
        if 0 <= idx < len(self.patches):
            self.current_index = idx
            self.quad_points = []
            self.is_drawing = False
            self.rotation = 0
            self.composite_canvas()
            self.update_info_panel()
            self.update_patch_list()

    def _set_status_and_advance(self, status):
        ps = self.current_patch()
        if ps:
            ps.status = status
            if self.current_index < len(self.patches) - 1:
                self.navigate_to(self.current_index + 1)
            else:
                self.update_info_panel()
                self.update_patch_list()

    def on_accept(self):
        self._set_status_and_advance("accepted")

    def on_accept_floor(self):
        self._set_status_and_advance("floor")

    def on_accept_ceiling(self):
        self._set_status_and_advance("ceiling")

    def on_reject(self):
        self._set_status_and_advance("rejected")

    def on_prev(self):
        self.navigate_to(self.current_index - 1)

    def on_next(self):
        self.navigate_to(self.current_index + 1)

    def on_rotate(self):
        self.rotation = (self.rotation + 1) % 4
        self.quad_points = []
        self.is_drawing = False
        ps = self.current_patch()
        if ps:
            ps.cutout_rects.clear()
        self.composite_canvas()
        self.update_info_panel()

    def on_draw_cutout(self):
        self.is_drawing = True
        self.quad_points = []
        self.update_info_panel()

    def on_clear_cutouts(self):
        ps = self.current_patch()
        if ps:
            ps.cutout_rects.clear()
            self.quad_points = []
            self.is_drawing = False
            self.composite_canvas()
            self.update_info_panel()

    def on_export(self):
        mesh_path = export_augmented(self.pcd, self.patches, self.project_name, FILL_RESOLUTION, PROJECTION_THRESHOLD, TRIANGLE_THRESHOLD)
        dpg.destroy_context()

        

    def canvas_click(self, sender, app_data):
        if not self.is_drawing:
            return

        ps = self.current_patch()
        if ps is None:
            return

        disp_w, disp_h = self.displayed_dims()

        mouse_pos = dpg.get_mouse_pos(local=False)
        img_pos = dpg.get_item_rect_min("__canvas_image")
        x = max(0, min(int(mouse_pos[0] - img_pos[0]), disp_w - 1))
        y = max(0, min(int(mouse_pos[1] - img_pos[1]), disp_h - 1))
        pt = (x, y)

        self.quad_points.append(pt)
        self.composite_canvas()
        self.update_info_panel()

        if len(self.quad_points) == 4:
            r = random.randint(50, 255)
            g = random.randint(50, 255)
            b = random.randint(50, 255)

            orig_pixel_pts = []
            for dx, dy in self.quad_points:
                ox, oy = unrotate_point(dx, dy, ps.tex_w, ps.tex_h, self.rotation)
                orig_pixel_pts.append((ox, oy))

            local_pts = []
            for px, py in orig_pixel_pts:
                u, v = pixel_to_local2d(px, py, ps.obox, ps.tex_w, ps.tex_h)
                local_pts.append((u, v))

            rect = Rect(pixel=orig_pixel_pts, local_2d=local_pts)
            rect.color = (b, g, r, 100)
            ps.cutout_rects.append(rect)

            self.quad_points = []
            self.is_drawing = False
            self.composite_canvas()
            self.update_info_panel()

    def run(self):
        dpg.create_context()
        dpg.create_viewport(title="Patch Review Tool", width=1000, height=700)

        dpg.add_texture_registry(tag="__texture_container")

        ps = self.patches[0] if self.patches else None
        if ps:
            init_img = texture_to_bgra(ps.texture)
            init_data = bgra_to_dpg(init_img)
            init_w, init_h = ps.tex_w, ps.tex_h
        else:
            init_w, init_h = MAX_TEX_DIM, MAX_TEX_DIM
            init_data = [0.15] * (init_w * init_h * 4)

        global tex_iter
        dpg.add_dynamic_texture(
            parent="__texture_container",
            tag=f"__canvas_tex_{tex_iter}",
            width=init_w,
            height=init_h,
            default_value=init_data)

        with dpg.theme(tag="theme_accepted"):
            with dpg.theme_component(dpg.mvText):
                dpg.add_theme_color(dpg.mvThemeCol_Text, (100, 255, 100))

        with dpg.theme(tag="theme_rejected"):
            with dpg.theme_component(dpg.mvText):
                dpg.add_theme_color(dpg.mvThemeCol_Text, (255, 100, 100))

        with dpg.theme(tag="theme_pending"):
            with dpg.theme_component(dpg.mvText):
                dpg.add_theme_color(dpg.mvThemeCol_Text, (180, 180, 180))

        with dpg.window(tag="primary", label="Patch Review Tool"):
            with dpg.group(horizontal=True):
                with dpg.child_window(width=200, tag="left_panel"):
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="<- Prev", callback=lambda: self.on_prev())
                        dpg.add_button(label="Next ->", callback=lambda: self.on_next())
                    dpg.add_separator()
                    dpg.add_button(label="Accept Wall", callback=lambda: self.on_accept())
                    dpg.add_button(label="Accept Floor", callback=lambda: self.on_accept_floor())
                    dpg.add_button(label="Accept Ceiling", callback=lambda: self.on_accept_ceiling())
                    dpg.add_button(label="Reject", callback=lambda: self.on_reject())
                    dpg.add_separator()
                    dpg.add_button(label="Draw Cutout", callback=lambda: self.on_draw_cutout())
                    dpg.add_button(label="Clear Cutouts", callback=lambda: self.on_clear_cutouts())
                    dpg.add_button(label="Rotate 90", callback=lambda: self.on_rotate())
                    dpg.add_separator()
                    dpg.add_button(label="Finish & Export", callback=lambda: self.on_export())
                    dpg.add_separator()
                    dpg.add_text("", tag="info_text")

                with dpg.child_window(width=MAX_TEX_DIM + 16, no_scrollbar=True, tag="center_panel"):
                    dpg.add_image(f"__canvas_tex_{tex_iter}", tag="__canvas_image",
                                  width=init_w, height=init_h)

                with dpg.child_window(width=200, tag="right_panel"):
                    dpg.add_text("Patches")
                    dpg.add_separator()
                    status_chars = {"pending": "-", "accepted": "W", "floor": "F", "ceiling": "C", "rejected": "X"}
                    for i, p in enumerate(self.patches):
                        sc = status_chars.get(p.status, "?")
                        label = f"#{i} | {p.surface_area:.2f} | [{sc}]"
                        dpg.add_text(label, tag=f"plist_{i}")

        with dpg.item_handler_registry(tag="__canvas_handler"):
            dpg.add_item_clicked_handler(callback=self.canvas_click)
        dpg.bind_item_handler_registry("__canvas_image", "__canvas_handler")

        dpg.set_primary_window("primary", True)
        dpg.setup_dearpygui()
        dpg.show_viewport()

        self.navigate_to(0)

        dpg.start_dearpygui()
        dpg.destroy_context()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <project_name>")
        sys.exit(1)

    project_name = sys.argv[1]
    print(f"Loading point cloud for project '{project_name}'...")
    pcd, patch_states = load_patches(project_name)
    print(f"{len(patch_states)} patches passed area filter (>= {MIN_SURFACE_AREA})")

    if not patch_states:
        print("No patches to review.")
        sys.exit(0)

    app = App(pcd, patch_states, project_name)
    app.run()
