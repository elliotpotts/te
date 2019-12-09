#version 150 core
in vec3 POSITION;
in vec3 INSTANCE_OFFSET;
in vec2 TEXCOORD_0;
out vec2 texcoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texcoord = TEXCOORD_0;
    gl_Position = projection * view * model * vec4(POSITION + INSTANCE_OFFSET, 1.0);
}
