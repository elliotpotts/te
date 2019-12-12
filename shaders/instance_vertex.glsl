#version 150 core
in vec3 POSITION;
in vec2 INSTANCE_OFFSET;
in vec3 INSTANCE_COLOUR;
in vec2 TEXCOORD_0;
out vec2 texcoord;
out vec3 tint_colour;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texcoord = TEXCOORD_0;
    tint_colour = INSTANCE_COLOUR;
    gl_Position = projection * view * ((model * vec4(POSITION, 1.0)) + vec4(INSTANCE_OFFSET, 0.0, 0.0));
}
