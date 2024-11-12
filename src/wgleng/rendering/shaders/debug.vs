R"(#version 300 es
precision mediump float;

layout (location = 0) in vec3 position;
layout (location = 1) in uint highlightId;

layout(std140) uniform CameraUniform {
    mat4 projxview;
    vec2 nearFarPlane;
};

flat out uint u_highlightId;

void main() {
    u_highlightId = highlightId;
    gl_Position = projxview * vec4(position, 1.0);
}
)"