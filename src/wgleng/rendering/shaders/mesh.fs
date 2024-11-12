R"(#version 300 es
precision mediump float;

layout (location = 0) out uvec4 gMaterial;
layout (location = 1) out vec4 gNormal;

in vec3 u_normal;
flat in uint u_highlightId;
flat in uint u_materialId;

void main() {
    gMaterial = uvec4(u_materialId >> 8, u_materialId & 0xFFU, u_highlightId, 0);
    gNormal = vec4(normalize(u_normal), 1.0);
}
)"