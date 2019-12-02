#version 150 core
out vec4 out_colour;
uniform vec3 pick_colour;
void main() {
    out_colour = vec4(pick_colour, 1.0f);
}
