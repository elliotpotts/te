#version 150 core
in vec2 texcoord;
in vec3 normal;
out vec4 out_colour;
uniform sampler2D tex;
void main() {
    out_colour = 0.8*vec4(normal, 1.0) + 0.2*texture(tex, texcoord);
}
