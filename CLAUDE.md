# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

This project uses CMake with the Visual Studio / Ninja generator. The configured preset is **x64-Debug** (see `CMakeSettings.json`).

From Visual Studio: open the folder, select the `x64-Debug` configuration, and build the desired target.

From the command line (after CMake configuration):
```
ninja CustomRoomSim   # CGT537 custom simulation
ninja SPHSimulator    # Full interactive simulator
ninja                 # All targets
```

Output binaries land in `out/build/x64-Debug/bin/`. Shaders are expected at `<exePath>/resources/shaders`.

CMake fetches external dependencies (CompactNSearch, PositionBasedDynamics, Discregrid, GenericParameters) at configure time via `CMake/SetUpExternalProjects.cmake`.

## Architecture

### Two executables, one shared library

- **`SPHSimulator`** — the upstream full interactive simulator. Uses `MiniGL` for camera/input and `Simulator_GUI_imgui` for the UI overlay.
- **`CustomRoomSim`** — the CGT537 project entry point (`CGT537/Custom.cpp`). Calls `base->setUseGUI(false)` and drives its own GLFW loop; rendering is handled by `CustomRender()` in `CGT537/CustomRender.cpp`.

Both link against **`SimulatorBase`** (a static library built from `Simulator/`), which owns scene loading, simulation stepping, fluid/boundary model management, and exporters.

### Core SPH library (`SPlisHSPlasH/`)

Contains the pressure solvers (WCSPH, PCISPH, PBF, IISPH, DFSPH, PF, ICSPH), `FluidModel`, `EmitterSystem`, and the `Simulation` singleton (`Simulation::getCurrent()`). Scene configuration is accessed via `SceneConfiguration::getCurrent()->getScene()`.

### GUI layers (`Simulator/GUI/`)

- `OpenGL/` — `MiniGL` handles window/input/camera for `SPHSimulator`; `Simulator_OpenGL` provides mesh and particle point-sprite shaders.
- `imgui/` — `Simulator_GUI_imgui` wraps ImGui menus and parameter controls. Its input callbacks return `ImGui::GetIO().WantCaptureMouse` / `WantCaptureKeyboard`, which blocks MiniGL camera input when the UI is focused.

### CGT537 custom rendering pipeline

`GLHelpers` (`CGT537/GLHelpers.cpp/h`) is a standalone replacement for MiniGL used only by `CustomRoomSim`. It owns:
- GLFW window creation and the keyboard/mouse/framebuffer callbacks
- Camera state (`m_camera_loc`, `m_eye_vec`, `m_camera_up`) updated in `viewport()` each frame
- A single shared VAO + three VBOs (vertices, normals, faces/indices)

**Camera controls (CustomRoomSim):** WASD + Q/E to move, right-mouse-drag to look.

`CustomRender()` is called once per frame from the main loop in `Custom.cpp`. It uses `Simulator_OpenGL`'s mesh shader (requires `GLHelpers::getModelviewMatrix()` / `getProjectionMatrix()` to be current).

### Scene files

JSON scene files live in `data/Scenes/`. Format is documented in `doc/file_format.md`. Sampled boundary meshes are cached in `data/Scenes/Cache/`.
