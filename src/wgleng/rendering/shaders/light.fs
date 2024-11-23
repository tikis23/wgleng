R"(#version 300 es
precision mediump float;
out vec4 gColor;

#define OUTLINE 1
#define SHADOWS 1
#define SHADOW_PCF 1

uniform sampler2D tDepth;
uniform mediump usampler2D tMaterial;
uniform sampler2D tNormal;
uniform mediump sampler2DArray tShadow;

layout(std140) uniform LightingInfoUniform {
    vec3 sunlightDir;
    vec4 sunlightColor; // rgb-color, a-intensity
    vec3 cameraPos;
    vec4 viewportSize_nearFarPlane;
    mat4 invProjView;
};
layout(std140) uniform CSMUniform {
    mat4 lightSpaceMatrices[<<MAX_FRUSTUMS>>];
};
const int cascadeCount = <<CASCADE_COUNT>>;
const float cascadeSplits[<<CASCADE_COUNT>>] = float[](<<CASCADE_SPLITS>>);

struct Material {
    vec4 diffuse;
};
layout(std140) uniform MaterialUniform {
    Material materials[<<MATERIALS_PER_UBO>>];
};
const vec3 highlightColors[<<HIGHLIGHT_COUNT>>] = vec3[](<<HIGHLIGHT_COLORS>>);

// struct PointLight {    
//     vec3 position;
//     vec3 color;
//     vec3 attenuation; // r-constant, g-linear, b-quadratic
// };  
// layout(std140) uniform PointLightInfo {
//     PointLight pointLights[<<POINT_LIGHT_COUNT>>];
// };
in vec2 uv;

float getSunlight(vec3 position, vec3 normal);
float getShadow(vec3 fragPosWorldSpace, vec3 normal, float lDepth);
float getOutline(vec3 normal, float lDepth);
float linearDepth(float depth);
vec3 getWorldPos(vec2 uv, float depth);

void main() {
    // init values
    float depth = texture(tDepth, uv).r;
    float lDepth = linearDepth(depth);
    vec3 position = getWorldPos(uv, depth);

    vec3 normal = texture(tNormal, uv).rgb;

    uvec4 packedMaterial = texture(tMaterial, uv);
    uint materialId = packedMaterial.r << 8 | packedMaterial.g;
    uint highlightId = packedMaterial.b;
    vec3 highlightColor = highlightColors[highlightId];

    vec3 backgroundColor = vec3(0.5, 0.4, 0.3);

    // toon shading
    float lightStrength = getSunlight(position, normal);
    lightStrength = floor(lightStrength * 4.0) / 6.0 + 0.05;
    vec3 color = materials[materialId].diffuse.rgb * sunlightColor.rgb * lightStrength;

    // add shadow
#if SHADOWS
    float shadow = getShadow(position, normal, lDepth);
    shadow = 1.0 - shadow * 0.8;
    color *= shadow;
#endif

    // add outline
#if OUTLINE
    vec3 outlineColor = highlightColor;
    float outline = getOutline(normal, lDepth);
    color = mix(color, outlineColor, outline);
#endif

    float selfDot = dot(normal, normal);
    if (selfDot == 0.0) { // fill background color
        color = backgroundColor;
    } else if (selfDot < 0.1) { // if material is 0, show highlight only
        color = highlightColor;
        highlightColor = vec3(0.0);
    }

    // gamma correction
    float gamma = 2.2;
    color = pow(color, vec3(1.0 / gamma));

    gColor = vec4(color + highlightColor * 0.5, 1.0);

    // show cursor
    vec2 cursorLoc = gl_FragCoord.xy - viewportSize_nearFarPlane.xy * 0.5;
    float cursorDist = dot(cursorLoc, cursorLoc);
    if (cursorDist < 2.0) gColor.xyz = vec3(1.0) - gColor.xyz;
    else if (cursorDist < 3.0) gColor.xyz = vec3(0.0);
}

