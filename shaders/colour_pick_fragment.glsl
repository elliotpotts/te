#version 150 core
in vec3 pick_colour;
out vec4 out_colour;
void main() {
    out_colour = vec4(pick_colour, 1.0f);
}
