#pragma once

#include "SPlisHSPlasH/Common.h"
#include "GUI/OpenGL/Shader.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <vector>
#include <string>

struct GLFWwindow;

namespace SPH
{
	enum class ParticleRenderMode { Points, Density };

	class GLHelpers
	{
	public:
		static void init(const std::string& shaderPath);
		static void destroy();
		static void setMatrices(const glm::mat4& modelview, const glm::mat4& projection);

		static const ParticleRenderMode m_particleRenderMode = ParticleRenderMode::Points;
		static ParticleRenderMode getParticleRenderMode() { return m_particleRenderMode; }

		static GLFWwindow* getWindow(int* size, const char * title);
		static void viewport();
		static bool getVSync();

		static const glm::mat4& getModelviewMatrix() { return m_modelview; }
		static const glm::mat4& getProjectionMatrix() { return m_projection; }
		static GLuint getVao() { return m_vao; }
		static GLuint getVboNormals() { return m_vbo_normals; }
		static Shader& getParticleShader() { return m_particleShader; }
		static Shader& getPointShader() { return m_pointShader; }

		static void supplyVertices(GLuint index, unsigned int n, const Real* data);
		static void supplyNormals(GLuint index, unsigned int n, const Real* data);
		static void supplyFaces(unsigned int n, const unsigned int* data);

		static void hsvToRgb(float h, float s, float v, float* rgb);
		static void drawMesh(const std::vector<Vector3r>& vertices,
		                     const std::vector<unsigned int>& faces,
		                     const std::vector<Vector3r>& vertexNormals,
		                     const float* color);

	private:

		static glm::vec3 GLHelpers::m_camera_up;
		static glm::vec3 GLHelpers::m_eye_vec;
		static glm::vec3 GLHelpers::m_camera_loc;
		static glm::mat4 GLHelpers::m_modelview;
		static glm::mat4 GLHelpers::m_projection;
		static float     GLHelpers::m_aspect_ratio;
		static float     GLHelpers::m_move_sensitivity;
		static float     GLHelpers::m_look_sensitivity;
		static glm::vec3 GLHelpers::m_movement;
		static glm::vec2 GLHelpers::m_start_drag;
		static glm::vec2 GLHelpers::m_cursor_pos;
		static bool      GLHelpers::m_dragging;
		static float     GLHelpers::m_max_look;
		static float     GLHelpers::m_min_look;

		static GLuint  m_vao;
		static GLuint  m_vbo_vertices;
		static GLuint  m_vbo_normals;
		static GLuint  m_vbo_faces;
		static Shader  m_particleShader;
		static Shader  m_pointShader;

		static void updateCameraRotation(glm::vec3* new_eye_vec, glm::vec3* new_right, glm::vec3* new_up);
		static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void mouse_cursor(GLFWwindow* window, double x, double y);
		static void mouse_button(GLFWwindow* window, int button, int action, int mods);
		static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
		static void supplyVectors(GLuint index, GLuint vbo, unsigned int dim, unsigned int n, const Real* data);
		static void supplyIndices(GLuint vbo, unsigned int n, const unsigned int* data);
	};
}
