#version 150 core
in vec2 position;
in vec3 colour;
in vec2 texcoord;
out vec3 v_colour;
out vec2 v_texcoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    v_colour = colour;
    v_texcoord = texcoord;
    gl_Position = projection * view * model * vec4(position, 0.0, 1.0);
}
