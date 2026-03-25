#version 330
in float v_density;
uniform float rest_density;
out vec4 out_color;
void main()
{
    // vec2  coord   = gl_PointCoord * 2.0 - 1.0;
    // float dist2   = dot(coord, coord);
    // if (dist2 > 1.0) discard;

    float falloff = exp(-dist2 * 3.0);
    float density_ratio = clamp(v_density / rest_density, 0.0, 2.0);
    float alpha = falloff * density_ratio * 0.15;

    out_color = vec4(1.0, 0.0, 0.0, alpha);
}
