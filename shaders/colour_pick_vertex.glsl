#version 150 core
in vec3 POSITION;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(POSITION, 1.0);
}
