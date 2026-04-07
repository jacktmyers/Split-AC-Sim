#include "CustomRender.h"

#include "GLHelpers.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Simulator/GUI/OpenGL/Simulator_OpenGL.h"
#include "Simulator/SimulatorBase.h"
#include "Simulator/SceneConfiguration.h"
#include "SPlisHSPlasH/Simulation.h"
#include "SPlisHSPlasH/BoundaryModel_Akinci2012.h"
#include "SPlisHSPlasH/FluidModel.h"

#include <glad/gl.h>

using namespace SPH;
using namespace Utilities;

void CustomRender(SimulatorBase* base)
{
	Simulation* sim = Simulation::getCurrent();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glCullFace(GL_BACK);
	glBindVertexArray(GLHelpers::getVao());


	// --- Boundary rendering ---
	{
		const SceneLoader::Scene& scene = SceneConfiguration::getCurrent()->getScene();
		const int renderWalls = base->getValue<int>(SimulatorBase::RENDER_WALLS);

		// renderBoundary mesh (inlined) ---
		for (int body = sim->numberOfBoundaryModels() - 1; body >= 0; body--)
		{
			if ((renderWalls == 3) || (!scene.boundaryModels[body]->isWall))
			{
				BoundaryModel* bm = sim->getBoundaryModel(body);
				const Vector4r& c = scene.boundaryModels[body]->color;
				glm::vec4 col((float)c[0], (float)c[1], (float)c[2], (float)c[3]);
				const float* colPtr = glm::value_ptr(col);
				Shader& meshShader = Simulator_OpenGL::getMeshShader();

				// meshShaderBegin (inlined — uses MiniGL)
				{
					meshShader.begin();
					glUniform1f(meshShader.getUniform("shininess"), 5.0f);
					glUniform1f(meshShader.getUniform("specular_factor"), 0.2f);
					glUniformMatrix4fv(meshShader.getUniform("modelview_matrix"),  1, GL_FALSE, glm::value_ptr(GLHelpers::getModelviewMatrix()));
					glUniformMatrix4fv(meshShader.getUniform("projection_matrix"), 1, GL_FALSE, glm::value_ptr(GLHelpers::getProjectionMatrix()));
					glUniform3fv(meshShader.getUniform("surface_color"), 1, colPtr);
				}

				const std::vector<Vector3r>& vertices = bm->getRigidBodyObject()->getVertices();
				const std::vector<Vector3r>& vNormals = bm->getRigidBodyObject()->getVertexNormals();
				const std::vector<unsigned int>& faces = bm->getRigidBodyObject()->getFaces();

				GLHelpers::drawMesh(vertices, faces, vNormals, colPtr);

				// meshShaderEnd — no MiniGL calls, use original
				Simulator_OpenGL::meshShaderEnd();
			}
		}
	}
	// --- Fluid models ---
	for (unsigned int i = 0; i < sim->numberOfFluidModels(); i++)
	{
		FluidModel* model = sim->getFluidModel(i);

		if (!base->getVisible(i))
			continue;

		// --- renderFluid ---
		{
			const unsigned int nParticles = model->numActiveParticles();
			if (nParticles > 0)
			{
				glEnable(GL_PROGRAM_POINT_SIZE);

				if (GLHelpers::getParticleRenderMode() == ParticleRenderMode::Points)
				{
					Shader& shader = GLHelpers::getPointShader();
					shader.begin();
					glUniformMatrix4fv(shader.getUniform("modelview_matrix"),  1, GL_FALSE, glm::value_ptr(GLHelpers::getModelviewMatrix()));
					glUniformMatrix4fv(shader.getUniform("projection_matrix"), 1, GL_FALSE, glm::value_ptr(GLHelpers::getProjectionMatrix()));
					glUniform1f(shader.getUniform("point_size"), 10.0f);

					GLHelpers::supplyVertices(0, nParticles, &model->getPosition(0)(0));

					glDrawArrays(GL_POINTS, 0, nParticles);
					glDisableVertexAttribArray(0);
					shader.end();
				}
				else
				{
					Shader& shader = GLHelpers::getParticleShader();
					shader.begin();
					glUniformMatrix4fv(shader.getUniform("modelview_matrix"),  1, GL_FALSE, glm::value_ptr(GLHelpers::getModelviewMatrix()));
					glUniformMatrix4fv(shader.getUniform("projection_matrix"), 1, GL_FALSE, glm::value_ptr(GLHelpers::getProjectionMatrix()));
					glUniform1f(shader.getUniform("point_size"), 20.0f);
					glUniform1f(shader.getUniform("rest_density"), (float)model->getDensity0());

					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					glDepthMask(GL_FALSE);

					GLHelpers::supplyVertices(0, nParticles, &model->getPosition(0)(0));

					glBindBuffer(GL_ARRAY_BUFFER, GLHelpers::getVboNormals());
					glBufferData(GL_ARRAY_BUFFER, nParticles * sizeof(Real), &model->getDensity(0), GL_STREAM_DRAW);
					glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);
					glEnableVertexAttribArray(1);

					glDrawArrays(GL_POINTS, 0, nParticles);
					glDisableVertexAttribArray(0);
					glDisableVertexAttribArray(1);

					glDepthMask(GL_TRUE);
					glDisable(GL_BLEND);
					shader.end();
				}
			}
		}
	}
}
