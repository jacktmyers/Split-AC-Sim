#include "GLHelpers.h"

#include <glad/gl.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>

#ifdef USE_DOUBLE
#define GL_REAL GL_DOUBLE
#else
#define GL_REAL GL_FLOAT
#endif

using namespace SPH;

glm::vec3    GLHelpers::m_camera_up  = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3    GLHelpers::m_eye_vec    = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3    GLHelpers::m_camera_loc = glm::vec3(0.0f, 0.0f, 1.0f);
glm::mat4    GLHelpers::m_modelview;
glm::mat4    GLHelpers::m_projection;
float        GLHelpers::m_aspect_ratio    = 1.0f;
float        GLHelpers::m_move_sensitivity = 0.01f;
float        GLHelpers::m_look_sensitivity = 0.01f;
glm::vec3    GLHelpers::m_movement        = glm::vec3(0.0f);
glm::vec2    GLHelpers::m_start_drag      = glm::vec2(0.0f);
glm::vec2    GLHelpers::m_cursor_pos      = glm::vec2(0.0f);
bool         GLHelpers::m_dragging        = false;
float        GLHelpers::m_max_look        = 0.0f;
float        GLHelpers::m_min_look        = 0.0f;

GLuint       GLHelpers::m_vao = 0;
GLuint       GLHelpers::m_vbo_vertices = 0;
GLuint       GLHelpers::m_vbo_normals = 0;
GLuint       GLHelpers::m_vbo_faces = 0;
Shader       GLHelpers::m_particleShader;
Shader       GLHelpers::m_pointShader;

void GLHelpers::init(const std::string& shaderPath)
{
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	glGenBuffers(1, &m_vbo_vertices);
	glGenBuffers(1, &m_vbo_normals);
	glGenBuffers(1, &m_vbo_faces);

	m_particleShader.compileShaderFile(GL_VERTEX_SHADER,   shaderPath + "/density_vs.glsl");
	m_particleShader.compileShaderFile(GL_FRAGMENT_SHADER, shaderPath + "/density_fs.glsl");
	m_particleShader.createAndLinkProgram();
	m_particleShader.begin();
	m_particleShader.addUniform("modelview_matrix");
	m_particleShader.addUniform("projection_matrix");
	m_particleShader.addUniform("point_size");
	m_particleShader.addUniform("rest_density");
	m_particleShader.end();

	m_pointShader.compileShaderFile(GL_VERTEX_SHADER,   shaderPath + "/point_vs.glsl");
	m_pointShader.compileShaderFile(GL_FRAGMENT_SHADER, shaderPath + "/point_fs.glsl");
	m_pointShader.createAndLinkProgram();
	m_pointShader.begin();
	m_pointShader.addUniform("modelview_matrix");
	m_pointShader.addUniform("projection_matrix");
	m_pointShader.addUniform("point_size");
	m_pointShader.end();
}

void GLHelpers::destroy()
{
	m_particleShader.destroy();
	m_pointShader.destroy();
	glDeleteBuffers(1, &m_vbo_vertices);
	glDeleteBuffers(1, &m_vbo_normals);
	glDeleteBuffers(1, &m_vbo_faces);
	glDeleteVertexArrays(1, &m_vao);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
}

GLFWwindow* GLHelpers::getWindow(int* size, const char* title)
{
	GLFWwindow* window;

	if (!glfwInit())
		return nullptr;

#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

	window = glfwCreateWindow(size[0], size[1], title, NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	glfwSetKeyCallback(window, keyboard);
	glfwSetCursorPosCallback(window, mouse_cursor);
	glfwSetMouseButtonCallback(window, mouse_button);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	gladLoadGL(glfwGetProcAddress);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	return window;
}

void GLHelpers::viewport()
{
	glm::mat4 V;
	if (m_dragging)
	{
		glm::vec3 new_eye_vec, new_right, new_up;
		updateCameraRotation(&new_eye_vec, &new_right, &new_up);
		glm::vec3 move = (new_eye_vec * m_movement.y) + (new_right * m_movement.x) + (new_up * m_movement.z);
		m_camera_loc += move;
		V = glm::lookAt(m_camera_loc, m_camera_loc + new_eye_vec, new_up);
	}
	else
	{
		V = glm::lookAt(m_camera_loc, m_camera_loc + m_eye_vec, m_camera_up);
	}
	m_modelview  = V;
	m_projection = glm::perspective(glm::pi<float>() / 2.0f, m_aspect_ratio, 0.1f, 100.0f);
}

void GLHelpers::updateCameraRotation(glm::vec3* new_eye_vec, glm::vec3* new_right, glm::vec3* new_up)
{
	glm::vec2 look_delta = (m_start_drag - m_cursor_pos) * m_look_sensitivity;
	look_delta.y = std::max(std::min(look_delta.y, m_max_look), m_min_look);
	m_start_drag.y = m_cursor_pos.y + (look_delta.y / m_look_sensitivity);
	glm::mat4 rot_h = glm::rotate(look_delta.x, glm::vec3(0, 1, 0));
	glm::mat4 rot_v = glm::rotate(look_delta.y * -1.0f, glm::cross(glm::vec3(0, 1, 0), m_eye_vec));
	*new_eye_vec = rot_h * rot_v * glm::vec4(m_eye_vec, 0.0f);
	*new_up      = rot_h * rot_v * glm::vec4(m_camera_up, 0.0f);
	*new_right   = rot_h * glm::vec4(glm::cross(m_camera_up, m_eye_vec), 0.0f);
}

void GLHelpers::keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_W: m_movement.y =  m_move_sensitivity; break;
		case GLFW_KEY_S: m_movement.y = -m_move_sensitivity; break;
		case GLFW_KEY_A: m_movement.x =  m_move_sensitivity; break;
		case GLFW_KEY_D: m_movement.x = -m_move_sensitivity; break;
		case GLFW_KEY_Q: m_movement.z = -m_move_sensitivity; break;
		case GLFW_KEY_E: m_movement.z =  m_move_sensitivity; break;
		}
	}
	else if (action == GLFW_RELEASE)
	{
		switch (key)
		{
		case GLFW_KEY_W: case GLFW_KEY_S: m_movement.y = 0.0f; break;
		case GLFW_KEY_A: case GLFW_KEY_D: m_movement.x = 0.0f; break;
		case GLFW_KEY_Q: case GLFW_KEY_E: m_movement.z = 0.0f; break;
		}
	}
}

