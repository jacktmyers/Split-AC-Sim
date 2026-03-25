import pysplishsplash as sph
import pysplishsplash.Utilities.SceneLoaderStructs as Scenes

MESH_FILE = "C:\\Users\\Jack\\Documents\\School\\CGT-537\\clones\\SPlisHSPlasH\\Temp\\Small-Room-Window-Volume.obj"



def main():
    base = sph.Exec.SimulatorBase()
    base.init(useGui=True, sceneFile=sph.Extras.Scenes.Empty)

    gui = sph.GUI.Simulator_GUI_imgui(base)
    base.setGui(gui)

    base.setValueBool(base.PAUSE, True)

    scene = sph.Exec.SceneConfiguration.getCurrent().getScene()

    scene.particleRadius = 0.05
    scene.sim2D = False

    sim = sph.Simulation.getCurrent()

    scene.boundaryModels.append(Scenes.BoundaryData(
        meshFile="C:\\Users\\Jack\\Documents\\School\\CGT-537\\clones\\SPlisHSPlasH\\Temp\\Small-Room-Window-Volume.obj",
        translation=[0.0, 0.0, 0.0],
        scale=[1, 1, 1],
        color=[0.1, 0.4, 0.6, 1.0],
        isDynamic=False,
        isWall=False,
        mapInvert=True,
        mapThickness=0.0,
        mapResolution=[20, 20, 20]
    ))

    scene.emitters.append(Scenes.EmitterData(
        id="Fluid",
        width=5,
        height=5,
        x=[0.0, 0.0, -1.0],
        velocity=1,
        axis=[0, 1, 0],
        angle=0,
        emitStartTime=0,
        emitEndTime=5000,
        type=0
    ))

    base.initSimulation()

    sim.setValueInt(sim.SIMULATION_METHOD, 2)
    sim.setValueInt(sim.CFL_METHOD, 1)
    sim.setValueFloat(sim.CFL_FACTOR, 1)

    sim.setVec3ValueReal(sim.GRAVITATION, [0.0, 0.0, 0.0])

    base.runSimulation()
    base.cleanup()


if __name__ == "__main__":
    main()
