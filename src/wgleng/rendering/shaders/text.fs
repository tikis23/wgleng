R"(#version 300 es
precision mediump float;

layout (location = 0) out uvec4 gMaterial;
layout (location = 1) out vec4 gNormal;

uniform mediump sampler2DArray tGlyphs;

in vec2 u_uv;
flat in uint u_textureId;
flat in uint u_highlightId;

void main() {
    float glyph = texture(tGlyphs, vec3(u_uv, u_textureId)).r;
    if (glyph < 0.5) discard;
    gMaterial = uvec4(0, 0, u_highlightId, 0);
    gNormal = vec4(0.1, 0.1, 0.1, 1.0);
}
)"