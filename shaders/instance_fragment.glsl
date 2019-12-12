#version 150 core
in vec2 texcoord;
in vec3 tint_colour;
out vec4 out_colour;
uniform sampler2D tex;
void main() {
    out_colour = texture(tex, texcoord) + 0.3 * vec4(tint_colour, 1.0);
}