float linearDepth(float depth) {
    float near = viewportSize_nearFarPlane.z;
    float far = viewportSize_nearFarPlane.w;
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}
vec3 getWorldPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = invProjView * ndc;
    return world.xyz / world.w;
}

float getSunlight(vec3 position, vec3 normal) {
    vec3 lightDir = sunlightDir;
    float strength = sunlightColor.a;

    float ambient = strength * 0.05;
    float diffuse = max(dot(lightDir, normal), 0.0);

    // maby later
    // vec3 viewDir = normalize(cameraPos - position);
    // vec3 reflectDir = reflect(-lightDir, normal);
    // vec3 halfwayDir = normalize(lightDir + viewDir);  
    // float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0); 
    
    return (ambient + diffuse) * strength;
}

float getShadow(vec3 fragPosWorldSpace, vec3 normal, float lDepth) {
    // select cascade layer
    int layer = cascadeCount - 1;
    for (int i = 0; i < cascadeCount; i++) {
        if (lDepth < cascadeSplits[i]) {
            layer = i;
            break;
        }
    }


    // get frag pos in light space
    vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    float fragDepth = projCoords.z;
    if (fragDepth > 0.999) return 0.0;

    float lightDirBias = dot(normal, sunlightDir);
    float bias = max(lightDirBias * -100.0, 0.04 / cascadeSplits[layer]);
#if SHADOW_PCF == 0
    float depth = texture(tShadow, vec3(projCoords.xy, layer)).r;
    return (fragDepth + bias) > depth ? 1.0 : 0.0;
#else
    // PCF
    bias -= cascadeSplits[layer] * 0.00000025;
    float shadow = 0.0;
    vec2 texelSize = 0.8 / vec2(textureSize(tShadow, 0));
    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            float pcfDepth = texture(tShadow, vec3(projCoords.xy + vec2(x, y) * texelSize, layer)).r;
            shadow += (fragDepth + bias) > pcfDepth ? 1.0 : 0.0;      
        }
    }
    shadow /= 9.0;
    return shadow;
#endif
}

float sobel(mat3 vars) {
    mat3 sobelY = mat3( 
        1.0, 0.0, -1.0, 
        2.0, 0.0, -2.0, 
        1.0, 0.0, -1.0 
    );
    mat3 sobelX = mat3( 
        1.0, 2.0, 1.0, 
        0.0, 0.0, 0.0, 
        -1.0, -2.0, -1.0 
    );
    float gx = dot(sobelX[0], vars[0]) + dot(sobelX[1], vars[1]) + dot(sobelX[2], vars[2]); 
    float gy = dot(sobelY[0], vars[0]) + dot(sobelY[1], vars[1]) + dot(sobelY[2], vars[2]);
    return sqrt(gx * gx + gy * gy);
}

float getOutline(vec3 normal, float lDepth) {
    vec2 stepDist = max(0.5, 2.0 - lDepth / 40.0) / viewportSize_nearFarPlane.xy;

    mat3 sobelPositions;
    mat3 sobelNormals;
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            vec2 uvNeighbour = uv + vec2(x, y) * stepDist - stepDist;
            // I dont understand why position works so well here.
            // Discovered by accident. should have been normal buffer.
            float neighbourDepth = texture(tDepth, uvNeighbour).r;
            sobelPositions[x][y] = dot(normal, getWorldPos(uvNeighbour, neighbourDepth));
            vec3 neighbourNormal = texture(tNormal, uvNeighbour).xyz;
            float neighbourSelfDot = dot(neighbourNormal, neighbourNormal);
            if (neighbourSelfDot > 0.0 && neighbourSelfDot < 0.1) return 0.0; // disable outlines on text / debug draw
            sobelNormals[x][y] = dot(normal, neighbourNormal);
        }
    }
    float positionG = sobel(sobelPositions);
    if (positionG < 1.0) positionG = 0.0;
    else positionG = 1.0;

    float normalG = sobel(sobelNormals);
    if (normalG < 1.0) normalG = 0.0;
    else normalG = 1.0;

    return max(positionG, normalG);
}
)"