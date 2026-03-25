#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>

//Imgui headers for UI
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <format>

#include "DebugCallback.h"
#include "InitShader.h"    //Functions for loading shaders from text files
#include "LoadMesh.h"      //Functions for creating OpenGL buffers from mesh files
#include "LoadTexture.h"   //Functions for creating OpenGL textures from image files

#include "ReadInCSV.h"
#include <filesystem>
namespace fs = std::filesystem;


namespace window
{
  const char* const title = "Pose Visualizer"; //defined in project settings
  int size[2] = {1024, 1024};
  float clear_color[4] = {0.35f, 0.35f, 0.35f, 1.0f};
}

namespace scene
{
  const std::string base = "stool";
  const std::string shader_dir = "shaders/";
  const std::string vertex_shader("lab4_vs.glsl");
  const std::string fragment_shader("lab4_fs.glsl");
  const std::string frustum_vs("frustum_vs.glsl");
  const std::string frustum_fs("frustum_fs.glsl");
  const std::string video_vs("video_vs.glsl");
  const std::string video_fs("video_fs.glsl");
  const std::string default_track_vs("default_track_vs.glsl");
  const std::string default_track_fs("default_track_fs.glsl");
  const std::string model_vs("model_vs.glsl");
  const std::string model_fs("model_fs.glsl");
  float aspect_ratio = 1.0f;

  MeshData mesh;
  GLuint tex_id;
  
  GLuint track_shader = -1;
  GLuint frustum_shader = -1;
  GLuint video_shader = -1;
  GLuint model_shader = -1;
  GLuint default_track_shader = -1;
  
  glm::vec3 rotate = glm::vec3(0,0,0);
  float scale = 10.0f;
  
  glm::vec3 model_rotate = glm::vec3(glm::pi<float>() * -1.0f/2.0f,0,-1.06465);
  glm::vec3 model_trans = glm::vec3(-4,4,-12);
  float model_scale = 0.4f;

  GLuint point_vao = -1;
  GLuint frustum_vao = -1;
  GLuint video_vao = -1;

  float frust_size = 3;
  glm::vec3 frustum_data[] = {
    glm::vec3(0,0,0),
    glm::vec3(1/frust_size,1/frust_size,-1/frust_size),
    
    glm::vec3(0,0,0),
    glm::vec3(-1/frust_size,1/frust_size,-1/frust_size),
    
    glm::vec3(0,0,0),
    glm::vec3(1/frust_size,-1/frust_size,-1/frust_size),
    
    glm::vec3(0,0,0),
    glm::vec3(-1/frust_size,-1/frust_size,-1/frust_size),
  };

  glm::vec3 video_tris[] = {
    glm::vec3(-1/frust_size,1/frust_size,-1/frust_size),
    glm::vec3(-1/frust_size,-1/frust_size,-1/frust_size),
    glm::vec3(1/frust_size,-1/frust_size,-1/frust_size),
    
    glm::vec3(-1/frust_size,1/frust_size,-1/frust_size),
    glm::vec3(1/frust_size,1/frust_size,-1/frust_size),
    glm::vec3(1/frust_size,-1/frust_size,-1/frust_size),
  };

  glm::vec3 video_tex_coords[] = {
    glm::vec3(0,1,1),
    glm::vec3(0,0,1),
    glm::vec3(1,0,1),
    
    glm::vec3(0,1,1),
    glm::vec3(1,1,1),
    glm::vec3(1,0,1),
  };

  GLuint video_3d_tex = -1;
  int video_width = 0;
  int video_height = 0;
  int video_layer_count = 0;
  
  std::vector<trackPoint> track_data = std::vector<trackPoint>();
  std::vector<glm::vec4> track_pos = std::vector<glm::vec4>();
  std::vector<glm::vec3> track_rot = std::vector<glm::vec3>();
  
  std::vector<std::vector<std::vector<glm::vec3>>> video_data;
  int frame = 0;
  
  glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 eye_vec = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 camera_loc = glm::vec3(0.0f, 0.0f, 1.0f);
  float max_look;
  float min_look;
  
  float move_sensitivity = 0.01f;
  float look_sensitivity = 0.01f;
  glm::vec3 movement = glm::vec3(0, 0, 0);
  glm::vec2 start_drag = glm::vec2(0,0);
  glm::vec3 start_drag_look_at = glm::vec3(0,0,0);
  glm::vec2 cursor_pos = glm::vec2(0,0);
  bool dragging = false;
  
