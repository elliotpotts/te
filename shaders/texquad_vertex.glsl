#version 150 core

in vec2 POSITION;
in vec2 TEXCOORD_0;
in vec4 COLOUR;

out vec2 texcoord;
out vec4 colour;

uniform mat4 projection;

void main() {
    texcoord = TEXCOORD_0;
    colour = COLOUR;
    gl_Position = projection * vec4(POSITION, 0.0, 1.0);
}
