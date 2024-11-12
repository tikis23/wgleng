R"(#version 300 es
precision mediump float;

layout (location = 0) out uvec4 gMaterial;
layout (location = 1) out vec4 gNormal;

flat in uint u_highlightId;

void main() {
    gMaterial = uvec4(0, 0, u_highlightId, 0);
    gNormal = vec4(0., 0., 0., 1.0);
}
)"