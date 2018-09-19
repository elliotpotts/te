#version 150 core
in vec3 v_colour;
in vec2 v_texcoord;
out vec4 out_colour;
uniform sampler2D tex;
void main() {
    out_colour = texture(tex, v_texcoord) * vec4(v_colour, 1.0f);
}