void GLHelpers::mouse_cursor(GLFWwindow* window, double x, double y)
{
	m_cursor_pos = glm::vec2((float)x, (float)y);
}

void GLHelpers::mouse_button(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		m_start_drag = m_cursor_pos;
		m_max_look = std::abs(std::acos(glm::dot(m_eye_vec, glm::vec3(0, 1, 0)))) - 0.001f;
		m_min_look = (std::abs(std::acos(glm::dot(m_eye_vec, glm::vec3(0, -1, 0)))) * -1.0f) + 0.001f;
		m_dragging = true;
	}
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
	{
		glm::vec3 new_eye_vec, new_right, new_up;
		updateCameraRotation(&new_eye_vec, &new_right, &new_up);
		m_eye_vec   = new_eye_vec;
		m_camera_up = new_up;
		m_dragging  = false;
	}
}

void GLHelpers::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	m_aspect_ratio = (float)width / (float)height;
}

bool GLHelpers::getVSync()
{
	return true;
}

void GLHelpers::setMatrices(const glm::mat4& modelview, const glm::mat4& projection)
{
	m_modelview  = modelview;
	m_projection = projection;
}

void GLHelpers::supplyVectors(GLuint index, GLuint vbo, unsigned int dim, unsigned int n, const Real* data)
{
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, n * dim * sizeof(Real), data, GL_STREAM_DRAW);
	glVertexAttribPointer(index, dim, GL_REAL, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(index);
}

void GLHelpers::supplyIndices(GLuint vbo, unsigned int n, const unsigned int* data)
{
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, n * sizeof(unsigned int), data, GL_STREAM_DRAW);
}

void GLHelpers::supplyVertices(GLuint index, unsigned int n, const Real* data)
{
	supplyVectors(index, m_vbo_vertices, 3, n, data);
}

void GLHelpers::supplyNormals(GLuint index, unsigned int n, const Real* data)
{
	supplyVectors(index, m_vbo_normals, 3, n, data);
}

void GLHelpers::supplyFaces(unsigned int n, const unsigned int* data)
{
	supplyIndices(m_vbo_faces, n, data);
}

void GLHelpers::hsvToRgb(float h, float s, float v, float* rgb)
{
	int i = (int)floor(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);

	switch (i % 6)
	{
	case 0: rgb[0] = v, rgb[1] = t, rgb[2] = p; break;
	case 1: rgb[0] = q, rgb[1] = v, rgb[2] = p; break;
	case 2: rgb[0] = p, rgb[1] = v, rgb[2] = t; break;
	case 3: rgb[0] = p, rgb[1] = q, rgb[2] = v; break;
	case 4: rgb[0] = t, rgb[1] = p, rgb[2] = v; break;
	case 5: rgb[0] = v, rgb[1] = p, rgb[2] = q; break;
	}
}

void GLHelpers::drawMesh(const std::vector<Vector3r>& vertices,
                          const std::vector<unsigned int>& faces,
                          const std::vector<Vector3r>& vertexNormals,
                          const float* color)
{
	supplyVertices(0, (unsigned int)vertices.size(), &vertices[0][0]);
	if (vertexNormals.size() > 0)
		supplyNormals(2, (unsigned int)vertexNormals.size(), &vertexNormals[0][0]);
	supplyFaces((unsigned int)faces.size(), faces.data());

	glDrawElements(GL_TRIANGLES, (GLsizei)faces.size(), GL_UNSIGNED_INT, (void*)0);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(2);
}