  int trail = 0;

  int axis_fix_x = 0;
  int axis_fix_y = 0;
  int axis_fix_z = 0;
  int axis_fix_w = 0;
}
//For an explanation of this program's structure see https://www.glfw.org/docs/3.3/quick.html 


//Draw the ImGui user interface
void draw_gui(GLFWwindow* window)
{
  //Begin ImGui Frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  
  static bool show_test = false;
  if(show_test)
  {
    ImGui::ShowDemoWindow(&show_test);
  }
  
  //Draw Gui
  ImGui::Begin("Debug window");
  
  if (ImGui::Button("Quit"))
  {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
  
  //ImGui::InputFloat("Model x", &scene::model_trans.x, 0.01f, 1000, "%.3f");
  //ImGui::InputFloat("Model y", &scene::model_trans.y, 0.01f, 1000, "%.3f");
  //ImGui::InputFloat("Model z", &scene::model_trans.z, 0.01f, 1000, "%.3f");
  //
  //ImGui::InputFloat("Scale", &scene::scale, 0.01f, 20.0f, "%.3f");

  //ImGui::InputFloat("Model Scale", &scene::model_scale, 0.01f, 20.0f, "%.3f");
  //
  //ImGui::SliderAngle("X Rotate", &scene::model_rotate.x);
  //ImGui::SliderAngle("Y Rotate", &scene::model_rotate.y);
  //ImGui::SliderAngle("Z Rotate", &scene::model_rotate.z);
  //
  ImGui::SliderInt("Frame", &scene::frame, 0, scene::track_data.size()-1);

  ImGui::RadioButton("Show All", &scene::trail, 0);
  ImGui::RadioButton("Show Trail", &scene::trail, 1);

  // ImGui::RadioButton("X", &scene::axis_fix_x, 0);
  // ImGui::RadioButton("Y", &scene::axis_fix_y, 0);
  // ImGui::RadioButton("Z", &scene::axis_fix_z, 0);
  // ImGui::RadioButton("W", &scene::axis_fix_w, 0);

  // ImGui::RadioButton("X_Inv", &scene::axis_fix_x, 1);
  // ImGui::RadioButton("Y_Inv", &scene::axis_fix_y, 1);
  // ImGui::RadioButton("Z_Inv", &scene::axis_fix_z, 1);
  // ImGui::RadioButton("W_Inv", &scene::axis_fix_w, 1);
  
  ImGui::End();
  
  //End ImGui Frame
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void updateCameraRotation(glm::vec3* new_eye_vec, glm::vec3* new_right, glm::vec3* new_up) {
  glm::vec2 look_delta = (scene::start_drag - scene::cursor_pos) * scene::look_sensitivity;
  look_delta.y = std::max(std::min(look_delta.y, scene::max_look), scene::min_look);
  //Update start accordingly for natural controls
  scene::start_drag.y = scene::cursor_pos.y + (look_delta.y / scene::look_sensitivity);
  glm::mat4 rot_h = glm::rotate(look_delta.x, glm::vec3(0, 1, 0));
  glm::mat4 rot_v = glm::rotate(look_delta.y * -1.0f, glm::cross(glm::vec3(0, 1, 0), scene::eye_vec));
  
  *new_eye_vec = rot_h * rot_v * glm::vec4(scene::eye_vec, 0.0f);
  *new_up = rot_h * rot_v * glm::vec4(scene::camera_up, 0.0f);
  *new_right = rot_h * glm::vec4(glm::cross(scene::camera_up, scene::eye_vec), 0.0f);
}

void assignPVMtoShader(GLuint shader, glm::mat4 P, glm::mat4 V, glm::mat4 M) {
  //Get location for shader uniform variable
  int PVM_loc = glGetUniformLocation(shader, "PVM");
  if (PVM_loc != -1)
  {
    glm::mat4 PVM = P * V * M;
    //Set the value of the variable at a specific location
    glUniformMatrix4fv(PVM_loc, 1, false, glm::value_ptr(PVM));
  }
  
  int M_loc = glGetUniformLocation(shader, "M");
  if (M_loc != -1)
  {
    //Set the value of the variable at a specific location
    glUniformMatrix4fv(M_loc, 1, false, glm::value_ptr(M));
  }
  
  int norm_M_loc = glGetUniformLocation(shader, "norm_M");
  if (norm_M_loc != -1)
  {
    glm::mat4 norm_M = glm::inverse(glm::transpose(M));
    //Set the value of the variable at a specific location
    glUniformMatrix4fv(norm_M_loc, 1, false, glm::value_ptr(norm_M));
  }
}

// This function gets called every time the scene gets redisplayed
void display(GLFWwindow* window)
{
  GLboolean depth_active;
  glGetBooleanv(GL_DEPTH_TEST, &depth_active);
  
  //glClearColor(window::clear_color[0], window::clear_color[1], window::clear_color[2], window::clear_color[3]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glm::mat4 T = glm::translate(glm::vec3(0,0,0));
  glm::mat4 R = glm::rotate(scene::rotate.x, glm::vec3(1.0f, 0.0f, 0.0f))*glm::rotate(scene::rotate.y, glm::vec3(0.0f, 1.0f, 0.0f))*glm::rotate(scene::rotate.z, glm::vec3(0.0f, 0.0f, 1.0f)); //Rotation about X Y Z
  glm::mat4 S = glm::scale(glm::vec3(scene::scale));

  glm::mat4 model_T = glm::translate(scene::model_trans);
  glm::mat4 model_R = glm::rotate(scene::model_rotate.x, glm::vec3(1.0f, 0.0f, 0.0f))*glm::rotate(scene::model_rotate.y, glm::vec3(0.0f, 1.0f, 0.0f))*glm::rotate(scene::model_rotate.z, glm::vec3(0.0f, 0.0f, 1.0f)); //Rotation about X Y Z
  glm::mat4 model_S = glm::scale(glm::vec3(scene::model_scale));
  
  glm::mat4 M;
  M = R * T * S;
  glm::mat4 model_M;
  model_M = model_R * model_T * model_S;
  //M = T * R * S;
  
  glm::mat4 V;
  if (scene::dragging) {
    glm::vec3 new_eye_vec, new_right, new_up;
    updateCameraRotation(&new_eye_vec, &new_right, &new_up);
    
    glm::vec3 move = (new_eye_vec * scene::movement.y) + (new_right * scene::movement.x) + (new_up * scene::movement.z);
    scene::camera_loc += move;
    
    V = glm::lookAt(scene::camera_loc, scene::camera_loc + new_eye_vec, new_up);
  }
  else {
    V = glm::lookAt(scene::camera_loc, scene::camera_loc + scene::eye_vec, scene::camera_up);
  }
  glm::mat4 P = glm::perspective(glm::pi<float>() / 2.0f, scene::aspect_ratio, 0.1f, 100.0f);
  

  glm::quat q = scene::track_data[scene::frame].rotation;
   glm::quat quat_fix = glm::quat(
     q.x*-1.0f,
     q.y*-1.0f,
     q.z*1.0f,
     q.w*1.0f);
  //glm::quat quat_fix = glm::quat(
  //  q.x * (scene::axis_fix_x == 0 ? 1.0f : -1.0f),
  //  q.y * (scene::axis_fix_y == 0 ? 1.0f : -1.0f),
  //  q.z * (scene::axis_fix_z == 0 ? 1.0f : -1.0f),
  //  q.w * (scene::axis_fix_w == 0 ? 1.0f : -1.0f)
  //);

  glUseProgram(scene::model_shader);

  int PVM_loc = glGetUniformLocation(scene::model_shader, "PVM");
  if (PVM_loc != -1)
  {
    glm::mat4 PVM = P * V * model_M;
    //Set the value of the variable at a specific location
    glUniformMatrix4fv(PVM_loc, 1, false, glm::value_ptr(PVM));
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene::tex_id);
  int tex_loc = glGetUniformLocation(scene::model_shader, "diffuse_tex");
  if (tex_loc != -1)
  {
      glUniform1i(tex_loc, 0); // we bound our texture to texture unit 0
  }


  glBindVertexArray(scene::mesh.mVao);
  scene::mesh.DrawMesh();

 
  
  glUseProgram(scene::frustum_shader);
  //glm::quat rot = glm::inverse(scene::track_data[0].rotation)*scene::track_data[scene::frame].rotation;
  glm::vec3 trans = scene::track_data[scene::frame].position;
  glm::mat4 frus_mat = glm::translate(trans) * glm::mat4_cast(quat_fix) * glm::scale(glm::vec3(scene::video_width, scene::video_height, 400.0f))*glm::scale(glm::vec3(.001f));
  //glm::mat4 frus_mat = glm::mat4_cast(quat_fix) * glm::scale(glm::vec3(scene::video_width, scene::video_height, 400.0f))*glm::scale(glm::vec3(.001f));

  glm::mat4 axis_fix = glm::scale(glm::vec3(1.0f, 1.0f, -1.0f));


  assignPVMtoShader(scene::frustum_shader, P, V, M*axis_fix*frus_mat);
  
  // scene::frustum_data[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)*glm::rotate(rot.x, glm::vec3(1.0f, 0.0f, 0.0f))*glm::rotate(rot.y, glm::vec3(0.0f, 1.0f, 0.0f))*glm::rotate(rot.z, glm::vec3(0.0f, 0.0f, 1.0f));
  
  glPointSize(20.0f);
  
  glBindVertexArray(scene::frustum_vao);
  
  glDrawArrays(GL_LINES, 0, 8);

  glUseProgram(scene::video_shader);
  assignPVMtoShader(scene::video_shader, P, V, M*axis_fix*frus_mat);

  glActiveTexture(GL_TEXTURE0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  tex_loc = glGetUniformLocation(scene::video_shader, "tex_look_up");
  glBindTexture(GL_TEXTURE_3D, scene::video_3d_tex);
  glUniform1i(tex_loc, 0);

  int tl_loc = glGetUniformLocation(scene::video_shader, "tex_layer");
  glUniform1f(tl_loc, (float)scene::frame / (float)scene::track_data.size());

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBindVertexArray(scene::video_vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  if (scene::trail == 0) {
    glUseProgram(scene::default_track_shader);
    assignPVMtoShader(scene::default_track_shader, P, V, M*axis_fix);
  }else{
    glUseProgram(scene::track_shader);
    assignPVMtoShader(scene::track_shader, P, V, M*axis_fix);
    int frame_loc = glGetUniformLocation(scene::track_shader, "frame");
    glUniform1i(frame_loc, scene::frame);
  }
  
  //glEnable(GL_POINT_SMOOTH);
  
  glPointSize(10.0f);
  glBindVertexArray(scene::point_vao);
  
  glDrawArrays(GL_POINTS, 0, scene::track_data.size());
  
  draw_gui(window);
  // Swap front and back buffers
  glfwSwapBuffers(window);
}

void idle()
{
  float time_sec = static_cast<float>(glfwGetTime());
  
  //Pass time_sec value to the shaders
  int time_loc = glGetUniformLocation(scene::track_shader, "time");
  if (time_loc != -1)
  {
    glUniform1f(time_loc, time_sec);
  }
}

void read_in_track_data(){
  scene::track_data = readMotionTrackToArray("C:\\Users\\Jack\\Documents\\School\\CGT-537\\Exercise-6.11\\pos-data\\" + scene::base + ".csv");
  
  scene::track_pos.clear();
  int i = 0;
  for (const auto &tp : scene::track_data) {
    scene::track_pos.push_back(glm::vec4(tp.position, (float)i++));
  }

  GLuint vbo = -1;
  
  glGenVertexArrays(1, &scene::point_vao);
  glGenBuffers(1, &vbo);
  
  glBindVertexArray(scene::point_vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  
  glBufferData(GL_ARRAY_BUFFER, scene::track_pos.size() * sizeof(glm::vec4), scene::track_pos.data(), GL_STATIC_DRAW);
  GLint pos_loc = glGetAttribLocation(scene::track_shader, "pos_attrib");
  glEnableVertexAttribArray(pos_loc);
  glVertexAttribPointer(pos_loc, 4, GL_FLOAT, false, 0, 0);
  
  glGenVertexArrays(1, &scene::frustum_vao);
  glGenBuffers(1, &vbo);
  
  glBindVertexArray(scene::frustum_vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  
  glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(glm::vec3), scene::frustum_data, GL_STATIC_DRAW);
  GLint frustum_loc = glGetAttribLocation(scene::frustum_shader, "pos_attrib");
  glEnableVertexAttribArray(frustum_loc);
  glVertexAttribPointer(frustum_loc, 3, GL_FLOAT, false, 0, 0);


  glGenVertexArrays(1, &scene::video_vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindVertexArray(scene::video_vao);

  glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(glm::vec3), scene::video_tris, GL_STATIC_DRAW);
  GLint video_loc = glGetAttribLocation(scene::video_shader, "pos_attrib");
  glEnableVertexAttribArray(video_loc);
  glVertexAttribPointer(video_loc, 3, GL_FLOAT, false, 0, 0);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindVertexArray(scene::video_vao);

  glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(glm::vec3), scene::video_tex_coords, GL_STATIC_DRAW);
  GLint video_tl_loc = glGetAttribLocation(scene::video_shader, "tex_coord_attrib");
  glEnableVertexAttribArray(video_tl_loc);
  glVertexAttribPointer(video_tl_loc, 3, GL_FLOAT, false, 0, 0);
  
}


void read_in_video_data(){
  std::string dir = "C:\\Users\\Jack\\Documents\\School\\CGT-537\\Exercise-6.11\\pos-data\\" + scene::base + "_pics\\";
  std::vector<std::string> fnames = std::vector<std::string>();

  for (const fs::directory_entry& e : fs::directory_iterator(dir)) {
    fnames.push_back(e.path().string());
  }

  scene::video_3d_tex = Load3DTexture(fnames, &scene::video_width, &scene::video_height);
  std::cout << "video width: " << scene::video_width << ", video height: " << scene::video_height << std::endl; 
}


void reload_shader()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  std::string vs = scene::shader_dir + scene::vertex_shader;
  std::string fs = scene::shader_dir + scene::fragment_shader;
  std::string frus_vs = scene::shader_dir + scene::frustum_vs;
  std::string frus_fs = scene::shader_dir + scene::frustum_fs;
  std::string video_vs = scene::shader_dir + scene::video_vs;
  std::string video_fs = scene::shader_dir + scene::video_fs;
  std::string default_track_vs = scene::shader_dir + scene::default_track_vs;
  std::string default_track_fs = scene::shader_dir + scene::default_track_fs;
  std::string model_vs = scene::shader_dir + scene::model_vs;
  std::string model_fs = scene::shader_dir + scene::model_fs;

  
  GLuint track_shader = InitShader(vs.c_str(), fs.c_str());
  GLuint frustum_shader = InitShader(frus_vs.c_str(), frus_fs.c_str());
  GLuint video_shader = InitShader(video_vs.c_str(), video_fs.c_str());
  GLuint default_track_shader = InitShader(default_track_vs.c_str(), default_track_fs.c_str());
  GLuint model_shader = InitShader(model_vs.c_str(), model_fs.c_str());
  
  if (track_shader == -1 || frustum_shader == -1 || video_shader == -1 || default_track_shader == -1 || model_shader == -1) // loading failed
  {
    glClearColor(1.0f, 0.0f, 1.0f, 0.0f); //change clear color if shader can't be compiled
  }
  else
  {
    glClearColor(window::clear_color[0], window::clear_color[1], window::clear_color[2], window::clear_color[3]);
    
    if (scene::track_shader != -1)
    {
      glDeleteProgram(scene::track_shader);
    }
    if (scene::frustum_shader != -1)
    {
      glDeleteProgram(scene::frustum_shader);
    }
    if (scene::video_shader != -1)
    {
      glDeleteProgram(scene::video_shader);
    }
    if (scene::default_track_shader != -1)
    {
      glDeleteProgram(scene::default_track_shader);
    }
    scene::track_shader = track_shader;
    scene::frustum_shader = frustum_shader;
    scene::video_shader = video_shader;
    scene::default_track_shader = default_track_shader;
    scene::model_shader = model_shader;
  }
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
  glViewport(0, 0, width, height);
  scene::aspect_ratio = (float)width / (float)height;
}


//This function gets called when a key is pressed
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  std::cout << "key : " << key << ", " << char(key) << ", scancode: " << scancode << ", action: " << action << ", mods: " << mods << std::endl;
  
  if (action == GLFW_PRESS)
  {
    switch (key)
    {
      case 'r':
      case 'R':
      reload_shader();
      break;
      
      case GLFW_KEY_ESCAPE:
      glfwSetWindowShouldClose(window, GLFW_TRUE);
      break;
      case 'w':
      case 'W':
      scene::movement.y = scene::move_sensitivity;
      break;
      case 's':
      case 'S':
      scene::movement.y = scene::move_sensitivity * -1.0f;
      break;
      case 'a':
      case 'A':
      scene::movement.x = scene::move_sensitivity;
      break;
      case 'd':
      case 'D':
      scene::movement.x = scene::move_sensitivity * -1.0f;
      break;
      case 'q':
      case 'Q':
      scene::movement.z = scene::move_sensitivity * -1.0f;
      break;
      case 'e':
      case 'E':
      scene::movement.z = scene::move_sensitivity;
      break;
    }
  }
  else if (action == GLFW_RELEASE){
    switch (key) {
      case 'w':
      case 'W':
      case 's':
      case 'S':
      scene::movement.y = 0;
      break;
      case 'a':
      case 'A':
      case 'd':
      case 'D':
      scene::movement.x = 0;
      break;
      case 'q':
      case 'Q':
      case 'e':
      case 'E':
      scene::movement.z = 0;
      break;
    }
  }
}

//This function gets called when the mouse moves over the window.
void mouse_cursor(GLFWwindow* window, double x, double y)
{
  //std::cout << "cursor pos: " << x << ", " << y << std::endl;
  scene::cursor_pos = glm::vec2(x, y);
}

//This function gets called when a mouse button is pressed.
void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  if ((button == GLFW_MOUSE_BUTTON_RIGHT) && (action == GLFW_PRESS)) {
    scene::start_drag = scene::cursor_pos;
    scene::max_look = abs(acos(glm::dot(scene::eye_vec, glm::vec3(0, 1, 0)))) - .001f;
    scene::min_look = (abs(acos(glm::dot(scene::eye_vec, glm::vec3(0, -1, 0)))) * -1.0f) +.001f;
    scene::dragging = true;
  }
  if ((button == GLFW_MOUSE_BUTTON_RIGHT) && (action == GLFW_RELEASE)) {
    glm::vec3 new_eye_vec, new_right, new_up;
    updateCameraRotation(&new_eye_vec, &new_right, &new_up);
    scene::eye_vec = new_eye_vec;
    scene::camera_up = new_up;
    scene::dragging = false;
  }
}

//Initialize OpenGL state. This function only gets called once.
void init()
{
  glewInit();
  RegisterDebugCallback();
  
  std::ostringstream oss;
  //Get information about the OpenGL version supported by the graphics driver.	
  oss << "Vendor: "       << glGetString(GL_VENDOR)                    << std::endl;
  oss << "Renderer: "     << glGetString(GL_RENDERER)                  << std::endl;
  oss << "Version: "      << glGetString(GL_VERSION)                   << std::endl;
  oss << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)  << std::endl;
  
  //Output info to console
  std::cout << oss.str();
  
  //Output info to file
  std::fstream info_file("info.txt", std::ios::out | std::ios::trunc);
  info_file << oss.str();
  info_file.close();
  
  //Set the color the screen will be cleared to when glClear is called
  glClearColor(window::clear_color[0], window::clear_color[1], window::clear_color[2], window::clear_color[3]);
  
  glEnable(GL_DEPTH_TEST);
  
  reload_shader();

  std::string meshPath = "C:\\Users\\Jack\\Documents\\School\\CGT-537\\Exercise-6.11\\pos-data\\" + scene::base + ".obj";
  std::string texPath = "C:\\Users\\Jack\\Documents\\School\\CGT-537\\Exercise-6.11\\pos-data\\" + scene::base + ".bmp";
  scene::mesh = LoadMesh(meshPath);
  scene::tex_id = LoadTexture(texPath);

}


// C++ programs start executing in the main() function.
int main(void)
{
  GLFWwindow* window;
  
  // Initialize the library
  if (!glfwInit())
  {
    return -1;
  }
  
  #ifdef _DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  #endif
  
  // Create a windowed mode window and its OpenGL context
  window = glfwCreateWindow(window::size[0], window::size[1], window::title, NULL, NULL);
  if (!window)
  {
    glfwTerminate();
    return -1;
  }
  
  //Register callback functions with glfw. 
  glfwSetKeyCallback(window, keyboard);
  glfwSetCursorPosCallback(window, mouse_cursor);
  glfwSetMouseButtonCallback(window, mouse_button);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  
  // Make the window's context current
  glfwMakeContextCurrent(window);
  
  init();
  
  // New in Lab 2: Init ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  
  ImGui_ImplOpenGL3_Init("#version 150");

  read_in_track_data();
  read_in_video_data();

  // Loop until the user closes the window 
  while (!glfwWindowShouldClose(window))
  {
    idle();
    display(window);
    
    // Poll for and process events 
    glfwPollEvents();
  }
  
  // New in Lab 2: Cleanup ImGui
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  
  glfwTerminate();
  return 0;
}