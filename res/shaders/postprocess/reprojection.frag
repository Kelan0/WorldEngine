#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    uvec2 resolution;
    float taaHistoryFactor;
    bool taaUseFullKernel;
};

layout(set = 0, binding = 1) uniform sampler2D frameTexture;
layout(set = 0, binding = 2) uniform sampler2D velocityTexture;
layout(set = 0, binding = 3) uniform sampler2D depthTexture;
layout(set = 0, binding = 4) uniform sampler2D historyTexture;

vec3 calculateTemporalAntiAliasing(in vec3 frameColour) {
    const vec2 TAA_PLUS_KERNEL[] = vec2[4](vec2(-1,0),vec2(+1,0),vec2(0,-1),vec2(0,+1));
    const vec2 TAA_FULL_KERNEL[] = vec2[8](vec2(-1,-1),vec2(0,-1),vec2(+1,-1),vec2(-1,0),vec2(+1,0),vec2(-1,+1),vec2(0,+1),vec2(+1,+1));

    const vec2 pixelSize = vec2(1.0) / vec2(resolution);
    vec2 closestCoord = fs_texture;
    float closestDepth = texture(depthTexture, closestCoord).r;
    vec2 currentCoord;
    float currentDepth;

    for (uint i = 0; i < (taaUseFullKernel ? 8 : 4); ++i) {
        vec2 offset = taaUseFullKernel ? TAA_FULL_KERNEL[i] : TAA_PLUS_KERNEL[i];
        currentCoord = fs_texture + offset * pixelSize;
        currentDepth = texture(depthTexture, currentCoord).r;
        if (currentDepth < closestDepth) {
            closestDepth = currentDepth;
            closestCoord = currentCoord;
        }
    }

    vec2 velocity = texture(velocityTexture, closestCoord).xy / 100.0;
    vec3 prevFrameColour = texture(historyTexture, fs_texture - velocity).rgb;
    return mix(prevFrameColour, frameColour, taaHistoryFactor);
}

void main() {
    vec3 finalColour = texture(frameTexture, fs_texture).rgb;

    finalColour = calculateTemporalAntiAliasing(finalColour);
    outColor = vec4(finalColour, 1.0);
}