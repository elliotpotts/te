#version 150 core
in vec2 POSITION;
in vec2 TEXCOORD_0;
out vec2 texcoord;
uniform mat4 projection;

void main() {
    texcoord = TEXCOORD_0;
    gl_Position = projection * vec4(POSITION, 0.0, 1.0);
}
