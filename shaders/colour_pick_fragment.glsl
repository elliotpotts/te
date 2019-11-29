#version 150 core
out vec4 out_colour;
uniform vec4 pick_colour;
void main() {
    out_colour = pick_colour;
}
