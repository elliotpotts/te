#version 150 core

in vec2 texcoord;
in vec4 colour;

out vec4 out_colour;

uniform sampler2D tex;

void main() {
    out_colour = texture(tex, texcoord) * colour;
}
