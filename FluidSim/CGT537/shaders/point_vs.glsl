#version 330
layout(location = 0) in vec3 position;
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
uniform float point_size;
void main()
{
    gl_Position  = projection_matrix * modelview_matrix * vec4(position, 1.0);
    gl_PointSize = point_size;
}
