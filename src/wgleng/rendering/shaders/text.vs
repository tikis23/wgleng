R"(#version 300 es
precision mediump float;

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in uint textureId;
layout (location = 3) in uint highlightId;

layout(std140) uniform TextUniform {
    mat4 projxview;
    mat4 model;
};

out vec2 u_uv;
flat out uint u_textureId;
flat out uint u_highlightId;

void main() {
    u_uv = uv;
    u_textureId = textureId;
    u_highlightId = highlightId;
    gl_Position = projxview * model * vec4(position, 0.0, 1.0);
}  
)"