#version 150 core
in vec3 NORMAL;
in vec3 POSITION;
in vec2 TEXCOORD_0;
out vec2 texcoord;
out vec3 normal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texcoord = TEXCOORD_0;
    normal = NORMAL;
    gl_Position = projection * view * model * vec4(POSITION, 1.0);
}
