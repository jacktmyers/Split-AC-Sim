#include "SPlisHSPlasH/Common.h"
#include "Simulator/SimulatorBase.h"
#include "Simulator/GUI/imgui/Simulator_GUI_imgui.h"
#include "Simulator/SceneConfiguration.h"
#include "SPlisHSPlasH/Simulation.h"
#include "SPlisHSPlasH/Utilities/SceneParameterObjects.h"
#include "SPlisHSPlasH/Utilities/MeshImport.h"
#include "SPlisHSPlasH/FluidModel.h"
#include "SPlisHSPlasH/EmitterSystem.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "CustomRender.h"
#include "GLHelpers.h"

#include "Simulator/GUI/OpenGL/Simulator_OpenGL.h"

#include <string>
#include <vector>

using namespace SPH;

SimulatorBase* base = nullptr;
Simulator_GUI_imgui* gui = nullptr;

const char* const  title = "Room Sim";
int window_size[2] = {1024,1024};

int main(int argc, char** argv)
{
	const std::string ROOM_MESH_FILE = "C:\\Users\\Jack\\Documents\\School\\CGT-537\\Split-AC-Sim-1\\FluidSim\\CGT537\\models\\Small-Room-Window.obj"; //For some reason this has to be an absolute path

	TriangleMesh roomMesh;
	MeshImport::importMesh(ROOM_MESH_FILE, roomMesh, Vector3r::Zero(), Matrix3r::Identity(), Vector3r::Ones());

	Vector3r deletionBoxMin = Vector3r::Constant(std::numeric_limits<Real>::max());
	Vector3r deletionBoxMax = Vector3r::Constant(std::numeric_limits<Real>::lowest());
	for (const Vector3r& v : roomMesh.getVertices())
	{
		deletionBoxMin = deletionBoxMin.cwiseMin(v);
		deletionBoxMax = deletionBoxMax.cwiseMax(v);
	}

	base = new SimulatorBase();

	std::vector<std::string> args(argv, argv + argc);
	bool hasSceneArg = false;
	for (int i = 1; i < (int)args.size(); i++)
	{
		if (args[i].rfind(".json") != std::string::npos || args[i] == "--scene-file")
		{
			hasSceneArg = true;
			break;
		}
	}
	if (!hasSceneArg)
		args.push_back("../data/Scenes/Empty.json");

	base->init(args, "Custom Room Simulation");
	base->setUseGUI(false);

	// gui = new Simulator_GUI_imgui(base);
	// base->setGui(gui);

	const Real PARTICLE_RADIUS = static_cast<Real>(0.05);
	const unsigned char FILL_MODE = 0;

	Utilities::SceneLoader::Scene& scene = SceneConfiguration::getCurrent()->getScene();
	scene.particleRadius = PARTICLE_RADIUS;
	scene.sim2D = false;

	// auto* fluidBlock = new Utilities::FluidBlockParameterObject();
	// fluidBlock->id = "Fluid";
	// fluidBlock->boxMin = deletionBoxMin;
	// fluidBlock->boxMax = deletionBoxMax;
	// fluidBlock->translation = Vector3r::Zero();
	// fluidBlock->scale = Vector3r::Ones();
	// fluidBlock->mode = FILL_MODE;
	// fluidBlock->initialVelocity = Vector3r::Zero();
	// fluidBlock->initialAngularVelocity = Vector3r::Zero();
	// scene.fluidBlocks.push_back(fluidBlock);

	auto* boundary = new Utilities::BoundaryParameterObject();
	boundary->meshFile = ROOM_MESH_FILE;
	boundary->translation = Vector3r(0.0, 0.0, 0.0);
	boundary->scale = Vector3r(1.0, 1.0, 1.0);
	boundary->color = Vector4r(0.1, 0.4, 0.6, 1.0);
	boundary->dynamic = false;
	boundary->isWall = false;
	boundary->mapInvert = false;
	boundary->mapThickness = static_cast<Real>(0.0);
	boundary->mapResolution = Eigen::Matrix<unsigned int, 3, 1>(20, 20, 20);
	scene.boundaryModels.push_back(boundary);

	auto* emitter = new Utilities::EmitterParameterObject();
	emitter->id = "Fluid";
	emitter->width = 5;
	emitter->height = 5;
	emitter->x = Vector3r(-1.2, 1.0, -1.0);
	emitter->velocity = static_cast<Real>(1.0);
	emitter->axis = Vector3r(0.0, 1.0, 0.0);
	emitter->angle = static_cast<Real>(0.0);
	emitter->emitStartTime = static_cast<Real>(0.0);
	emitter->emitEndTime = static_cast<Real>(5000.0);
	emitter->type = 0;
	scene.emitters.push_back(emitter);

	base->setValue<bool>(base->PAUSE, false);

	base->initSimulation();

	Simulation* sim = Simulation::getCurrent();
	sim->setValue<int>(sim->BOUNDARY_HANDLING_METHOD, 0);
	sim->setValue<int>(sim->SIMULATION_METHOD, 2);
	sim->setValue<int>(sim->CFL_METHOD, 1);
	sim->setValue<Real>(sim->CFL_FACTOR, static_cast<Real>(1.0));

	Vector3r gravity(0.0, 0.0, 0.0);
	sim->setVecValue<Real>(sim->GRAVITATION, gravity.data());

	sim->getFluidModel(0)->getEmitterSystem()->enableReuseParticles(deletionBoxMin, deletionBoxMax);

	base->deferredInit();

	GLFWwindow* window = GLHelpers::getWindow(window_size, title);
	GLHelpers::init(base->getExePath() + "/resources/shaders");
	Simulator_OpenGL::initShaders(base->getExePath() + "/resources/shaders");

	double lastRenderTime = 0.0;
	const double renderInterval = 1.0 / 60.0;

	while (!glfwWindowShouldClose(window))
	{
		base->timeStep();

		const double currentTime = glfwGetTime();
		if (currentTime - lastRenderTime >= renderInterval)
		{
			glfwPollEvents();
			GLHelpers::viewport();
			CustomRender(base);

			glfwSwapBuffers(window);

			lastRenderTime = currentTime;
		}
	}

	GLHelpers::destroy();
	glfwDestroyWindow(window);
	glfwTerminate();

	base->cleanup();

	delete gui;
	delete base;

	return 0;
}
